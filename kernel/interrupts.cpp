#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sys/arch.h>
#include <sys/atomic.h>
#include <sys/spinlock.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/interrupts.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/once.h>
#include <kernel/process.hpp>
#include <kernel/ref-list.hpp>
#include <kernel/slaballocator.hpp>

static InterruptController * gController = 0;

void Interrupts::RegisterController (InterruptController * controller)
{
    assert(gController == 0);
    gController = controller;
}

enum
{
    NUM_IRQS = 32,
};

/**
 * Stack for IRQ context to execute on.
 */
static uint8_t irq_stack[PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/**
 * Stack for abort-handler context to execute on.
 */
static uint8_t abt_stack[PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/**
 * Dedicated kernel handlers for IRQs. Elements in this list are
 * the 'link' field of the UserInterruptHandlerRecord structure.
 */
static IrqKernelHandlerFunc kernel_irq_handlers[NUM_IRQS];

/**
 * User program's IRQ handlers
 */
typedef RefList<UserInterruptHandler, &UserInterruptHandler::mLink> user_irq_handler_list_t;
static user_irq_handler_list_t user_irq_handlers[NUM_IRQS];

/**
 * Tracks how many times a particular IRQ has been masked.
 */
static unsigned int irq_mask_counts[NUM_IRQS] = { 0 };

/**
 * Lock to protect lists of IRQ handlers
 */
static Spinlock_t irq_handlers_lock = SPINLOCK_INIT;

static void decrement_irq_mask (unsigned int irq_number)
{
    if (AtomicSubAndFetch((int *)&irq_mask_counts[irq_number], 1) == 0) {
        InterruptUnmaskIrq(irq_number);
    }
}

static void increment_irq_mask (unsigned int irq_number)
{
    if (AtomicAddAndFetch((int *)&irq_mask_counts[irq_number], 1) == 1) {
        InterruptMaskIrq(irq_number);
    }
}

static void init_handlers (void * ignored)
{
    #ifdef __arm__
        /**
         * Install stack pointer for IRQ mode
         */
        asm volatile (
            /* Save current execution mode                  */
            "mrs v1, cpsr               \n\t"

            /* Switch to IRQ mode and install stack pointer */
            "cps %[irq_mode_bits]       \n\t"
            "mov sp, %[irq_sp]          \n\t"

            /* Switch to ABT mode and install stack pointer */
            "cps %[abt_mode_bits]       \n\t"
            "mov sp, %[abt_sp]          \n\t"

            /* Restore previous execution mode              */
            "msr cpsr, v1               \n\t"
            :
            : [irq_sp] "r" (&irq_stack[0] + sizeof(irq_stack))
            , [irq_mode_bits] "i" (ARM_PSR_MODE_IRQ_BITS)
            , [abt_sp] "r" (&abt_stack[0] + sizeof(abt_stack))
            , [abt_mode_bits] "i" (ARM_PSR_MODE_ABT_BITS)
            : "memory", "v1"
        );
    #else
        #error
    #endif

    /**
     * Initialize array of in-kernel IRQ handler functions
     */
    memset(kernel_irq_handlers, 0, sizeof(kernel_irq_handlers));

    /**
     * Initialize all registered interrupt controller objects
     */
    assert(gController != 0);
    gController->Init();
}

void InterruptsConfigure ()
{
    static Once_t control = ONCE_INIT;

    Once(&control, init_handlers, NULL);
}

void InterruptAttachKernelHandler (unsigned int irq_number, IrqKernelHandlerFunc f)
{
    assert(irq_number < N_ELEMENTS(kernel_irq_handlers));

    SpinlockLock(&irq_handlers_lock);

    if (irq_number >= 0 && irq_number < N_ELEMENTS(kernel_irq_handlers)) {
        kernel_irq_handlers[irq_number] = f;
    }

    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptAttachUserHandler (
        RefPtr<UserInterruptHandler> handler
        )
{
    assert(handler->mLink.Unlinked());
    assert(handler->mHandlerInfo.mIrqNumber < NUM_IRQS && handler->mHandlerInfo.mIrqNumber >= 0);

    handler->mStateInfo.mMasked = false;

    /* Acquire interrupt protection */
    SpinlockLock(&irq_handlers_lock);

    user_irq_handlers[handler->mHandlerInfo.mIrqNumber].Append(handler);

    /*
    Blip the mask count up and then down again to trigger the interrupt
    controller to unmask it (on the downward stroke) if there are no
    other masks against it right now.
    */
    increment_irq_mask(handler->mHandlerInfo.mIrqNumber);
    decrement_irq_mask(handler->mHandlerInfo.mIrqNumber);

    /* Drop interrupt protection */
    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptDetachUserHandler (
        RefPtr<UserInterruptHandler> record
        )
{
    unsigned int n = record->mHandlerInfo.mIrqNumber;

    SpinlockLock(&irq_handlers_lock);

    user_irq_handlers[n].Remove(record);

    /* Flush out any outstanding per-drive interrupt masks */
    if (record->mStateInfo.mMasked) {
        decrement_irq_mask(n);
    }

    /* Mask the IRQ if there are no other handlers */
    if (user_irq_handlers[n].Empty() && kernel_irq_handlers[n] == NULL) {

        /*
        All other handlers are detached, so there had better not be
        any pending masks.
        */
        assert(irq_mask_counts[n] == 0);

        InterruptMaskIrq(n);
    }

    SpinlockUnlock(&irq_handlers_lock);
}

int InterruptCompleteUserHandler (
    RefPtr<UserInterruptHandler> handler
    )
{
    if (!handler->mStateInfo.mMasked) {
        return ERROR_INVALID;
    }
    else {
        handler->mStateInfo.mMasked = false;
        decrement_irq_mask(handler->mHandlerInfo.mIrqNumber);
        return ERROR_OK;
    }
}

void InterruptHandler ()
{
    /* Figure out which IRQ was raised. */
    int which = gController->GetRaisedIrqNum();
    assert(which >= 0);

    /* Bail out if IRQ out of bounds */
    if (which < 0 || which >= NUM_IRQS) {
        return;
    }

    SpinlockLock(&irq_handlers_lock);

    /* Execute any kernel-installed IRQ handlers */
    if (kernel_irq_handlers[which] != NULL) {
        kernel_irq_handlers[which]();
    }

    /* Execute any user-installed IRQ handlers */
    for (user_irq_handler_list_t::Iterator cursor = user_irq_handlers[which].Begin();
         cursor;
         cursor++) {

        RefPtr<UserInterruptHandler> record = *cursor;

        assert(!record->mStateInfo.mMasked);

        int result = record->mHandlerInfo.mConnection->SendMessageAsyncDuringException((uintptr_t)record->mHandlerInfo.mPulsePayload);
        if (result == ERROR_OK) {
            record->mStateInfo.mMasked = true;
            increment_irq_mask(record->mHandlerInfo.mIrqNumber);
        }
    }

    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptUnmaskIrq (int n)
{
    gController->UnmaskIrq(n);
}

void InterruptMaskIrq (int n)
{
    gController->MaskIrq(n);
}

SyncSlabAllocator<UserInterruptHandler> UserInterruptHandler::sSlab;

UserInterruptHandler::UserInterruptHandler ()
    : mDisposed(false)
{
}

UserInterruptHandler::~UserInterruptHandler ()
{
}

void UserInterruptHandler::Dispose ()
{
    if (!mDisposed)
    {
        mDisposed = true;
        mHandlerInfo.mConnection.Reset();
    }
}

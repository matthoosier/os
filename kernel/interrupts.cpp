#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <sys/arch.h>
#include <sys/atomic.h>
#include <sys/spinlock.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>
#include <kernel/process.hpp>

enum
{
    NUM_IRQS = 32,
};

/**
 * Data structure representing the important registers of the
 * vector interrupt controller.
 */
struct Pl190
{
    /**
     *
     */
    volatile uint32_t * IrqStatus;

    /**
     *
     */
    volatile uint32_t * IntEnable;

    /**
     *
     */
    volatile uint32_t * IntEnClear;
};

/**
 * Stack for IRQ context to execute on.
 */
static uint8_t irq_stack[PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

/**
 * Dedicated kernel handlers for IRQs. Elements in this list are
 * the 'link' field of the UserInterruptHandlerRecord structure.
 */
static IrqKernelHandlerFunc kernel_irq_handlers[NUM_IRQS];

/**
 * User program's IRQ handlers
 */
typedef List<UserInterruptHandlerRecord, &UserInterruptHandlerRecord::link> user_irq_handler_list_t;
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

static struct Pl190 * GetPl190 ();

static void init_handlers (void * ignored)
{
    /**
     * Install stack pointer for IRQ mode
     */
    asm volatile (
        /* Save current execution mode                  */
        "mrs v1, cpsr               \n\t"

        /* Switch to IRQ mode and install stack pointer */
        "cps %[irq_mode_bits]       \n\t"
        "mov sp, %[irq_sp]          \n\t"

        /* Restore previous execution mode              */
        "msr cpsr, v1               \n\t"
        :
        : [irq_sp] "r" (&irq_stack[0] + sizeof(irq_stack))
        , [irq_mode_bits] "i" (ARM_IRQ_MODE_BITS)
        : "memory", "v1"
    );

    /**
     * Initialize array of in-kernel IRQ handler functions
     */
    memset(kernel_irq_handlers, 0, sizeof(kernel_irq_handlers));
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
        struct UserInterruptHandlerRecord * handler
        )
{
    assert(handler->link.Unlinked());
    assert(handler->handler_info.irq_number < NUM_IRQS && handler->handler_info.irq_number >= 0);

    handler->state_info.masked = false;

    /* Acquire interrupt protection */
    SpinlockLock(&irq_handlers_lock);

    user_irq_handlers[handler->handler_info.irq_number].Append(handler);

    /*
    Blip the mask count up and then down again to trigger the interrupt
    controller to unmask it (on the downward stroke) if there are no
    other masks against it right now.
    */
    increment_irq_mask(handler->handler_info.irq_number);
    decrement_irq_mask(handler->handler_info.irq_number);

    /* Drop interrupt protection */
    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptDetachUserHandler (
        struct UserInterruptHandlerRecord * record
        )
{
    unsigned int n = record->handler_info.irq_number;

    SpinlockLock(&irq_handlers_lock);

    user_irq_handlers[n].Remove(record);

    /* Flush out any outstanding per-drive interrupt masks */
    if (record->state_info.masked) {
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
    struct UserInterruptHandlerRecord * handler
    )
{
    if (!handler->state_info.masked) {
        return ERROR_INVALID;
    }
    else {
        handler->state_info.masked = false;
        decrement_irq_mask(handler->handler_info.irq_number);
        return ERROR_OK;
    }
}

void InterruptHandler ()
{
    /* Figure out which IRQ was raised. */
    uint32_t irqs = *GetPl190()->IrqStatus;

    int which = ffs(irqs) - 1;
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

        struct Process            * process;
        struct Connection         * connection;

        struct UserInterruptHandlerRecord * record = *cursor;

        assert(!record->state_info.masked);

        process = ProcessLookup(record->handler_info.pid);

        if (process == NULL) {
            continue;
        }

        connection = ProcessLookupConnection(
                process,
                record->handler_info.coid
                );

        if (connection != NULL) {
            int result = KMessageSendAsync(connection, (uintptr_t)record->handler_info.param);
            if (result == ERROR_OK) {
                record->state_info.masked = true;
                increment_irq_mask(record->handler_info.irq_number);
            }
        }
    }

    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptUnmaskIrq (int n)
{
    *GetPl190()->IntEnable = 1u << n;
}

void InterruptMaskIrq (int n)
{
    *GetPl190()->IntEnClear = 1u << n;
}

void init_pl190 (void * pDevice)
{
    enum
    {
        PL190_BASE_PHYS = 0x10140000,
        PL190_BASE_VIRT = 0xfff10000,
    };

    struct Pl190 * device = (struct Pl190 *)pDevice;

    bool mapped = TranslationTableMapPage(
            MmuGetKernelTranslationTable(),
            PL190_BASE_VIRT,
            PL190_BASE_PHYS,
            PROT_KERNEL
            );
    assert(mapped);

    uint8_t * base = (uint8_t *)PL190_BASE_VIRT;

    device->IrqStatus   = (uint32_t *)(base + 0x000);
    device->IntEnable   = (uint32_t *)(base + 0x010);
    device->IntEnClear  = (uint32_t *)(base + 0x014);
}

static struct Pl190 * GetPl190 ()
{
    static struct Pl190 device;
    static Once_t       control = ONCE_INIT;

    Once(&control, init_pl190, &device);
    return &device;
}

static struct ObjectCache   user_interrupt_handler_cache;
static Spinlock_t           user_interrupt_handler_cache_lock = SPINLOCK_INIT;
static Once_t               user_interrupt_handler_cache_once = ONCE_INIT;

static void user_interrupt_handler_cache_init (void * ignored)
{
    ObjectCacheInit(&user_interrupt_handler_cache, sizeof(struct UserInterruptHandlerRecord));
}

struct UserInterruptHandlerRecord * UserInterruptHandlerRecordAlloc ()
{
    struct UserInterruptHandlerRecord * ret;

    Once(&user_interrupt_handler_cache_once, user_interrupt_handler_cache_init, NULL);

    SpinlockLock(&user_interrupt_handler_cache_lock);
    ret = (struct UserInterruptHandlerRecord *)ObjectCacheAlloc(&user_interrupt_handler_cache);
    SpinlockUnlock(&user_interrupt_handler_cache_lock);

    if (ret) {
        memset(ret, 0, sizeof(*ret));
        ret->link.DynamicInit();
    }

    return ret;
}

void UserInterruptHandlerRecordFree (struct UserInterruptHandlerRecord * record)
{
    Once(&user_interrupt_handler_cache_once, user_interrupt_handler_cache_init, NULL);

    SpinlockLock(&user_interrupt_handler_cache_lock);
    ObjectCacheFree(&user_interrupt_handler_cache, record);
    SpinlockUnlock(&user_interrupt_handler_cache_lock);
}

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <sys/arch.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/interrupt-handler.h>
#include <kernel/message.h>
#include <kernel/mmu.h>
#include <kernel/object-cache.h>
#include <kernel/once.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>

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
static struct list_head user_irq_handlers[NUM_IRQS];

/**
 * Lock to protect lists of IRQ handlers
 */
static Spinlock_t irq_handlers_lock = SPINLOCK_INIT;

static struct Pl190 * GetPl190 ();

void InterruptsConfigure ()
{
    static Once_t control = ONCE_INIT;

    void init (void * ignored)
    {
        unsigned int i;

        /**
         * Install stack pointer for IRQ mode
         */
        asm volatile (
            ".include \"arm-defs.inc\"  \n\t"
            "                           \n\t"

            /* Save current execution mode                  */
            "mrs v1, cpsr               \n\t"

            /* Switch to IRQ mode and install stack pointer */
            "cps #irq                   \n\t"
            "mov sp, %[irq_sp]          \n\t"

            /* Restore previous execution mode              */
            "msr cpsr, v1               \n\t"
            :
            : [irq_sp] "r" (&irq_stack[0] + sizeof(irq_stack))
            : "memory", "v1"
        );

        /**
         * Initialize array of in-kernel IRQ handler functions
         */
        memset(kernel_irq_handlers, 0, sizeof(kernel_irq_handlers));

        /**
         * Initialize array of user-process IRQ handler functions
         */
        memset(user_irq_handlers, 0, sizeof(user_irq_handlers));
        for (i = 0; i < N_ELEMENTS(user_irq_handlers); i++) {
            INIT_LIST_HEAD(&user_irq_handlers[i]);
        }
    }

    Once(&control, init, NULL);
}

void InterruptAttachKernelHandler (int irq_number, IrqKernelHandlerFunc f)
{
    assert((irq_number >= 0) && (irq_number < N_ELEMENTS(kernel_irq_handlers)));

    SpinlockLock(&irq_handlers_lock);

    if (irq_number >= 0 && irq_number < N_ELEMENTS(kernel_irq_handlers)) {
        kernel_irq_handlers[irq_number] = f;
    }

    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptAttachUserHandler (
        int irq_number,
        struct UserInterruptHandlerRecord * handler
        )
{
    assert(list_empty(&handler->link));
    assert(irq_number < NUM_IRQS && irq_number >= 0);

    SpinlockLock(&irq_handlers_lock);

    list_add_tail(&handler->link, &user_irq_handlers[irq_number]);

    SpinlockUnlock(&irq_handlers_lock);
}

void InterruptHandler ()
{
    struct list_head * cursor;

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

    /* Execute any user-install IRQ handlers */
    list_for_each (cursor, &user_irq_handlers[which]) {

        struct TranslationTable * prev_pagetable;
        struct Process * process;
        struct IoNotificationSink * message;

        struct UserInterruptHandlerRecord * record = list_entry(
                cursor,
                struct UserInterruptHandlerRecord,
                link
                );

        process = ProcessLookup(record->pid);

        if (process == NULL) {
            continue;
        }

        prev_pagetable = MmuGetUserTranslationTable();

        MmuSetUserTranslationTable(process->pagetable);

        message = record->func();

        if (message != NULL) {
            struct Process        * receiver_process;
            struct Connection     * connection;
            bool                    ok = true;

            receiver_process = ProcessLookup(message->pid);

            ok = ok && (receiver_process != NULL);

            if (ok) {
                connection = ProcessLookupConnection(
                        receiver_process,
                        message->connection_id
                        );

                ok = ok && (connection != NULL);
            }

            if (ok) {
                KMessageSendAsync(connection, message->payload);
            }
        }

        MmuSetUserTranslationTable(prev_pagetable);
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

static struct Pl190 * GetPl190 ()
{
    static struct Pl190 device;
    static Once_t       control = ONCE_INIT;

    void init (void * param)
    {
        enum
        {
            PL190_BASE_PHYS = 0x10140000,
            PL190_BASE_VIRT = 0xfff10000,
        };

        bool mapped = TranslationTableMapPage(
                MmuGetKernelTranslationTable(),
                PL190_BASE_VIRT,
                PL190_BASE_PHYS,
                PROT_KERNEL
                );
        assert(mapped);

        uint8_t * base = (uint8_t *)PL190_BASE_VIRT;

        device.IrqStatus    = (uint32_t *)(base + 0x000);
        device.IntEnable    = (uint32_t *)(base + 0x010);
        device.IntEnClear   = (uint32_t *)(base + 0x014);
    }

    Once(&control, init, NULL);
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
    ret = ObjectCacheAlloc(&user_interrupt_handler_cache);
    SpinlockUnlock(&user_interrupt_handler_cache_lock);

    if (ret) {
        memset(ret, 0, sizeof(*ret));
        INIT_LIST_HEAD(&ret->link);
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

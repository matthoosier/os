#include <stddef.h>
#include <stdint.h>
#include <strings.h>

#include <sys/arch.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/interrupts.h>
#include <kernel/mmu.h>
#include <kernel/once.h>

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
 * Handlers for IRQs
 */
static IrqHandlerFunc irq_handlers[32];


static struct Pl190 * GetPl190 ();

void InterruptsConfigure ()
{
    static Once_t control = ONCE_INIT;

    void init (void * ignored)
    {
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
    }

    Once(&control, init, NULL);
}

void InterruptInstallIrqHandler (int n, IrqHandlerFunc f)
{
    assert((n >= 0) && (n < N_ELEMENTS(irq_handlers)));

    if (n >= 0 && n < N_ELEMENTS(irq_handlers)) {
        irq_handlers[n] = f;
    }
}

void InterruptHandler ()
{
    uint32_t irqs = *GetPl190()->IrqStatus;

    int which = ffs(irqs) - 1;

    assert(which >= 0);

    if (irq_handlers[which] != NULL) {
        irq_handlers[which]();
    }
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

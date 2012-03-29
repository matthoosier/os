#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/mmu.hpp>
#include <kernel/once.h>
#include <kernel/timer.h>
#include <kernel/thread.hpp>

struct Sp804
{
    volatile        uint32_t * Load;
    volatile const  uint32_t * Value;
    volatile        uint32_t * Control;
    volatile        uint32_t * IntClr;
    volatile const  uint32_t * RIS;
    volatile const  uint32_t * MIS;
    volatile        uint32_t * BgLoad;
};

static void init_sp804 (void * pDevice)
{
    enum
    {
        SP804_BASE_PHYS = 0x101e2000,
        SP804_BASE_VIRT = 0xfff00000,
    };

    struct Sp804 * device = (struct Sp804 *)pDevice;

    bool mapped = TranslationTableMapPage(
            MmuGetKernelTranslationTable(),
            SP804_BASE_VIRT,
            SP804_BASE_PHYS,
            PROT_KERNEL
            );
    assert(mapped);

    uint8_t * base = (uint8_t *)SP804_BASE_VIRT;

    device->Load    = (uint32_t *)  (base + 0x00);
    device->Value   = (uint32_t *)  (base + 0x04);
    device->Control = (uint32_t *)  (base + 0x08);
    device->IntClr  = (uint32_t *)  (base + 0x0c);
    device->RIS     = (uint32_t *)  (base + 0x10);
    device->MIS     = (uint32_t *)  (base + 0x14);
    device->BgLoad  = (uint32_t *)  (base + 0x18);
}

static struct Sp804 * GetSp804 ()
{
    static struct Sp804 device;
    static Once_t       control = ONCE_INIT;

    Once(&control, init_sp804, &device);
    return &device;
}

static void OnTimerInterrupt ()
{
    /* Clear the interrupt */
    *GetSp804()->IntClr = 0;

    /* Boot the current task */
    ThreadSetNeedResched();
}

void TimerStartPeriodic (unsigned int period_ms)
{
    enum
    {
        TIMER0_IRQ = 4,
    };

    enum
    {
        /**
         * Number of timer cycles requires to elapse one second
         * on Versatile board.
         */
        ONE_SECOND = 1000000,
    };

    struct Sp804 * sp804 = GetSp804();
    uint32_t period_cycles = (ONE_SECOND * period_ms) / 1000;

    /*
    Now install hooks for handling the timer interrupt
    */
    InterruptAttachKernelHandler(TIMER0_IRQ, OnTimerInterrupt);
    InterruptUnmaskIrq(TIMER0_IRQ);

    /*
    Program the timer
    */

    *sp804->BgLoad = period_cycles;
    *sp804->Load = period_cycles;

    uint32_t control = *sp804->Control;
    
    /* First enable interrupts */
    control |= 0b00100000; /* timer interrupt enabled */
    *sp804->Control = control;

    control |= 0b00000010; /* 32-bit expressiveness */
    *sp804->Control = control;

    //control |= 0b00000001; /* oneshot */
    //*sp804->Control = control;

    control |= 0b01000000; /* periodic */
    *sp804->Control = control;

    control |= 0b10000000; /* timer enabled */
    *sp804->Control = control;
}


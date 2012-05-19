#include <stdint.h>

#include <kernel/interrupt-handler.hpp>
#include <kernel/mmu.hpp>
#include <kernel/timer.hpp>
#include <kernel/thread.hpp>

class Sp804 : public TimerDevice
{
public:
    Sp804 ();
    virtual ~Sp804 ();

    virtual void Init ();
    virtual void ClearInterrupt ();
    virtual void StartPeriodic (unsigned int period_ms);

private:
    volatile        uint32_t * Load;
    volatile const  uint32_t * Value;
    volatile        uint32_t * Control;
    volatile        uint32_t * IntClr;
    volatile const  uint32_t * RIS;
    volatile const  uint32_t * MIS;
    volatile        uint32_t * BgLoad;
};

static void OnTimerInterrupt (void);

Sp804::Sp804 ()
{
    Timer::RegisterDevice(this);
}

Sp804::~Sp804 ()
{
}

void Sp804::ClearInterrupt ()
{
    *this->IntClr = 0;
}

void Sp804::Init ()
{
    enum
    {
        SP804_BASE_PHYS = 0x101e2000,
        SP804_BASE_VIRT = 0xfff00000,
    };

    bool mapped = TranslationTable::GetKernel()->MapPage(
            SP804_BASE_VIRT,
            SP804_BASE_PHYS,
            PROT_KERNEL
            );
    assert(mapped);

    uint8_t * base = (uint8_t *)SP804_BASE_VIRT;

    this->Load    = (uint32_t *)  (base + 0x00);
    this->Value   = (uint32_t *)  (base + 0x04);
    this->Control = (uint32_t *)  (base + 0x08);
    this->IntClr  = (uint32_t *)  (base + 0x0c);
    this->RIS     = (uint32_t *)  (base + 0x10);
    this->MIS     = (uint32_t *)  (base + 0x14);
    this->BgLoad  = (uint32_t *)  (base + 0x18);
}

void Sp804::StartPeriodic (unsigned int period_ms)
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

    uint32_t period_cycles = (ONE_SECOND * period_ms) / 1000;

    /*
    Now install hooks for handling the timer interrupt
    */
    InterruptAttachKernelHandler(TIMER0_IRQ, OnTimerInterrupt);
    InterruptUnmaskIrq(TIMER0_IRQ);

    /*
    Program the timer
    */

    *this->BgLoad = period_cycles;
    *this->Load = period_cycles;

    uint32_t control = *this->Control;
    
    /* First enable interrupts */
    control |= 0b00100000; /* timer interrupt enabled */
    *this->Control = control;

    control |= 0b00000010; /* 32-bit expressiveness */
    *this->Control = control;

    //control |= 0b00000001; /* oneshot */
    //*this->Control = control;

    control |= 0b01000000; /* periodic */
    *this->Control = control;

    control |= 0b10000000; /* timer enabled */
    *this->Control = control;
}

static Sp804 instance;

static void OnTimerInterrupt ()
{
    /* Clear the interrupt */
    instance.ClearInterrupt();

    /* Boot the current task */
    Timer::ReportPeriodicInterrupt();
}

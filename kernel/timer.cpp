#include <kernel/once.h>
#include <kernel/timer.hpp>
#include <kernel/thread.hpp>

static TimerDevice * timer = 0;

void Timer::RegisterDevice (TimerDevice * device)
{
    timer = device;
}

static Once_t timer_init_once = ONCE_INIT;

static void init_timer (void * ignored)
{
    assert(timer != 0);
    timer->Init();
}

void Timer::StartPeriodic (unsigned int period_ms)
{
    Once(&timer_init_once, init_timer, NULL);
    timer->StartPeriodic(period_ms);
}

void Timer::ReportPeriodicInterrupt ()
{
    Thread::SetNeedResched();
}

#ifndef __KERNEL_TIMER_HPP__
#define __KERNEL_TIMER_HPP__

/**
 * \brief   Base class on in-kernel programmable timer driver
 *
 * \class TimerDevice timer.hpp kernel/timer.hpp
 */
class TimerDevice
{
public:
    virtual ~TimerDevice () {};

    virtual void Init () = 0;
    virtual void ClearInterrupt () = 0;
    virtual void StartPeriodic (unsigned int period_ms) = 0;
};

/**
 * \brief   Factory for doing programmable timer operations
 *
 * \class Timer timer.hpp kernel/timer.hpp
 */
class Timer
{
public:
    static void RegisterDevice (TimerDevice * device);
    static void StartPeriodic (unsigned int period_ms);
    static void ReportPeriodicInterrupt ();
};

#endif /* __KERNEL_TIMER_HPP__ */

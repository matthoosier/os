class TimerDevice
{
public:
    virtual ~TimerDevice () {};

    virtual void Init () = 0;
    virtual void ClearInterrupt () = 0;
    virtual void StartPeriodic (unsigned int period_ms) = 0;
};

class Timer
{
public:
    static void RegisterDevice (TimerDevice * device);
    static void StartPeriodic (unsigned int period_ms);
    static void ReportPeriodicInterrupt ();
};

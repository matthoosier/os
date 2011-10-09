#include "once.h"
#include "spinlock.h"

void Once (Once_t * control, OnceFunc func, void * param)
{
    if (!control->done)
    {
        SpinlockLock(&control->lock);

        if (!control->done)
        {
            control->done = true;
            func(param);
        }

        SpinlockUnlock(&control->lock);
    }
}

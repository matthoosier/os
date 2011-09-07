#include "once.h"
#include "spinlock.h"

void once (once_t * control, once_func func, void * param)
{
    if (!control->done)
    {
        spinlock_lock(&control->lock);

        if (!control->done)
        {
            control->done = true;
            func(param);
        }

        spinlock_unlock(&control->lock);
    }
}

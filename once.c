#include "once.h"

void once (once_t * control, once_func func, void * param)
{
    if (*control == ONCE_INIT)
    {
        *control = !ONCE_INIT;
        func(param);
    }
}

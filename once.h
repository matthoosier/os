#ifndef __ONCE_H__
#define __ONCE_H__

#include <stdbool.h>
#include "spinlock.h"

typedef struct
{
    spinlock_t  lock;
    bool        done;
} once_t;

#define ONCE_INIT { SPINLOCK_INIT, false }

typedef void (*once_func) (void * param);

extern void once (once_t * control, once_func func, void * param);

#endif /* __ONCE_H__ */

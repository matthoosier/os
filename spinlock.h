#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "decls.h"
#include "atomic.h"

BEGIN_DECLS

typedef struct
{
    uint32_t lockval;
} Spinlock_t;

#define SPINLOCK_LOCKVAL_UNLOCKED   0
#define SPINLOCK_LOCKVAL_LOCKED     1

#define SPINLOCK_INIT { SPINLOCK_LOCKVAL_UNLOCKED }

static inline void SpinlockLock (Spinlock_t * lock)
{
    while (!AtomicCompareAndExchange(&lock->lockval, SPINLOCK_LOCKVAL_UNLOCKED, SPINLOCK_LOCKVAL_LOCKED))
    {
    }
}

static inline void SpinlockUnlock (Spinlock_t * lock)
{
    while (!AtomicCompareAndExchange(&lock->lockval, SPINLOCK_LOCKVAL_LOCKED, SPINLOCK_LOCKVAL_UNLOCKED))
    {
    }
}

END_DECLS

#endif /* __SPINLOCK_H__ */

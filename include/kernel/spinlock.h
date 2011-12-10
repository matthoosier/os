#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <sys/decls.h>

#include <kernel/atomic.h>
#include <kernel/interrupts.h>

BEGIN_DECLS

typedef struct
{
    uint32_t    lockval;
    IrqSave_t   irq_saved_state;
} Spinlock_t;

#define SPINLOCK_LOCKVAL_UNLOCKED   0
#define SPINLOCK_LOCKVAL_LOCKED     1

#define SPINLOCK_INIT { .lockval = SPINLOCK_LOCKVAL_UNLOCKED }

static inline void SpinlockInit (Spinlock_t * lock)
{
    /* Initially unlocked */
    lock->lockval = SPINLOCK_LOCKVAL_UNLOCKED;

    /* Don't care about saved IRQ status. It's overwritten on first use anyway. */
    lock->irq_saved_state = lock->irq_saved_state;
}

static inline void SpinlockLock (Spinlock_t * lock)
{
    /* On UP systems, this line alone does all the real work. */
    lock->irq_saved_state = InterruptsDisable();

    while (!AtomicCompareAndExchange(&lock->lockval, SPINLOCK_LOCKVAL_UNLOCKED, SPINLOCK_LOCKVAL_LOCKED))
    {
    }
}

static inline void SpinlockUnlock (Spinlock_t * lock)
{
    while (!AtomicCompareAndExchange(&lock->lockval, SPINLOCK_LOCKVAL_LOCKED, SPINLOCK_LOCKVAL_UNLOCKED))
    {
    }

    InterruptsRestore(lock->irq_saved_state);
}

END_DECLS

#endif /* __SPINLOCK_H__ */

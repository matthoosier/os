#ifndef __MUOS_ATOMIC_H__
#define __MUOS_ATOMIC_H__

/*! \file */

#include <stdbool.h>
#include <stdint.h>

#include <muos/decls.h>

/**
 * @class Atomics atomic.h muos/atomic.h
 */

BEGIN_DECLS

/**
 * @brief   If the value addressed by <tt>ptr</tt> if still equal
 *          to <tt>oldval</tt>, replace the value at <tt>ptr</tt>
 *          with <tt>newval</tt>.
 *
 * @return  <tt>true</tt> if the exchange happened, or <tt>false</tt>
 *          if the value at <tt>ptr</tt> was out-of-date
 *
 * @memberof Atomics
 */
static inline bool AtomicCompareAndExchange (
        uint32_t *ptr,
        uint32_t oldval,
        uint32_t newval
        )
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

/**
 * @brief   Add <tt>amount</tt> to the value at <tt>ptr</tt>, and
 *          return the resulting value
 *
 * @memberof Atomics
 */
static inline int AtomicAddAndFetch (
        int   * ptr,
        int     amount
        )
{
    return __sync_add_and_fetch(ptr, amount);
}

/**
 * @brief   Subtract <tt>amount</tt> from the value at <tt>ptr</tt>, and
 *          return the resulting value
 *
 * @memberof Atomics
 */
static inline int AtomicSubAndFetch (
        int   * ptr,
        int     amount
        )
{
    return __sync_sub_and_fetch(ptr, amount);
}

/**
 * @brief   Insert a directive to stop the compiler from reordering
 *          memory access across the barrier
 *
 * @memberof Atomics
 */
static inline void AtomicCompilerMemoryBarrier (void)
{
    asm volatile("" : : : "memory");
}

END_DECLS

#endif /* __MUOS_ATOMIC_H__ */

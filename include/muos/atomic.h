#ifndef __MUOS_ATOMIC_H__
#define __MUOS_ATOMIC_H__

#include <stdbool.h>
#include <stdint.h>

#include <muos/decls.h>

BEGIN_DECLS

static inline bool AtomicCompareAndExchange (
        uint32_t *ptr,
        uint32_t oldval,
        uint32_t newval
        )
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

static inline int AtomicAddAndFetch (
        int   * ptr,
        int     amount
        )
{
    return __sync_add_and_fetch(ptr, amount);
}

static inline int AtomicSubAndFetch (
        int   * ptr,
        int     amount
        )
{
    return __sync_sub_and_fetch(ptr, amount);
}

static inline void AtomicCompilerMemoryBarrier (void)
{
    asm volatile("" : : : "memory");
}

END_DECLS

#endif /* __MUOS_ATOMIC_H__ */

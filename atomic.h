#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include <stdbool.h>
#include <stdint.h>

static inline bool atomic_compare_and_exchange (
        uint32_t *ptr,
        uint32_t oldval,
        uint32_t newval
        )
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

#endif /* __ATOMIC_H__ */

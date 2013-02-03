#include <muos/arch.h>
#include <muos/array.h>
#include <muos/spinlock.h>

#include <kernel/kmalloc.h>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>

enum
{
    /**
     * kmalloc() will be able to service objects up to 2**(PAGE_SHIFT -1)
     * bytes in size. For larger things, just use page allocations directly.
     */
    NUM_BUCKETS = PAGE_SHIFT - 1,
};

/**
 * allocators[i] serves out objects 2**i bytes long
 */
static struct ObjectCache   allocators[NUM_BUCKETS];

static Once_t               allocators_once = ONCE_INIT;
static Spinlock_t           allocators_lock = SPINLOCK_INIT;

static void init (void * ignored)
{
    unsigned int i;

    for (i = 0; i < N_ELEMENTS(allocators); i++) {
        ObjectCacheInit(&allocators[i], 1 << i);
    }
}

__attribute__((optimize(2)))
static inline int bucket_from_size (size_t size)
{
    int bucket_index;

    /* Find highest bit set in @size */
    int leading_zeroes = __builtin_clz((unsigned int)size);
    int highest_bit = ((sizeof(size) * 8) - 1) - leading_zeroes;

    /* Test whether size is exactly equal to 2**highest_bit */
    if ((size & (1 << highest_bit)) != 0) {
        /* It's not. So round up to next largest bucket. */
        bucket_index = highest_bit + 1;
    }
    else {
        /* It is. Highest-bit index is the correct bucket index. */
        bucket_index = highest_bit;
    }

    return bucket_index;
}

void * kmalloc (size_t size)
{
    int bucket;
    void * ret;

    Once(&allocators_once, init, NULL);

    if (size < 0) {
        return NULL;
    }

    bucket = bucket_from_size(size);

    if (bucket >= 0 && (unsigned int)bucket < N_ELEMENTS(allocators)) {
        SpinlockLock(&allocators_lock);
        ret = ObjectCacheAlloc(&allocators[bucket]);
        SpinlockUnlock(&allocators_lock);
    }
    else {
        ret = NULL;
    }

    return ret;
}

void kfree (void * ptr, size_t size)
{
    int bucket;

    Once(&allocators_once, init, NULL);

    if (size < 0) {
        return;
    }

    bucket = bucket_from_size(size);

    if (bucket >= 0 && (unsigned int)bucket < N_ELEMENTS(allocators)) {
        SpinlockLock(&allocators_lock);
        ObjectCacheFree(&allocators[bucket], ptr);
        SpinlockUnlock(&allocators_lock);
    }
}

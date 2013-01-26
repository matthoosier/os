#ifndef __OBJECT_CACHE_INTERNAL_H__
#define __OBJECT_CACHE_INTERNAL_H__

#include <muos/arch.h>
#include <muos/compiler.h>
#include <muos/decls.h>

#include <kernel/object-cache.hpp>

BEGIN_DECLS

/* One-eighth of a page (right-shift by three is same as divide by 8) */
#define MAX_SMALL_OBJECT_SIZE (PAGE_SIZE >> 3)

struct Slab;

struct ObjectCacheOps
{
    void (*StaticInit) (void);
    void (*Constructor) (struct ObjectCache * cache);
    void (*Destructor) (struct ObjectCache * cache);
    struct Slab * (*TryAllocateSlab) (struct ObjectCache * cache);
    void (*TryFreeSlab) (struct ObjectCache * cache, struct Slab * slab);
    struct Slab * (*MapBufctlToSlab) (struct ObjectCache * cache, void * bufctl_addr);
};

extern const struct ObjectCacheOps small_objects_ops;
extern const struct ObjectCacheOps large_objects_ops;

/* Enforce that the bufctl's don't grow larger than 1/8 of a page */
COMPILER_ASSERT(sizeof(struct Bufctl) << 3 <= PAGE_SIZE);

extern void InitSlab (struct Slab * slab);
extern void InitBufctl (struct Bufctl * bufctl);

END_DECLS

#endif /* __OBJECT_CACHE_INTERNAL_H__ */

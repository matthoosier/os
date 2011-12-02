#ifndef __OBJECT_CACHE_INTERNAL_H__
#define __OBJECT_CACHE_INTERNAL_H__

#include <sys/decls.h>

#include <kernel/arch.h>
#include <kernel/compiler.h>
#include <kernel/list.h>
#include <kernel/object-cache.h>

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

struct Bufctl
{
    /* Links in the free-list chain */
    struct list_head freelist_link;

    /* The object */
    void * buf;
};

/* Enforce that the bufctl's don't grow larger than 1/8 of a page */
COMPILER_ASSERT(sizeof(struct Bufctl) << 3 <= PAGE_SIZE);

struct Slab
{
    /* Descriptor for the raw virtual memory used by this slab. */
    struct Page * page;

    /* How many objects from this slab are still held by users. */
    unsigned int refcount;

    /* All objects ready to be supplied to users */
    struct list_head freelist_head;

    /* Linkage in the controlling object cache */
    struct list_head cache_link;
};

extern void InitSlab (struct Slab * slab);
extern void InitBufctl (struct Bufctl * bufctl);

END_DECLS

#endif /* __OBJECT_CACHE_INTERNAL_H__ */

#ifndef __OBJECT_CACHE_INTERNAL_H__
#define __OBJECT_CACHE_INTERNAL_H__

#include "arch.h"
#include "compiler.h"
#include "decls.h"
#include "list.h"
#include "object-cache.h"

BEGIN_DECLS

/* One-eighth of a page (right-shift by three is same as divide by 8) */
#define MAX_SMALL_OBJECT_SIZE (PAGE_SIZE >> 3)

struct slab;

struct object_cache_ops
{
    void (*static_init) (void);
    void (*constructor) (struct object_cache * cache);
    void (*destructor) (struct object_cache * cache);
    struct slab * (*try_allocate_slab) (struct object_cache * cache);
    void (*try_free_slab) (struct object_cache * cache, struct slab * slab);
    struct slab * (*map_bufctl_to_slab) (struct object_cache * cache, void * bufctl_addr);
};

extern const struct object_cache_ops small_objects_ops;
extern const struct object_cache_ops large_objects_ops;

struct bufctl
{
    /* Links in the free-list chain */
    struct list_head freelist_link;

    /* The object */
    void * buf;
};

/* Enforce that the bufctl's don't grow larger than 1/8 of a page */
COMPILER_ASSERT(sizeof(struct bufctl) << 3 <= PAGE_SIZE);

struct slab
{
    /* Descriptor for the raw virtual memory used by this slab. */
    struct page * page;

    /* How many objects from this slab are still held by users. */
    unsigned int refcount;

    /* All objects ready to be supplied to users */
    struct list_head freelist_head;

    /* Linkage in the controlling object cache */
    struct list_head cache_link;
};

extern void init_slab (struct slab * slab);
extern void init_bufctl (struct bufctl * bufctl);

END_DECLS

#endif /* __OBJECT_CACHE_INTERNAL_H__ */

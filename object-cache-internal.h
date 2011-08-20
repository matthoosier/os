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

/*
Used by large-object caches to get bufctl's not hosted inside the slab
storage. Prevents terrible space waste for large (>= PAGE_SIZE / 2)
objects.
*/
extern struct object_cache butctls_cache;

struct bufctl
{
    /* Links in the free-list chain */
    struct list_head freelist_link;

    /* The object */
    void * buf;
};

/* Enforce that the bufctl's don't grow larger than 1/8 of a page */
COMPILER_ASSERT(3 * 8 <= PAGE_SIZE);

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

void init_slab (struct slab * slab);

void * small_object_cache_alloc (struct object_cache * cache);
void small_object_cache_free (struct object_cache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_INTERNAL_H__ */

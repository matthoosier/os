#ifndef __OBJECT_CACHE_H__
#define __OBJECT_CACHE_H__

#include <stdlib.h>

#include <sys/decls.h>

#include <kernel/list.hpp>

BEGIN_DECLS

struct TreeMap;
struct ObjectCacheOps;

struct Bufctl
{
    /* Links in the free-list chain */
    ListElement freelist_link;

    /* The object */
    void * buf;
};

struct Slab
{
    /* Descriptor for the raw virtual memory used by this slab. */
    struct Page * page;

    /* How many objects from this slab are still held by users. */
    unsigned int refcount;

    /* All objects ready to be supplied to users */
    List<Bufctl, &Bufctl::freelist_link> freelist_head;

    /* Linkage in the controlling object cache */
    ListElement cache_link;
};

struct ObjectCache
{
    size_t element_size;

    /* List of slab's */
    List<Slab, &Slab::cache_link> slab_head;

    /* Used for large-object case */
    struct TreeMap * bufctl_to_slab_map;

    const struct ObjectCacheOps * ops;
};

extern void ObjectCacheInit (struct ObjectCache * cache, size_t element_size);

extern void * ObjectCacheAlloc (struct ObjectCache * cache);
extern void ObjectCacheFree (struct ObjectCache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_H__ */


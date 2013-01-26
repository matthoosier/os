#ifndef __OBJECT_CACHE_H__
#define __OBJECT_CACHE_H__

#include <stdlib.h>

#include <muos/decls.h>

#include <kernel/list.hpp>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

BEGIN_DECLS

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
    Page * page;

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

    typedef TreeMap<void *, struct Slab *> BufctlToSlabMap_t;

    /* Used for large-object case */
    BufctlToSlabMap_t * bufctl_to_slab_map;

    const struct ObjectCacheOps * ops;
};

extern void ObjectCacheInit (struct ObjectCache * cache, size_t element_size);

extern void * ObjectCacheAlloc (struct ObjectCache * cache);
extern void ObjectCacheFree (struct ObjectCache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_H__ */


#ifndef __OBJECT_CACHE_H__
#define __OBJECT_CACHE_H__

#include <stdlib.h>
#include "decls.h"
#include "list.h"

BEGIN_DECLS

struct TreeMap;
struct ObjectCacheOps;

struct ObjectCache
{
    size_t element_size;

    /* List of slab's */
    struct list_head slab_head;

    /* Used for large-object case */
    struct TreeMap * bufctl_to_slab_map;

    const struct ObjectCacheOps * ops;
};

extern void ObjectCacheInit (struct ObjectCache * cache, size_t element_size);

extern void * ObjectCacheAlloc (struct ObjectCache * cache);
extern void ObjectCacheFree (struct ObjectCache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_H__ */


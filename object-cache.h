#ifndef __OBJECT_CACHE_H__
#define __OBJECT_CACHE_H__

#include <stdlib.h>
#include "decls.h"
#include "list.h"

BEGIN_DECLS

struct tree_map;
struct object_cache_ops;

struct object_cache
{
    size_t element_size;

    /* List of slab's */
    struct list_head slab_head;

    /* Used for large-object case */
    struct tree_map * bufctl_to_slab_map;

    const struct object_cache_ops * ops;
};

extern void object_cache_init (struct object_cache * cache, size_t element_size);

extern void * object_cache_alloc (struct object_cache * cache);
extern void object_cache_free (struct object_cache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_H__ */


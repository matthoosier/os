#ifndef __OBJECT_CACHE_H__
#define __OBJECT_CACHE_H__

#include <stdlib.h>
#include "decls.h"
#include "list.h"

BEGIN_DECLS

struct object_cache
{
    size_t element_size;

    /* List of slab's */
    struct list_head slab_head;
};

extern void object_cache_init (struct object_cache * cache, size_t element_size);

extern void * object_cache_alloc (struct object_cache * cache);
extern void object_cache_free (struct object_cache * cache, void * element);

END_DECLS

#endif /* __OBJECT_CACHE_H__ */


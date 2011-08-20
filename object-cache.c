#include "object-cache.h"
#include "object-cache-internal.h"

void object_cache_init (struct object_cache * cache, size_t element_size)
{
    /*
    Ensure that each carved slab element will be large enough to hold the
    larger of the user's object type AND the internal free-list node.
    */
    if (sizeof(struct bufctl) > element_size) {
        element_size = sizeof(struct bufctl);
    }

    cache->element_size = element_size;
    INIT_LIST_HEAD(&cache->slab_head);
}

void * object_cache_alloc (struct object_cache * cache)
{
    if (cache->element_size > MAX_SMALL_OBJECT_SIZE) {
        return NULL;
    }
    else {
        return small_object_cache_alloc(cache);
    }
}

void object_cache_free (struct object_cache * cache, void * element)
{
    if (cache->element_size > MAX_SMALL_OBJECT_SIZE) {
        return;
    }
    else {
        small_object_cache_free(cache, element);
    }
}

void init_slab (struct slab * slab)
{
    slab->page = NULL;
    slab->refcount = 0;
    INIT_LIST_HEAD(&slab->freelist_head);
    INIT_LIST_HEAD(&slab->cache_link);
}

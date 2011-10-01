#include "assert.h"
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

    if (cache->element_size >= MAX_SMALL_OBJECT_SIZE) {
        cache->ops = &large_objects_ops;
    }
    else {
        cache->ops = &small_objects_ops;
    }

    cache->ops->static_init();
    cache->ops->constructor(cache);
}

void * object_cache_alloc (struct object_cache * cache)
{
    struct slab * slab_cursor;
    struct slab * new_slab;
    struct bufctl * bufctl;

    list_for_each_entry (slab_cursor, &cache->slab_head, cache_link) {
        if (!list_empty(&slab_cursor->freelist_head)) {
            bufctl = list_first_entry(&slab_cursor->freelist_head, struct bufctl, freelist_link);
            slab_cursor->refcount++;
            list_del_init(&bufctl->freelist_link);
            return bufctl;
        }
    }

    /* If control gets here, we're out of objects. Try to make more. */
    if ((new_slab = cache->ops->try_allocate_slab(cache)) == NULL) {
        return NULL;
    }

    list_add(&new_slab->cache_link, &cache->slab_head);

    if (!list_empty(&new_slab->freelist_head)) {
        bufctl = list_first_entry(&new_slab->freelist_head, struct bufctl, freelist_link);
        new_slab->refcount++;
        list_del_init(&bufctl->freelist_link);
        return bufctl;
    }
    else {
        return NULL;
    }
}

void object_cache_free (struct object_cache * cache, void * element)
{
    struct bufctl * reclaimed_bufctl;
    struct slab *   slab;

    /*
    Take back over the payload of the object as our internal
    free-list representation.
    */
    reclaimed_bufctl = (struct bufctl *)element;
    init_bufctl(reclaimed_bufctl);

    slab = cache->ops->map_bufctl_to_slab(cache, reclaimed_bufctl);
    assert(slab != NULL);

    /*
    Stick on head of freelist to promote reuse of objects from slab
    that already had some allocations made.
    */
    list_add(&reclaimed_bufctl->freelist_link, &slab->freelist_head);
    slab->refcount--;
    cache->ops->try_free_slab(cache, slab);
}

void init_slab (struct slab * slab)
{
    slab->page = NULL;
    slab->refcount = 0;
    INIT_LIST_HEAD(&slab->freelist_head);
    INIT_LIST_HEAD(&slab->cache_link);
}

void init_bufctl (struct bufctl * bufctl)
{
    /* When not allocated to user, the object itself is freelist element */
    bufctl->buf = (void *)bufctl;

    INIT_LIST_HEAD(&bufctl->freelist_link);
}


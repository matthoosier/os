#include "assert.h"
#include "object-cache.h"
#include "object-cache-internal.h"

void ObjectCacheInit (struct ObjectCache * cache, size_t element_size)
{
    /*
    Ensure that each carved slab element will be large enough to hold the
    larger of the user's object type AND the internal free-list node.
    */
    if (sizeof(struct Bufctl) > element_size) {
        element_size = sizeof(struct Bufctl);
    }

    cache->element_size = element_size;
    INIT_LIST_HEAD(&cache->slab_head);

    if (cache->element_size >= MAX_SMALL_OBJECT_SIZE) {
        cache->ops = &large_objects_ops;
    }
    else {
        cache->ops = &small_objects_ops;
    }

    cache->ops->StaticInit();
    cache->ops->Constructor(cache);
}

void * ObjectCacheAlloc (struct ObjectCache * cache)
{
    struct Slab * slab_cursor;
    struct Slab * new_slab;
    struct Bufctl * bufctl;

    list_for_each_entry (slab_cursor, &cache->slab_head, cache_link) {
        if (!list_empty(&slab_cursor->freelist_head)) {
            bufctl = list_first_entry(&slab_cursor->freelist_head, struct Bufctl, freelist_link);
            slab_cursor->refcount++;
            list_del_init(&bufctl->freelist_link);
            return bufctl;
        }
    }

    /* If control gets here, we're out of objects. Try to make more. */
    if ((new_slab = cache->ops->TryAllocateSlab(cache)) == NULL) {
        return NULL;
    }

    list_add(&new_slab->cache_link, &cache->slab_head);

    if (!list_empty(&new_slab->freelist_head)) {
        bufctl = list_first_entry(&new_slab->freelist_head, struct Bufctl, freelist_link);
        new_slab->refcount++;
        list_del_init(&bufctl->freelist_link);
        return bufctl;
    }
    else {
        return NULL;
    }
}

void ObjectCacheFree (struct ObjectCache * cache, void * element)
{
    struct Bufctl * reclaimed_bufctl;
    struct Slab *   slab;

    /*
    Take back over the payload of the object as our internal
    free-list representation.
    */
    reclaimed_bufctl = (struct Bufctl *)element;
    InitBufctl(reclaimed_bufctl);

    slab = cache->ops->MapBufctlToSlab(cache, reclaimed_bufctl);
    assert(slab != NULL);

    /*
    Stick on head of freelist to promote reuse of objects from slab
    that already had some allocations made.
    */
    list_add(&reclaimed_bufctl->freelist_link, &slab->freelist_head);
    slab->refcount--;
    cache->ops->TryFreeSlab(cache, slab);
}

void InitSlab (struct Slab * slab)
{
    slab->page = NULL;
    slab->refcount = 0;
    INIT_LIST_HEAD(&slab->freelist_head);
    INIT_LIST_HEAD(&slab->cache_link);
}

void InitBufctl (struct Bufctl * bufctl)
{
    /* When not allocated to user, the object itself is freelist element */
    bufctl->buf = (void *)bufctl;

    INIT_LIST_HEAD(&bufctl->freelist_link);
}


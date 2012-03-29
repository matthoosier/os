#include <kernel/assert.h>
#include <kernel/object-cache.hpp>

#include "object-cache-internal.hpp"

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
    cache->slab_head.DynamicInit();

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
    typedef List<Slab, &Slab::cache_link> list_t;

    struct Slab * new_slab;
    struct Bufctl * bufctl;

    for (list_t::Iterator slab_cursor = cache->slab_head.Begin(); slab_cursor; slab_cursor++)
    {
        if (!slab_cursor->freelist_head.Empty()) {
            bufctl = slab_cursor->freelist_head.PopFirst();
            slab_cursor->refcount++;

            return bufctl;
        }
    }

    /* If control gets here, we're out of objects. Try to make more. */
    if ((new_slab = cache->ops->TryAllocateSlab(cache)) == NULL) {
        return NULL;
    }

    cache->slab_head.Prepend(new_slab);

    if (!new_slab->freelist_head.Empty()) {
        bufctl = new_slab->freelist_head.PopFirst();
        new_slab->refcount++;
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
    slab->freelist_head.Prepend(reclaimed_bufctl);
    slab->refcount--;
    cache->ops->TryFreeSlab(cache, slab);
}

void InitSlab (struct Slab * slab)
{
    slab->page = NULL;
    slab->refcount = 0;
    slab->freelist_head.DynamicInit();
    slab->cache_link.DynamicInit();
}

void InitBufctl (struct Bufctl * bufctl)
{
    /* When not allocated to user, the object itself is freelist element */
    bufctl->buf = (void *)bufctl;

    bufctl->freelist_link.DynamicInit();
}


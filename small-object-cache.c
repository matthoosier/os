#include "list.h"
#include "object-cache.h"
#include "object-cache-internal.h"
#include "vm.h"

static inline struct slab * slab_from_bufctl (void * bufctl_addr)
{
    return (struct slab *)(((vmaddr_t)bufctl_addr & PAGE_MASK) + PAGE_SIZE - sizeof(struct slab));
}

static void init_bufctl (struct bufctl * bufctl)
{
    /* When not allocated to user, the object itself is freelist element */
    bufctl->buf = (void *)bufctl;

    INIT_LIST_HEAD(&bufctl->freelist_link);
}

static void small_object_cache_try_reclaim_slab (struct object_cache * cache, struct slab * slab)
{
    if (slab->refcount == 0) {
        /*
        No need to deconstruct the bufctl freelist. It's all just one
        big cycle contained inside this slab. We'll just consider it
        all garbage-collected.
        */
        list_del_init(&slab->cache_link);

        /*
        The storage of the slab and all the bufctl's lives inside the
        allocated page, so by returning the page, we've implicitly
        deallocated the slab struct too.
        */
        vm_page_free(slab->page);
    }
}

static void small_object_cache_allocate_slab (struct object_cache * cache)
{
    struct page *   new_page;
    struct slab *   new_slab;
    unsigned int    objs_in_slab;
    unsigned int    i;

    new_page = vm_page_alloc();

    if (!new_page) {
        return;
    }

    new_slab = slab_from_bufctl((void *)new_page->base_address);
    init_slab(new_slab);
    new_slab->page = new_page;

    objs_in_slab = (new_page->length - sizeof(*new_slab))/ cache->element_size;

    /* Carve out (PAGE_SIZE / element_size) individual buffers. */
    for (i = 0; i < objs_in_slab; ++i) {
        vmaddr_t        buf_base;
        struct bufctl * new_bufctl;

        buf_base = new_page->base_address + cache->element_size * i;
        new_bufctl = (struct bufctl *)buf_base;
        init_bufctl(new_bufctl);

        /* Now insert into freelist */
        list_add_tail(&new_bufctl->freelist_link, &new_slab->freelist_head);
    }

    /* Finally insert the slab itself into the cache's records */
    list_add(&new_slab->cache_link, &cache->slab_head);
}

void * small_object_cache_alloc (struct object_cache * cache)
{
    struct slab * slab_cursor;
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
    small_object_cache_allocate_slab(cache);

    /* If a new slab was successfully added, it would be at the front */
    if (!list_empty(&cache->slab_head)) {
        slab_cursor = list_entry(cache->slab_head.next, struct slab, cache_link);
        if (!list_empty(&slab_cursor->freelist_head)) {
            bufctl = list_first_entry(&slab_cursor->freelist_head, struct bufctl, freelist_link);
            slab_cursor->refcount++;
            list_del_init(&bufctl->freelist_link);
            return bufctl;
        }
    }

    return NULL;
}

void small_object_cache_free (struct object_cache * cache, void * element)
{
    struct bufctl * reclaimed_bufctl;
    struct slab *   slab;

    /*
    Take back over the payload of the object as our internal
    free-list representation.
    */
    reclaimed_bufctl = (struct bufctl *)element;
    init_bufctl(reclaimed_bufctl);

    slab = slab_from_bufctl(reclaimed_bufctl);

    /*
    Stick on head of freelist to promote reuse of objects from slab
    that already had some allocations made.
    */
    list_add(&reclaimed_bufctl->freelist_link, &slab->freelist_head);
    slab->refcount--;
    small_object_cache_try_reclaim_slab(cache, slab);
}

#include "list.h"
#include "object-cache.h"
#include "object-cache-internal.h"
#include "vm.h"

static void static_init (void)
{
}

static void constructor (struct object_cache * cache)
{
}

static void destructor (struct object_cache * cache)
{
}

static struct slab * small_objects_slab_from_bufctl (
        struct object_cache * cache,
        void * bufctl_addr
        )
{
    return (struct slab *)(((vmaddr_t)bufctl_addr & PAGE_MASK) + PAGE_SIZE - sizeof(struct slab));
}

static void small_objects_free_slab (struct object_cache * cache, struct slab * slab)
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

static struct slab * small_objects_try_allocate_slab (struct object_cache * cache)
{
    struct page *   new_page;
    struct slab *   new_slab;
    unsigned int    objs_in_slab;
    unsigned int    i;

    new_page = vm_page_alloc();

    if (!new_page) {
        return NULL;
    }

    new_slab = cache->ops->map_bufctl_to_slab(cache, (void *)new_page->base_address);
    init_slab(new_slab);
    new_slab->page = new_page;

    objs_in_slab = (PAGE_SIZE - sizeof(*new_slab))/ cache->element_size;

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

    return new_slab;
}

const struct object_cache_ops small_objects_ops = {
    .static_init = static_init,
    .constructor = constructor,
    .destructor = destructor,
    .try_allocate_slab = small_objects_try_allocate_slab,
    .try_free_slab = small_objects_free_slab,
    .map_bufctl_to_slab = small_objects_slab_from_bufctl,
};

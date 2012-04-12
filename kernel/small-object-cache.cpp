#include <kernel/list.hpp>
#include <kernel/object-cache.hpp>
#include <kernel/vm.hpp>

#include "object-cache-internal.hpp"

static void static_init (void)
{
}

static void constructor (struct ObjectCache * cache)
{
}

static void destructor (struct ObjectCache * cache)
{
}

static struct Slab * small_objects_slab_from_bufctl (
        struct ObjectCache * cache,
        void * bufctl_addr
        )
{
    return (struct Slab *)(((VmAddr_t)bufctl_addr & PAGE_MASK) + PAGE_SIZE - sizeof(struct Slab));
}

static void small_objects_free_slab (struct ObjectCache * cache, struct Slab * slab)
{
    if (slab->refcount == 0) {
        /*
        No need to deconstruct the bufctl freelist. It's all just one
        big cycle contained inside this slab. We'll just consider it
        all garbage-collected.
        */
        List<Slab, &Slab::cache_link>::Remove(slab);

        /*
        The storage of the slab and all the bufctl's lives inside the
        allocated page, so by returning the page, we've implicitly
        deallocated the slab struct too.
        */
        Page::Free(slab->page);
    }
}

static struct Slab * small_objects_try_allocate_slab (struct ObjectCache * cache)
{
    Page *          new_page;
    struct Slab *   new_slab;
    unsigned int    objs_in_slab;
    unsigned int    i;

    new_page = Page::Alloc();

    if (!new_page) {
        return NULL;
    }

    new_slab = cache->ops->MapBufctlToSlab(cache, (void *)new_page->base_address);
    InitSlab(new_slab);
    new_slab->page = new_page;

    objs_in_slab = (PAGE_SIZE - sizeof(*new_slab))/ cache->element_size;

    /* Carve out (PAGE_SIZE / element_size) individual buffers. */
    for (i = 0; i < objs_in_slab; ++i) {
        VmAddr_t        buf_base;
        struct Bufctl * new_bufctl;

        buf_base = new_page->base_address + cache->element_size * i;
        new_bufctl = (struct Bufctl *)buf_base;
        InitBufctl(new_bufctl);

        /* Now insert into freelist */
        new_slab->freelist_head.Append(new_bufctl);
    }

    return new_slab;
}

const struct ObjectCacheOps small_objects_ops = {
    /* StaticInit       */  static_init,
    /* Constructor      */  constructor,
    /* Destructor       */  destructor,
    /* TryAllocateSlab  */  small_objects_try_allocate_slab,
    /* TryFreeSlab      */  small_objects_free_slab,
    /* MapBufctlToSlab  */  small_objects_slab_from_bufctl,
};

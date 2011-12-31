#include <stdint.h>

#include <sys/arch.h>
#include <sys/spinlock.h>

#include <kernel/assert.h>
#include <kernel/list.h>
#include <kernel/once.h>
#include <kernel/tree-map.h>
#include <kernel/vm.h>

#include "object-cache-internal.h"

/*
Used by large-object caches to get slab's not hosted inside the slab
storage. Prevents terrible space waste for large (>= PAGE_SIZE / 2)
objects.
*/
struct ObjectCache slabs_cache;
Spinlock_t         slabs_cache_lock = SPINLOCK_INIT;

static Once_t init_control = ONCE_INIT;

static void static_init ()
{
    void init_once (void * param)
    {
        ObjectCacheInit(&slabs_cache, sizeof(struct Slab));
    }

    Once(&init_control, init_once, NULL);
}

static void constructor (struct ObjectCache * cache)
{
    cache->bufctl_to_slab_map = TreeMapAlloc(TreeMapAddressCompareFunc);
}

static void destructor (struct ObjectCache * cache)
{
    TreeMapFree(cache->bufctl_to_slab_map);
    cache->bufctl_to_slab_map = NULL;
}

static struct Slab * large_objects_try_allocate_slab (struct ObjectCache * cache)
{
    struct Page *   new_page;
    struct Slab *   new_slab;
    unsigned int    objs_in_slab;
    unsigned int    i;

    new_page = VmPageAlloc();

    if (!new_page) {
        return NULL;
    }

    SpinlockLock(&slabs_cache_lock);
    new_slab = ObjectCacheAlloc(&slabs_cache);
    SpinlockUnlock(&slabs_cache_lock);

    if (!new_slab) {
        VmPageFree(new_page);
        return NULL;
    }

    InitSlab(new_slab);
    new_slab->page = new_page;

    objs_in_slab = PAGE_SIZE / cache->element_size;

    /* Carve out (PAGE_SIZE / element_size) individual buffers. */
    for (i = 0; i < objs_in_slab; ++i) {
        VmAddr_t        buf_base;
        struct Bufctl * new_bufctl;

        buf_base = new_page->base_address + cache->element_size * i;
        new_bufctl = (struct Bufctl *)buf_base;
        InitBufctl(new_bufctl);

        /* Record controlling slab's location in auxiliary map */
        TreeMapInsert(cache->bufctl_to_slab_map, new_bufctl, new_slab);
        assert(TreeMapLookup(cache->bufctl_to_slab_map, new_bufctl) == new_slab);

        /* Now insert into freelist */
        list_add_tail(&new_bufctl->freelist_link, &new_slab->freelist_head);
    }

    return new_slab;
}

static void large_objects_free_slab (struct ObjectCache * cache, struct Slab * slab)
{
    struct Bufctl * bufctl_cursor;

    if (slab->refcount == 0) {
        /* Unlink this slab from the cache's list */
        list_del_init(&slab->cache_link);

        /*
        There's no need to deconstruct each separate bufctl object contained
        in the freelist. They all live inside the storage of the VM page
        that we're about to free anyway.

        But we do need to iterate the list and remove the bufctl-to-slab mapping
        entry for each bufctl.
        */
        list_for_each_entry (bufctl_cursor, &slab->freelist_head, freelist_link) {
            
            struct Slab * removed;

            removed = TreeMapRemove(cache->bufctl_to_slab_map, bufctl_cursor);
            assert(removed != NULL);
        }

        /* Release the page that stored the user buffers */
        VmPageFree(slab->page);

        /* Finally free the slab, which is allocated from an object cache. */
        SpinlockLock(&slabs_cache_lock);
        ObjectCacheFree(&slabs_cache, slab);
        SpinlockUnlock(&slabs_cache_lock);
    }
}

static struct Slab * large_objects_slab_from_bufctl (
        struct ObjectCache * cache,
        void * bufctl_addr
        )
{
    return TreeMapLookup(cache->bufctl_to_slab_map, bufctl_addr);
}

const struct ObjectCacheOps large_objects_ops = {
    .StaticInit = static_init,
    .Constructor = constructor,
    .Destructor = destructor,
    .TryAllocateSlab = large_objects_try_allocate_slab,
    .TryFreeSlab = large_objects_free_slab,
    .MapBufctlToSlab = large_objects_slab_from_bufctl,
};

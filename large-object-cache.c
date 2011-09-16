#include <stdint.h>

#include "arch.h"
#include "assert.h"
#include "list.h"
#include "object-cache-internal.h"
#include "once.h"
#include "tree-map.h"
#include "vm.h"

/*
Used by large-object caches to get slab's not hosted inside the slab
storage. Prevents terrible space waste for large (>= PAGE_SIZE / 2)
objects.
*/
struct object_cache slabs_cache;

static once_t init_control = ONCE_INIT;

static void static_init ()
{
    void init_once (void * param)
    {
        object_cache_init(&slabs_cache, sizeof(struct slab));
    }

    once(&init_control, init_once, NULL);
}

static void constructor (struct object_cache * cache)
{
    int compare_func (tree_map_key_t left, tree_map_key_t right)
    {
        uintptr_t left_ptr = (uintptr_t)left;
        uintptr_t right_ptr = (uintptr_t)right;

        /* Explicit comparisons to avoid wraparound on uint math */
        if (left_ptr < right_ptr) {
            return -1;
        }
        else if (left_ptr > right_ptr) {
            return 1;
        }
        else {
            return 0;
        }
    }

    cache->bufctl_to_slab_map = tree_map_alloc(compare_func);
}

static void destructor (struct object_cache * cache)
{
    tree_map_free(cache->bufctl_to_slab_map);
    cache->bufctl_to_slab_map = NULL;
}

static struct slab * large_objects_try_allocate_slab (struct object_cache * cache)
{
    struct page *   new_page;
    struct slab *   new_slab;
    unsigned int    objs_in_slab;
    unsigned int    i;

    new_page = vm_page_alloc();

    if (!new_page) {
        return NULL;
    }

    new_slab = object_cache_alloc(&slabs_cache);

    if (!new_slab) {
        vm_page_free(new_page);
        return NULL;
    }

    init_slab(new_slab);
    new_slab->page = new_page;

    objs_in_slab = PAGE_SIZE / cache->element_size;

    /* Carve out (PAGE_SIZE / element_size) individual buffers. */
    for (i = 0; i < objs_in_slab; ++i) {
        vmaddr_t        buf_base;
        struct bufctl * new_bufctl;

        buf_base = new_page->base_address + cache->element_size * i;
        new_bufctl = (struct bufctl *)buf_base;
        init_bufctl(new_bufctl);

        /* Record controlling slab's location in auxiliary map */
        tree_map_insert(cache->bufctl_to_slab_map, new_bufctl, new_slab);
        assert(tree_map_lookup(cache->bufctl_to_slab_map, new_bufctl) == new_slab);

        /* Now insert into freelist */
        list_add_tail(&new_bufctl->freelist_link, &new_slab->freelist_head);
    }

    return new_slab;
}

static void large_objects_free_slab (struct object_cache * cache, struct slab * slab)
{
    struct bufctl * bufctl_cursor;

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
            
            struct slab * removed;

            removed = tree_map_remove(cache->bufctl_to_slab_map, bufctl_cursor);
            assert(removed != NULL);
        }

        /* Release the page that stored the user buffers */
        vm_page_free(slab->page);

        /* Finally free the slab, which is allocated from an object cache. */
        object_cache_free(&slabs_cache, slab);
    }
}

static struct slab * large_objects_slab_from_bufctl (
        struct object_cache * cache,
        void * bufctl_addr
        )
{
    return tree_map_lookup(cache->bufctl_to_slab_map, bufctl_addr);
}

const struct object_cache_ops large_objects_ops = {
    .static_init = static_init,
    .constructor = constructor,
    .destructor = destructor,
    .try_allocate_slab = large_objects_try_allocate_slab,
    .try_free_slab = large_objects_free_slab,
    .map_bufctl_to_slab = large_objects_slab_from_bufctl,
};

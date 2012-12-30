#include <stdbool.h>
#include <stdlib.h>

#include <sys/arch.h>
#include <sys/bits.h>
#include <sys/spinlock.h>

#include <kernel/array.h>
#include <kernel/list.hpp>
#include <kernel/math.hpp>
#include <kernel/once.h>
#include <kernel/vm.hpp>

//! Info to track one window into the heap viewed as a series
//! of consecutive chunks of memory, each at some power of two
//! multiplied by PAGE_SIZE.
struct BuddylistLevel
{
    typedef List<Page, &Page::list_link> FreelistType;

    /*!
     * Tracks the set of available PAGE_SIZE*2^k chunks
     * of memory.
     */
    FreelistType * freelist_head;

    /*!
     * Storage pointed at by freelist_head
     *
     * We do this rather than just allocate freelist_head
     * directly in the payload of BuddylistLevel to prevent
     * its ctor from being automatically run during C++
     * startup code. That would cause undefined ordering
     * between the VM subsystem and the various slab allocators'
     * (which are commonly declared as static global objects)
     * ctors.
     */
    char storage[sizeof(*freelist_head)] __attribute__((aligned(__BIGGEST_ALIGNMENT__)));

    /* Each bitmap is an array of (num_pages / (2 << i)) flags */
    struct
    {
        unsigned int    element_count;
        uint8_t *       busy_elements;
    } bitmap;

};

/* The largest chunksize (in PAGE_SIZE * 2^k) that we'll track, plus 1 */
#define NUM_BUDDYLIST_LEVELS 3

static BuddylistLevel buddylists[NUM_BUDDYLIST_LEVELS];

static unsigned int     num_pages;
static Page *           page_structs;
static VmAddr_t         pages_base;
static Spinlock_t       lock = SPINLOCK_INIT;

static Once_t           init_control = ONCE_INIT;

static inline unsigned int page_index_from_base_address (VmAddr_t base)
{
    assert(base >= pages_base);
    return (base - pages_base) >> PAGE_SHIFT;
}

static inline unsigned int page_index_from_struct (Page * page)
{
    return page_index_from_base_address(page->base_address);
}

static inline int buddylist_level_from_alignment (VmAddr_t addr)
{
    int i;

    for (i = NUM_BUDDYLIST_LEVELS - 1; i >= 0; i--) {
        if (addr % (PAGE_SIZE << i) == 0) {
            return i;
        }
    }

    return -1;
}

static void vm_init (void * ignored)
{
    unsigned int page_structs_array_size;
    unsigned int i;

    for (i = 0; i < NUM_BUDDYLIST_LEVELS; i++) {
        buddylists[i].freelist_head = reinterpret_cast<BuddylistLevel::FreelistType *>(&buddylists[i].storage);
        new (buddylists[i].freelist_head) BuddylistLevel::FreelistType();
    }

    size_t page_count = PAGE_COUNT_FROM_SIZE(HEAP_SIZE);

    /* The number of pages divided by eight, rounded up to nearest 8 */
    page_structs_array_size = sizeof(*page_structs) * PAGE_COUNT_FROM_SIZE(HEAP_SIZE);

    pages_base = VIRTUAL_HEAP_START;

    /* Now carve out space for the metadata structs */
    pages_base += page_structs_array_size;

    /* Now carve out space for the buddy-list bitmaps */
    for (i = 0; i < NUM_BUDDYLIST_LEVELS; i++) {
        unsigned int idx;

        /* Level i of the buddylist will have PAGE_COUNT / 2^i blocks */
        buddylists[i].bitmap.element_count = page_count >> i;

        /* Assign bitmap pointer, increment pages_base */
        buddylists[i].bitmap.busy_elements = (uint8_t *)pages_base;
        pages_base += BITS_TO_BYTES(
            /* Round up to nearest byte boundary */
            (buddylists[i].bitmap.element_count + 7) & ~0x7
            );

        /* Initialize bitmap. All blocks are initially unowned. */
        for (idx = 0; idx < buddylists[i].bitmap.element_count; idx++) {
            BitmapClear(buddylists[i].bitmap.busy_elements, idx);
        }
    }

    /* Round up to the nearest largest-buddy block boundary */
    pages_base = Math::RoundUp(pages_base, PAGE_SIZE << (NUM_BUDDYLIST_LEVELS - 1));

    num_pages = PAGE_COUNT_FROM_SIZE(HEAP_SIZE - (pages_base - VIRTUAL_HEAP_START));
    page_structs = (Page *)VIRTUAL_HEAP_START;

    /*
    Initialize struct pointer for each block. We only iterate across
    every 2^(NUM_BUDDYLIST_LEVELS - 1) pages because initially all pages are
    coaslesced into the biggest possible chunks.
    */
    for (i = 0; i < num_pages; i += SETBIT(NUM_BUDDYLIST_LEVELS - 1)) {
        VmAddr_t base_address = pages_base + (i * PAGE_SIZE);
        int buddy_level = buddylist_level_from_alignment(base_address);
        assert(buddy_level == NUM_BUDDYLIST_LEVELS - 1);

        page_structs[i].base_address = base_address;

        new (&page_structs[i].list_link) ListElement();

        buddylists[buddy_level].freelist_head->Append(&page_structs[i]);

        for (unsigned int j = i + 1; j < i + SETBIT(NUM_BUDDYLIST_LEVELS - 1); j++)
        {
            base_address = pages_base + (j * PAGE_SIZE);
            page_structs[j].base_address = base_address;
            new (&page_structs[j].list_link) ListElement();
        }
    }
}

Page * Page::AllocInternal (
        unsigned int order,
        bool mark_busy_in_bitmap
        )
{
    Page * result;

    if (order >= NUM_BUDDYLIST_LEVELS) {
        return NULL;
    }

    /* If no block of the requested size is available, split a larger one */
    if (buddylists[order].freelist_head->Empty()) {
        Page * block_to_split;
        Page * second_half_struct;
        VmAddr_t  second_half_address;

        /* Recursive call */
        block_to_split = AllocInternal(order + 1, false);

        if (!block_to_split) {
            return NULL;
        }

        /*
        If we got this far, then the 'block_to_split' structure is already
        the right 'Page' instance to represent the lower-half of
        the block once it's split in two.

        We still need to compute the right struct instance (and initialize
        it, since it won't have been filled in when the larger block was
        held in the higher buddylist level) for the second half of the split
        memory.
        */
        second_half_address = block_to_split->base_address + (PAGE_SIZE << order);
        second_half_struct = &page_structs[page_index_from_base_address(second_half_address)];
        assert(second_half_struct->base_address == second_half_address);
        second_half_struct->base_address = second_half_address;

        /*
        Theoretically block_to_split's list head is already initialized.
        But just do this to make sure, since we already have to initialize
        second_half_struct's list head anyway.
        */

        /*
        Insert these two new (PAGE_SIZE << order)-sized chunks of memory
        into the buddylist freelist.

        They'll be found immediately below in the code that extracts
        the block at the head of the list.
        */
        assert(block_to_split->list_link.Unlinked());
        assert(second_half_struct->list_link.Unlinked());
        buddylists[order].freelist_head->Append(block_to_split);
        buddylists[order].freelist_head->Append(second_half_struct);
    }

    /*
     * Remove the selected page from the free-list.
     *
     * This will always find an entry on the first iteration.
     */
    result = buddylists[order].freelist_head->PopFirst();

    /* Mark page as busy in whichever level's bitmap is appropriate. */
    if (mark_busy_in_bitmap) {
        assert((page_index_from_struct(result) % (1 << order)) == 0);
        BitmapSet(
            buddylists[order].bitmap.busy_elements,
            page_index_from_struct(result) >> order
            );
    }

    return result;
}

Page * Page::Alloc (unsigned int order)
{
    Page * ret;

    Once(&init_control, vm_init, NULL);

    SpinlockLock(&lock);
    ret = AllocInternal(order, true);
    SpinlockUnlock(&lock);

    return ret;
}

static int get_order_allocated (Page * page)
{
    int largest_order = buddylist_level_from_alignment(page->base_address);

    // Find the chunk level this block was allocated on
    for (int order = largest_order; order >= 0; order--)
    {
        int idx = page_index_from_base_address(page->base_address) >> order;

        if (BitmapGet(buddylists[order].bitmap.busy_elements, idx))
        {
            return order;
        }
    }

    return -1;
}

static void try_merge_block (Page * block, unsigned int order)
{
    VmAddr_t        partner_address;
    unsigned int    partner_index;
    Page *          partner;

    /* No coaslescing is possible if block is from largest bucket */
    if (order >= NUM_BUDDYLIST_LEVELS - 1) {
        return;
    }

    if (block->base_address == Math::RoundUp(block->base_address, PAGE_SIZE << (order + 1)))
    {
        // 'block' is aligned at a PAGE_SIZE << (order + 1) boundary, so
        // its buddy must be immediately above
        partner_address = block->base_address + (PAGE_SIZE << order);
    }
    else
    {
        // 'block' is not aligned at a PAGE_SIZE << (order + 1) boundary,
        // so its buddy must be immediately below
        partner_address = block->base_address - (PAGE_SIZE << order);
    }

    partner_index = page_index_from_base_address(partner_address);

    partner = &page_structs[partner_index];

    // Check that the block passed in is _really_ marked as not-in-use
    assert(!BitmapGet(buddylists[order].bitmap.busy_elements, page_index_from_base_address(block->base_address) >> order));

    for (int fragment_order = order; fragment_order >= 0; fragment_order--)
    {
        for (VmAddr_t fragment_address = partner_address;
             fragment_address < partner_address + (PAGE_SIZE << order);
             fragment_address += PAGE_SIZE << (fragment_order))
        {
            unsigned int fragment_idx = page_index_from_base_address(fragment_address);

            if (BitmapGet(buddylists[fragment_order].bitmap.busy_elements,
                          fragment_idx >> fragment_order))
            {
                // Partner page (or a divided part of it) is still
                // allocated out to a client. Abandon attempt to
                // merge.
                return;
            }
        }
    }

    // Partner isn't allocated out to a user, so merge.
    Page * merged_block = block->base_address < partner->base_address
            ? block
            : partner;

    assert(!block->list_link.Unlinked());
    assert(!partner->list_link.Unlinked());

    /* Unlink from current blocksize's freelist */
    List<Page, &Page::list_link>::Remove(block);
    List<Page, &Page::list_link>::Remove(partner);

    /* Insert lower of the two blocks into next blocksize's freelist. */
    buddylists[order + 1].freelist_head->Append(merged_block);

    /* Now try to merge at the next level. */
    try_merge_block(merged_block, order + 1);
}

void Page::Free (Page * page)
{
    assert(page->list_link.Unlinked());

    Once(&init_control, vm_init, NULL);

    SpinlockLock(&lock);

    // Figure out what buddylevel the pageset was allocated on
    int order = get_order_allocated(page);
    assert(order != -1);

    int idx = page_index_from_base_address(page->base_address);

    // Return to freelist and clear bitmap
    new (&page->list_link) ListElement();
    buddylists[order].freelist_head->Prepend(page);
    BitmapClear(buddylists[order].bitmap.busy_elements, idx);

    // Try to coalesce
    try_merge_block(page, order);

    SpinlockUnlock(&lock);
    return;
}

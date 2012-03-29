#include <stdbool.h>
#include <stdlib.h>

#include <sys/arch.h>
#include <sys/bits.h>
#include <sys/spinlock.h>

#include <kernel/array.h>
#include <kernel/list.hpp>
#include <kernel/once.h>
#include <kernel/vm.hpp>

typedef struct
{
    List<Page, &Page::list_link> freelist_head;

    /* Each bitmap is an array of (num_pages / (2 << i)) flags */
    struct
    {
        unsigned int    element_count;
        uint8_t *       elements;
    } bitmap;

} buddylist_level;

/* The largest chunksize (in PAGE_SIZE * 2^k) that we'll track, plus 1 */
#define NUM_BUDDYLIST_LEVELS 3

static buddylist_level buddylists[NUM_BUDDYLIST_LEVELS];

static unsigned int     num_pages;
static struct Page *    page_structs;
static VmAddr_t         pages_base;
static Spinlock_t       lock = SPINLOCK_INIT;

static Once_t           init_control = ONCE_INIT;

static inline unsigned int page_index_from_base_address (VmAddr_t base)
{
    return (base - pages_base) >> PAGE_SHIFT;
}

static inline unsigned int page_index_from_struct (struct Page * page)
{
    return page_index_from_base_address(page->base_address);
}

static inline int buddylist_level_from_alignment (VmAddr_t addr)
{
    unsigned int i;

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
        buddylists[i].bitmap.elements = (uint8_t *)pages_base;
        pages_base += BITS_TO_BYTES(
            /* Round up to nearest byte boundary */
            (buddylists[i].bitmap.element_count + 7) & ~0x7
            );

        /* Initialize bitmap. All blocks are initially unowned. */
        for (idx = 0; idx < buddylists[i].bitmap.element_count; idx++) {
            BitmapClear(buddylists[i].bitmap.elements, idx);
        }
    }

    /* Round up to the nearest largest-buddy block boundary */
    pages_base += (PAGE_SIZE << (NUM_BUDDYLIST_LEVELS - 1)) - 1;
    pages_base &= PAGE_MASK << (NUM_BUDDYLIST_LEVELS - 1);

    num_pages = PAGE_COUNT_FROM_SIZE(HEAP_SIZE - (pages_base - VIRTUAL_HEAP_START));
    page_structs = (struct Page *)VIRTUAL_HEAP_START;

    /*
    Initialize struct pointer for each block. We only iterate across
    every 2^(NUM_BUDDYLIST_LEVELS - 1) pages because initially all pages are
    coaslesced into the biggest possible chunks.
    */
    for (i = 0; i < num_pages; i += SETBIT(NUM_BUDDYLIST_LEVELS - 1)) {
        VmAddr_t base_address = pages_base + (i * PAGE_SIZE);
        int buddy_level = buddylist_level_from_alignment(base_address);

        page_structs[i].base_address = base_address;

        page_structs[i].list_link.DynamicInit();

        buddylists[buddy_level].freelist_head.Append(&page_structs[i]);
    }
}

struct Page * vm_pages_alloc_internal (
        unsigned int order,
        bool mark_busy_in_bitmap
        )
{
    struct Page * result;

    Once(&init_control, vm_init, NULL);

    if (order >= NUM_BUDDYLIST_LEVELS) {
        return NULL;
    }

    /* If no block of the requested size is available, split a larger one */
    if (buddylists[order].freelist_head.Empty()) {
        struct Page * block_to_split;
        struct Page * second_half_struct;
        VmAddr_t  second_half_address;

        /* Recursive call */
        block_to_split = vm_pages_alloc_internal(order + 1, false);

        if (!block_to_split) {
            return NULL;
        }

        /*
        If we got this far, then the 'block_to_split' structure is already
        the right 'struct Page' instance to represent the lower-half of
        the block once it's split in two.

        We still need to compute the right struct instance (and initialize
        it, since it won't have been filled in when the larger block was
        held in the higher buddylist level) for the second half of the split
        memory.
        */
        second_half_address = block_to_split->base_address + (PAGE_SIZE << order);
        second_half_struct = &page_structs[page_index_from_base_address(second_half_address)];
        second_half_struct->base_address = second_half_address;

        /*
        Theoretically block_to_split's list head is already initialized.
        But just do this to make sure, since we already have to initialize
        second_half_struct's list head anyway.
        */
        block_to_split->list_link.DynamicInit();
        second_half_struct->list_link.DynamicInit();

        /*
        Insert these two new (PAGE_SIZE << order)-sized chunks of memory
        into the buddylist freelist.

        They'll be found immediately below in the code that extracts
        the block at the head of the list.
        */
        buddylists[order].freelist_head.Append(block_to_split);
        buddylists[order].freelist_head.Append(second_half_struct);
    }

    /*
     * Remove the selected page from the free-list.
     *
     * This will always find an entry on the first iteration.
     */
    result = buddylists[order].freelist_head.PopFirst();

    /* Mark page as busy in whichever level's bitmap is appropriate. */
    if (mark_busy_in_bitmap) {
        BitmapSet(
            buddylists[order].bitmap.elements,
            page_index_from_struct(result) >> order
            );
    }

    return result;
}

struct Page * VmPageAlloc ()
{
    struct Page * ret;

    SpinlockLock(&lock);
    ret = vm_pages_alloc_internal(0, true);
    SpinlockUnlock(&lock);

    return ret;
}

struct Page * VmPagesAlloc (unsigned int order)
{
    struct Page * ret;

    SpinlockLock(&lock);
    ret = vm_pages_alloc_internal(order, true);
    SpinlockUnlock(&lock);

    return ret;
}

static void try_merge_block (struct Page * block, unsigned int order)
{
    VmAddr_t        partner_address;
    unsigned int    partner_index;
    struct Page *   partner;

    /* No coaslescing is possible if block is from largest bucket */
    if (order >= NUM_BUDDYLIST_LEVELS - 1) {
        return;
    }

    partner_address = (block->base_address >> PAGE_SHIFT) == ((block->base_address >> (PAGE_SHIFT + 1)) << 1)
            ? block->base_address + (PAGE_SIZE << order)
            : block->base_address - (PAGE_SIZE << order);

    partner_index = page_index_from_base_address(partner_address);

    partner = &page_structs[partner_index];

    /* If the partner isn't allocated out to a user, then merge */
    if (!BitmapGet(buddylists[order].bitmap.elements, partner_index >> order)) {
        struct Page * merged_block = block->base_address < partner->base_address
                ? block
                : partner;

        /* Unlink from current blocksize's freelist */
        List<Page, &Page::list_link>::Remove(block);
        List<Page, &Page::list_link>::Remove(partner);

        /* Insert lower of the two blocks into next blocksize's freelist. */
        buddylists[order + 1].freelist_head.Append(merged_block);

        /* Now try to merge at the next level. */
        try_merge_block(merged_block, order + 1);
    }
}

void VmPageFree (struct Page * page)
{
    unsigned int order;
    unsigned int largest_order;
    unsigned int idx;

    Once(&init_control, vm_init, NULL);

    SpinlockLock(&lock);

    /*
    Figure out the chunkiest possible buddylist level this block could
    belong to, based on its address.
    */
    largest_order = buddylist_level_from_alignment(page->base_address);

    /*
    Find the chunk level this block was allocated on, and return it to
    the freelist for that size.
    */
    for (order = largest_order; order >= 0; order--) {
        idx = page_index_from_base_address(page->base_address) >> order;

        if (BitmapGet(buddylists[order].bitmap.elements, idx)) {

            /* Found it. Return to freelist and clear bitmap. Then done. */
            page->list_link.DynamicInit();
            buddylists[order].freelist_head.Prepend(page);
            BitmapClear(buddylists[order].bitmap.elements, idx);
            try_merge_block(page, order);
            goto done;
        }
    }

    while (1) {}

done:
    SpinlockUnlock(&lock);
    return;
}

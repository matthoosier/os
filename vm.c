#include <stdlib.h>
#include "arch.h"
#include "bits.h"
#include "vm.h"

static unsigned int num_pages;
static uint8_t *    pages_in_use_bitmap;
static vmaddr_t     pages_base;

static void ensure_init ()
{
    unsigned int bits_needed_for_bitmap;
    unsigned int i;

    /* The number of pages divided by eight, rounded up to nearest 8 */
    bits_needed_for_bitmap = PAGE_COUNT_FROM_SIZE(HEAP_SIZE);
    bits_needed_for_bitmap += 7;
    bits_needed_for_bitmap &= ~7;

    pages_base = VIRTUAL_HEAP_START;

    /* Now carve out space for the in-use bitmap */
    pages_base += bits_needed_for_bitmap >> 3;

    /* Round up to the nearest page boundary */
    pages_base += PAGE_SIZE - 1;
    pages_base &= PAGE_MASK;

    num_pages = PAGE_COUNT_FROM_SIZE(HEAP_SIZE - (pages_base - VIRTUAL_HEAP_START));
    pages_in_use_bitmap = (uint8_t *)VIRTUAL_HEAP_START;

    for (i = 0; i < bits_needed_for_bitmap >> 3; i++) {
        pages_in_use_bitmap[i] = 0;
    }
}

void vm_init()
{
    ensure_init();
}

void * vm_page_alloc ()
{
    unsigned int i;

    for (i = 0; i < num_pages; i++) {
        unsigned int idx        = i >> 3;
        uint8_t packed_eight    = pages_in_use_bitmap[idx];
        uint8_t mask            = SETBIT(i & 0b111);

        if ((packed_eight & mask) == 0) {
            pages_in_use_bitmap[idx] |= mask;
            return (void *)(pages_base + (PAGE_SIZE * i));
        }
    }

    return NULL;
}

void vm_page_free (void * page_address)
{
    vmaddr_t page_addr  = ((vmaddr_t)page_address);
    unsigned int i      = (page_addr - pages_base) >> PAGE_SHIFT;
    unsigned int idx    = i >> 3;
    uint8_t mask        = SETBIT(i & 0b111);

    pages_in_use_bitmap[idx] &= ~mask;
}

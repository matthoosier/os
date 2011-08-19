#include <stdlib.h>
#include "arch.h"
#include "bits.h"
#include "vm.h"

static unsigned int     num_pages;
static struct page *    page_structs;
static vmaddr_t         pages_base;

static void ensure_init ()
{
    unsigned int array_size;
    unsigned int i;

    size_t struct_size = sizeof(*page_structs);
    size_t page_count = PAGE_COUNT_FROM_SIZE(HEAP_SIZE);

    struct_size = struct_size;
    page_count = page_count;

    /* The number of pages divided by eight, rounded up to nearest 8 */
    array_size = sizeof(*page_structs) * PAGE_COUNT_FROM_SIZE(HEAP_SIZE);

    pages_base = VIRTUAL_HEAP_START;

    /* Now carve out space for the metadata structs */
    pages_base += array_size;

    /* Round up to the nearest page boundary */
    pages_base += PAGE_SIZE - 1;
    pages_base &= PAGE_MASK;

    num_pages = PAGE_COUNT_FROM_SIZE(HEAP_SIZE - (pages_base - VIRTUAL_HEAP_START));
    page_structs = (struct page *)VIRTUAL_HEAP_START;

    for (i = 0; i < num_pages; i++) {
        page_structs[i].base_address    = pages_base + (i * PAGE_SIZE);
        page_structs[i].length          = PAGE_SIZE;
        page_structs[i].in_use          = 0;
    }
}

void vm_init()
{
    ensure_init();
}

struct page * vm_page_alloc ()
{
    unsigned int i;

    for (i = 0; i < num_pages; i++) {
        if (!page_structs[i].in_use) {
            page_structs[i].in_use = 1;
            return &page_structs[i];
        }
    }

    return NULL;
}

void vm_page_free (struct page * page)
{
    page->in_use = 0;
}

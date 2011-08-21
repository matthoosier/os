#include <stdlib.h>
#include "arch.h"
#include "bits.h"
#include "list.h"
#include "vm.h"

static unsigned int     num_pages;
static struct page *    page_structs;
static vmaddr_t         pages_base;

struct list_head        freelist_head = LIST_HEAD_INIT(freelist_head);

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

        INIT_LIST_HEAD(&page_structs[i].list_link);
        list_add_tail(&page_structs[i].list_link, &freelist_head);
    }
}

void vm_init()
{
    ensure_init();
}

struct page * vm_page_alloc ()
{
    struct page * result;

    if (list_empty(&freelist_head)) {
        return NULL;
    }

    /* This will always find an entry on the first iteration. */
    result = list_first_entry(&freelist_head, struct page, list_link);

    /* Remove the selected page from the free-list. */
    list_del_init(&result->list_link);

    return result;
}

void vm_page_free (struct page * page)
{
    /* Avoid unnecessary internal fragmentation.  */
    list_add(&page->list_link, &freelist_head);
}

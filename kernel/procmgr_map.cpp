#include <stdbool.h>

#include <sys/arch.h>
#include <sys/error.h>
#include <sys/procmgr.h>

#include <kernel/kmalloc.h>
#include <kernel/list.h>
#include <kernel/message.h>
#include <kernel/mmu.h>
#include <kernel/process.h>
#include <kernel/procmgr.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static void HandleMapPhys (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct ProcMgrReply reply;
    PhysAddr_t          phys;
    size_t              len;
    VmAddr_t            virt;
    bool                mapped;
    unsigned int        i;

    phys = buf->payload.map_phys.physaddr;
    len = buf->payload.map_phys.len;

    if ((phys % PAGE_SIZE != 0) || (len < 0)) {
        KMessageReply(message, ERROR_INVALID, buf, 0);
    }
    else {

        struct mapped_page
        {
            VmAddr_t            page_base;
            struct list_head    link;
        };

        struct list_head mapped_pages = LIST_HEAD_INIT(mapped_pages);
        struct mapped_page * page;

        for (i = 0; i < len; i += PAGE_SIZE) {

            page = (struct mapped_page *)kmalloc(sizeof(*page));

            if (!page) {
                break;
            }

            mapped = TranslationTableMapNextPage(
                    message->sender->process->pagetable,
                    &virt,
                    phys,
                    PROT_USER_READWRITE
                    );

            /*
            First allocation is the base address of the whole
            thing.
            */
            if (i == 0 && mapped) {
                reply.payload.map_phys.vmaddr = virt;
            }

            if (mapped) {
                page->page_base = virt;
                INIT_LIST_HEAD(&page->link);
                list_add_tail(&page->link, &mapped_pages);
            }
            else if (!mapped) {

                /* Back out all the existing mappings */
                while (!list_empty(&mapped_pages)) {
                    page = list_first_entry(&mapped_pages, struct mapped_page, link);
                    TranslationTableUnmapPage(
                            message->sender->process->pagetable,
                            page->page_base
                            );
                    list_del_init(&page->link);

                    /* Free the block hosting the list node */
                    kfree(page, sizeof(*page));
                }

                break;
            }
        }

        if (list_empty(&mapped_pages)) {
            KMessageReply(message, ERROR_INVALID, &reply, sizeof(reply));
        } else {
            KMessageReply(message, ERROR_OK, &reply, sizeof(reply));

            /* List of partial pages no longer needed */
            while (!list_empty(&mapped_pages)) {
                page = list_first_entry(&mapped_pages, struct mapped_page, link);
                list_del_init(&page->link);

                /* Free the block hosting the list node */
                kfree(page, sizeof(*page));
            }
        }
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_MAP_PHYS, HandleMapPhys)

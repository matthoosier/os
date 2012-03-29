#include <stdbool.h>

#include <sys/arch.h>
#include <sys/error.h>
#include <sys/procmgr.h>

#include <kernel/kmalloc.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>
#include <kernel/vm.hpp>

struct mapped_page
{
    VmAddr_t    page_base;
    ListElement link;
};

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

        List<mapped_page, &mapped_page::link> mapped_pages;

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
                page->link.DynamicInit();
                mapped_pages.Append(page);
            }
            else if (!mapped) {

                /* Back out all the existing mappings */
                while (!mapped_pages.Empty()) {
                    page = mapped_pages.PopFirst();
                    TranslationTableUnmapPage(
                            message->sender->process->pagetable,
                            page->page_base
                            );

                    /* Free the block hosting the list node */
                    kfree(page, sizeof(*page));
                }

                break;
            }
        }

        if (mapped_pages.Empty()) {
            KMessageReply(message, ERROR_INVALID, &reply, sizeof(reply));
        } else {
            KMessageReply(message, ERROR_OK, &reply, sizeof(reply));

            /* List of partial pages no longer needed */
            while (!mapped_pages.Empty()) {
                page = mapped_pages.PopFirst();

                /* Free the block hosting the list node */
                kfree(page, sizeof(*page));
            }
        }
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_MAP_PHYS, HandleMapPhys)

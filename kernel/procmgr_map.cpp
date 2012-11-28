#include <new>
#include <stdbool.h>

#include <sys/arch.h>
#include <sys/error.h>
#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/thread.hpp>
#include <kernel/vm.hpp>

struct MappedPage
{
public:
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(MappedPage));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

public:
    VmAddr_t    page_base;
    ListElement link;

public:
    static SyncSlabAllocator<MappedPage> sSlab;
};

SyncSlabAllocator<MappedPage> MappedPage::sSlab;

static void HandleMapPhys (
        Message * message,
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
        message->Reply(ERROR_INVALID, buf, 0);
    }
    else {

        List<MappedPage, &MappedPage::link> mapped_pages;

        MappedPage * page = 0;
        Thread * sender = message->GetSender();

        for (i = 0; i < len; i += PAGE_SIZE) {

            try {
                page = new MappedPage();
            } catch (std::bad_alloc) {
                break;
            }

            mapped = sender->process->GetTranslationTable()->MapNextPage(
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
                new (&page->link) ListElement();
                mapped_pages.Append(page);
            }
            else if (!mapped) {

                /* Back out all the existing mappings */
                while (!mapped_pages.Empty()) {
                    page = mapped_pages.PopFirst();
                    sender->process->GetTranslationTable()->UnmapPage(page->page_base);

                    /* Free the block hosting the list node */
                    delete page;
                }

                break;
            }
        }

        if (mapped_pages.Empty()) {
            message->Reply(ERROR_INVALID, &reply, sizeof(reply));
        } else {
            message->Reply(ERROR_OK, &reply, sizeof(reply));

            /* List of partial pages no longer needed */
            while (!mapped_pages.Empty()) {
                page = mapped_pages.PopFirst();

                /* Free the block hosting the list node */
                delete page;
            }
        }
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_MAP_PHYS, HandleMapPhys)

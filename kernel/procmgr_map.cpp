#include <new>
#include <stdbool.h>

#include <muos/arch.h>
#include <muos/error.h>
#include <muos/procmgr.h>

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

static void HandleMapPhys (RefPtr<Message> message)
{
    struct ProcMgrMessage   msg;
    struct ProcMgrReply reply;
    PhysAddr_t          phys;
    size_t              len_to_map;
    VmAddr_t            virt;

    ssize_t msg_len = PROC_MGR_MSG_LEN(map_phys);
    ssize_t actual_len = message->Read(0, &msg, msg_len);

    if (actual_len != msg_len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    phys = msg.payload.map_phys.physaddr;
    len_to_map = msg.payload.map_phys.len;

    if ((phys % PAGE_SIZE != 0) || (len_to_map < 0)) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    AddressSpace * addressSpace = message->GetSender()->process->GetAddressSpace();

    if (!addressSpace->CreatePhysicalMapping(phys, len_to_map, virt)) {
        message->Reply(ERROR_NO_MEM, IoBuffer::GetEmpty());
    }
    else {
        reply.payload.map_phys.vmaddr = virt;
        message->Reply(ERROR_OK, &reply, sizeof(reply));
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_MAP_PHYS, HandleMapPhys)

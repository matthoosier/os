#include <muos/error.h>
#include <muos/procmgr.h>

#include <kernel/address-space.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>

void HandleSbrk (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    size_t n;

    n = message->Read(0, &msg, sizeof(msg));

    if (n < sizeof(msg)) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    if (msg.payload.sbrk.increment % PAGE_SIZE != 0) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    VmAddr_t prev;
    VmAddr_t next;

    AddressSpace * addressSpace = message->GetSender()->process->GetAddressSpace();

    if (addressSpace->ExtendHeap(msg.payload.sbrk.increment, prev, next))
    {
        reply.payload.sbrk.previous = prev;
        message->Reply(ERROR_OK, &reply, sizeof(reply));
    }
    else
    {
        message->Reply(ERROR_NO_MEM, IoBuffer::GetEmpty());
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_SBRK, HandleSbrk)

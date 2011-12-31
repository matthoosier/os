#include <sys/io.h>
#include <sys/message.h>
#include <sys/procmgr.h>

InterruptHandler_t InterruptAttach (
        InterruptHandlerFunc func,
        int irq_number
        )
{
    struct ProcMgrMessage m;
    struct ProcMgrReply reply;

    m.type = PROC_MGR_MESSAGE_INTERRUPT_ATTACH;
    m.payload.interrupt_attach.func = func;
    m.payload.interrupt_attach.irq_number = irq_number;

    int ret = MessageSend(
            PROCMGR_CONNECTION_ID,
            &m,
            sizeof(m),
            &reply,
            sizeof(reply)
            );

    if (ret < 0) {
        return ret;
    } else {
        return reply.payload.interrupt_attach.handler;
    }
}

int InterruptDetach (
        InterruptHandler_t id
        )
{
    struct ProcMgrMessage m;
    struct ProcMgrReply reply;

    m.type = PROC_MGR_MESSAGE_INTERRUPT_DETACH;
    m.payload.interrupt_detach.handler = id;

    int ret = MessageSend(
            PROCMGR_CONNECTION_ID,
            &m,
            sizeof(m),
            &reply,
            sizeof(reply)
            );

    if (ret < 0) {
        return ret;
    } else {
        return 0;
    }
}

void * MapPhysical (
        uintptr_t physaddr,
        size_t len
        )
{
    struct ProcMgrMessage m;
    struct ProcMgrReply reply;

    m.type = PROC_MGR_MESSAGE_MAP_PHYS;
    m.payload.map_phys.physaddr = physaddr;
    m.payload.map_phys.len = len;

    int ret = MessageSend(
            PROCMGR_CONNECTION_ID,
            &m,
            sizeof(m),
            &reply,
            sizeof(reply)
            );

    if (ret < 0) {
        return NULL;
    }
    else {
        return (void *)reply.payload.map_phys.vmaddr;
    }
}

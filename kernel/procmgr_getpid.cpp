#include <sys/procmgr.h>

#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleGetPid (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct ProcMgrReply reply;

    memset(&reply, 0, sizeof(reply));
    reply.payload.getpid.pid = message->sender->process->GetId();
    KMessageReply(message, ERROR_OK, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_GETPID, HandleGetPid)

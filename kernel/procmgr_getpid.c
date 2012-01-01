#include <sys/procmgr.h>

#include <kernel/message.h>
#include <kernel/process.h>
#include <kernel/procmgr.h>
#include <kernel/thread.h>

static void HandleGetPid (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct ProcMgrReply reply;

    memset(&reply, 0, sizeof(reply));
    reply.payload.getpid.pid = message->sender->process->pid;
    KMessageReply(message, ERROR_OK, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_GETPID, HandleGetPid)

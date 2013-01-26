#include <muos/procmgr.h>

#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleGetPid (RefPtr<Message> message)
{
    struct ProcMgrReply reply;
    Thread * sender = message->GetSender();

    memset(&reply, 0, sizeof(reply));
    reply.payload.getpid.pid = sender->process->GetId();
    message->Reply(ERROR_OK, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_GETPID, HandleGetPid)

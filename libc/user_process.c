#include <sys/procmgr.h>
#include <sys/process.h>

int GetPid (void)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;

    msg.type = PROC_MGR_MESSAGE_GETPID;
    MessageSend(PROCMGR_CONNECTION_ID, &msg, sizeof(msg), &reply, sizeof(reply));

    return reply.payload.getpid.pid;
}

void Exit (void)
{
    struct ProcMgrMessage msg;
    msg.type = PROC_MGR_MESSAGE_EXIT;
    MessageSend(PROCMGR_CONNECTION_ID, &msg, sizeof(msg), &msg, sizeof(msg));
}

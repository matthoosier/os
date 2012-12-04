#include <string.h>

#include <sys/procmgr.h>
#include <sys/process.h>
#include <sys/uio.h>

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

int Spawn (char const path[])
{
    struct iovec msgv[3];
    struct iovec replyv[1];
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    int status;

    msg.type = PROC_MGR_MESSAGE_SPAWN;
    msg.payload.spawn.path_len = strlen(path) + 1;

    msgv[0].iov_base = &msg.type;
    msgv[0].iov_len = offsetof(struct ProcMgrMessage, payload.spawn.path_len) - offsetof(struct ProcMgrMessage, type);

    msgv[1].iov_base = &msg.payload.spawn.path_len;
    msgv[1].iov_len = offsetof(struct ProcMgrMessage, payload.spawn.path) - offsetof(struct ProcMgrMessage, payload.spawn.path_len);

    msgv[2].iov_base = (void *)&path[0];
    msgv[2].iov_len = msg.payload.spawn.path_len * sizeof(char);

    replyv[0].iov_base = &reply;
    replyv[0].iov_len = sizeof(reply);

    status = MessageSendV(PROCMGR_CONNECTION_ID,
                          msgv,
                          sizeof(msgv) / sizeof(msgv[0]),
                          replyv,
                          sizeof(replyv) / sizeof(replyv[0]));

    if (status >= 0) {
        return reply.payload.spawn.pid;
    }
    else {
        return status;
    }
}

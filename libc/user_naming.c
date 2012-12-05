#include <string.h>

#include <sys/message.h>
#include <sys/naming.h>
#include <sys/procmgr.h>

int NameAttach (char const full_path[])
{
    struct iovec msgv[3];
    struct iovec replyv[1];

    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;

    int msg_result;

    msg.type = PROC_MGR_MESSAGE_NAME_ATTACH;
    msg.payload.name_attach.path_len = strlen(full_path) + 1;

    msgv[0].iov_base = &msg.type;
    msgv[0].iov_len = offsetof(struct ProcMgrMessage, payload.name_attach.path_len) - offsetof(struct ProcMgrMessage, type);

    msgv[1].iov_base = &msg.payload.name_attach.path_len;
    msgv[1].iov_len = offsetof(struct ProcMgrMessage, payload.name_attach.path) - offsetof(struct ProcMgrMessage, payload.name_attach.path_len);

    msgv[2].iov_base = (void *)&full_path[0];
    msgv[2].iov_len = msg.payload.name_attach.path_len * sizeof(char);

    replyv[0].iov_base = &reply;
    replyv[0].iov_len = sizeof(reply);

    msg_result = MessageSendV(PROCMGR_CONNECTION_ID,
                              msgv,
                              sizeof(msgv) / sizeof(msgv[0]),
                              replyv,
                              sizeof(replyv) / sizeof(replyv[0]));

    if (msg_result >= 0) {
        return reply.payload.name_attach.channel_id;
    } else {
        return msg_result;
    }
}

int NameOpen (char const full_path[])
{
    struct iovec msgv[3];
    struct iovec replyv[1];

    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    int msg_result;

    msg.type = PROC_MGR_MESSAGE_NAME_OPEN;
    msg.payload.name_open.path_len = strlen(full_path) + 1;

    msgv[0].iov_base = &msg.type;
    msgv[0].iov_len = offsetof(struct ProcMgrMessage, payload.name_open.path_len) - offsetof(struct ProcMgrMessage, type);

    msgv[1].iov_base = &msg.payload.name_open.path_len;
    msgv[1].iov_len = offsetof(struct ProcMgrMessage, payload.name_open.path) - offsetof(struct ProcMgrMessage, payload.name_open.path_len);

    msgv[2].iov_base = (void *)&full_path[0];
    msgv[2].iov_len = msg.payload.name_open.path_len * sizeof(char);

    replyv[0].iov_base = &reply;
    replyv[0].iov_len = sizeof(reply);

    msg_result = MessageSendV(PROCMGR_CONNECTION_ID,
                              msgv,
                              sizeof(msgv) / sizeof(msgv[0]),
                              replyv,
                              sizeof(replyv) / sizeof(replyv[0]));

    if (msg_result >= 0) {
        return reply.payload.name_open.connection_id;
    } else {
        return msg_result;
    }
}

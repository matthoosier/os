#include <muos/error.h>

#include <kernel/kmalloc.h>
#include <kernel/message.hpp>
#include <kernel/nameserver.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleNameAttach (RefPtr<Message> message)
{
    struct ProcMgrReply reply;
    size_t path_len;
    char * path;
    RefPtr<Channel> channel;
    Channel_t channel_id;
    NameRecord * name_record;
    int status;

    message->Read(offsetof(struct ProcMgrMessage,
                           payload.name_attach.path_len),
                  &path_len,
                  sizeof(path_len));

    path = (char *)kmalloc(path_len);

    message->Read(offsetof(struct ProcMgrMessage,
                           payload.name_attach.path),
                  path,
                  path_len);

    try {
        channel.Reset(new Channel());
    }
    catch (std::bad_alloc) {
        status = ERROR_NO_MEM;
        goto cleanup;
    }

    channel_id = message->GetSender()->process->RegisterChannel(channel);

    if (channel_id < 0) {
        channel.Reset();
        status = ERROR_NO_MEM;
        goto cleanup;
    }

    name_record = NameServer::RegisterName(path, channel);

    if (name_record) {
        channel->SetNameRecord(name_record);
        reply.payload.name_attach.channel_id = channel_id;
        status = ERROR_OK;
    }
    else {
        message->GetSender()->process->UnregisterChannel(channel_id);
        channel.Reset();
        status = ERROR_NO_MEM;
    }

cleanup:
    kfree(path, path_len);

    message->Reply(status, &reply, sizeof(reply));
}

static void HandleNameOpen (RefPtr<Message> message)
{
    ProcMgrReply reply;
    size_t path_len;
    char * path;
    size_t n;
    RefPtr<Channel> channel;
    RefPtr<Connection> connection;
    Connection_t connection_id;
    int status;

    message->Read(offsetof(struct ProcMgrMessage,
                           payload.name_open.path_len),
                  &path_len,
                  sizeof(path_len));

    path = (char *)kmalloc(path_len);

    if (!path) {
        connection_id = -1;
        status = ERROR_NO_MEM;
        goto cleanup;
    }

    n = message->Read(offsetof(struct ProcMgrMessage,
                               payload.name_open.path),
                      path,
                      path_len);

    if (n != path_len) {
        connection_id = -1;
        status = ERROR_INVALID;
        goto cleanup;
    }

    channel = NameServer::LookupName(path);

    if (!channel) {
        connection_id = -1;
        status = ERROR_INVALID;
        goto cleanup;
    }

    try {
        connection.Reset(new Connection(channel));
    } catch (std::bad_alloc) {
        connection_id = -1;
        status = ERROR_NO_MEM;
        goto cleanup;
    }

    connection_id = message->GetSender()->process->RegisterConnection(connection);
    assert(connection == message->GetSender()->process->LookupConnection(connection_id));
    reply.payload.name_open.connection_id = connection_id;
    status = ERROR_OK;

    if (connection_id < 0) {
        connection.Reset();
        status = ERROR_NO_MEM;
        goto cleanup;
    }

cleanup:
    if (path) {
        kfree(path, path_len);
    }

    message->Reply(status, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_NAME_ATTACH, HandleNameAttach);
PROC_MGR_OPERATION(PROC_MGR_MESSAGE_NAME_OPEN, HandleNameOpen);

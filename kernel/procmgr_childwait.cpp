#include <sys/error.h>

#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/reaper.hpp>

void HandleInstallWait (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    size_t n;
    RefPtr<Connection> connection;
    RefPtr<Reaper> handler;
    Process * process = message->GetSender()->process;

    n = message->Read(0, IoBuffer(&msg, sizeof(msg)));

    if (n < sizeof(msg)) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    connection = process->LookupConnection(msg.payload.child_wait_attach.connection_id);

    if (!connection) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    try {
        handler = new Reaper(connection,
                             msg.payload.child_wait_attach.child_pid,
                             0);

    } catch (std::bad_alloc) {
        message->Reply(ERROR_NO_MEM, IoBuffer::GetEmpty());
        return;
    }

    reply.payload.child_wait_attach.handler_id = process->RegisterReaper(handler);

    if (reply.payload.child_wait_attach.handler_id < 0) {
        message->Reply(-reply.payload.child_wait_attach.handler_id, IoBuffer::GetEmpty());
        return;
    }

    process->TryReapChildren(handler);

    message->Reply(ERROR_OK, &reply, sizeof(reply));
}

void HandleRemoveWait (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    size_t n;
    Process * process = message->GetSender()->process;

    n = message->Read(0, &msg, sizeof(msg));
    if (n < sizeof(msg)) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    n = process->UnregisterReaper(msg.payload.child_wait_detach.handler_id);

    message->Reply(n == ERROR_OK ? ERROR_OK : -n, IoBuffer::GetEmpty());
}

void HandleAddWaitCount (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    size_t n;
    Process * process = message->GetSender()->process;

    n = message->Read(0, IoBuffer(&msg, sizeof(msg)));

    if (n < sizeof(msg)) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    RefPtr<Reaper> handler = process->LookupReaper(msg.payload.child_wait_arm.handler_id);

    if (!handler) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    handler->mCount += msg.payload.child_wait_arm.count;
    process->TryReapChildren(handler);

    message->Reply(ERROR_OK, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_CHILD_WAIT_ATTACH, HandleInstallWait);
PROC_MGR_OPERATION(PROC_MGR_MESSAGE_CHILD_WAIT_DETACH, HandleRemoveWait);
PROC_MGR_OPERATION(PROC_MGR_MESSAGE_CHILD_WAIT_ARM, HandleAddWaitCount);

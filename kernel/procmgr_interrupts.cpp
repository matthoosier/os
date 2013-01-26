#include <muos/procmgr.h>

#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleInterruptAttach (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    RefPtr<UserInterruptHandler> handler;
    RefPtr<Connection> connection;
    Thread * sender;
    Process * current;

    ssize_t msg_len = PROC_MGR_MSG_LEN(interrupt_attach);
    ssize_t len = message->Read(0, &msg, msg_len);

    if (msg_len != len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    sender = message->GetSender();
    current = sender->process;

    if (!current) {
        message->Reply(ERROR_INVALID, &reply, 0);
        return;
    }

    connection = current->LookupConnection(msg.payload.interrupt_attach.connection_id);

    if (!connection) {
        message->Reply(ERROR_INVALID, &reply, 0);
        return;
    }

    memset(&reply, 0, sizeof(reply));

    try {
        handler.Reset(new UserInterruptHandler());
    } catch (std::bad_alloc) {
        message->Reply(ERROR_NO_MEM, &reply, 0);
        return;
    }

    reply.payload.interrupt_attach.handler_id = current->RegisterInterruptHandler(handler);

    if (reply.payload.interrupt_attach.handler_id < 0) {
        message->Reply(ERROR_NO_MEM, &reply, 0);
        return;
    }

    handler->mHandlerInfo.mIrqNumber = msg.payload.interrupt_attach.irq_number;
    handler->mHandlerInfo.mConnection = connection;
    handler->mHandlerInfo.mPulsePayload = (uintptr_t)msg.payload.interrupt_attach.param;

    InterruptAttachUserHandler(handler);

    // Elevate scheduling priority to reflect interrupt handling
    sender->assigned_priority = Thread::PRIORITY_IO;

    message->Reply(ERROR_OK, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_ATTACH, HandleInterruptAttach)

static void HandleInterruptComplete (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    RefPtr<UserInterruptHandler> handler;
    Process * current = message->GetSender()->process;

    ssize_t msg_len = PROC_MGR_MSG_LEN(interrupt_complete);
    ssize_t len = message->Read(0, &msg, msg_len);

    if (msg_len != len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    handler = current->LookupInterruptHandler(msg.payload.interrupt_complete.handler_id);

    if (handler) {
        message->Reply(InterruptCompleteUserHandler(handler), IoBuffer::GetEmpty());
    }
    else {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_COMPLETE, HandleInterruptComplete)

static void HandleInterruptDetach (RefPtr<Message> message)
{
    struct ProcMgrMessage msg;
    RefPtr<UserInterruptHandler> handler;
    Process * current = message->GetSender()->process;

    ssize_t msg_len = PROC_MGR_MSG_LEN(interrupt_detach);
    ssize_t len = message->Read(0, &msg, msg_len);

    if (msg_len != len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    handler = current->LookupInterruptHandler(msg.payload.interrupt_detach.handler_id);

    if (handler) {
        InterruptDetachUserHandler(handler);
        current->UnregisterInterruptHandler(handler);
        message->Reply(ERROR_OK, IoBuffer::GetEmpty());
    }
    else {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_DETACH, HandleInterruptDetach)

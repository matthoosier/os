#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleInterruptAttach (Message * message)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    struct UserInterruptHandlerRecord * rec;
    Thread * sender;

    ssize_t msg_len = PROC_MGR_MSG_LEN(interrupt_attach);
    ssize_t len = message->Read(0, &msg, msg_len);

    if (msg_len != len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    memset(&reply, 0, sizeof(reply));

    rec = UserInterruptHandlerRecordAlloc();

    if (!rec) {
        message->Reply(ERROR_NO_MEM, &reply, 0);
    } else {
        sender = message->GetSender();

        rec->handler_info.irq_number = msg.payload.interrupt_attach.irq_number;
        rec->handler_info.pid = sender->process->GetId();
        rec->handler_info.coid = msg.payload.interrupt_attach.connection_id;
        rec->handler_info.param = (uintptr_t)msg.payload.interrupt_attach.param;

        InterruptAttachUserHandler(rec);

        sender->assigned_priority = Thread::PRIORITY_IO;

        reply.payload.interrupt_attach.handler = (uintptr_t)rec;

        message->Reply(ERROR_OK, &reply, sizeof(reply));
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_ATTACH, HandleInterruptAttach)

static void HandleInterruptComplete (Message * message)
{
    struct ProcMgrMessage msg;
    struct UserInterruptHandlerRecord * rec;

    ssize_t msg_len = PROC_MGR_MSG_LEN(interrupt_complete);
    ssize_t len = message->Read(0, &msg, msg_len);

    if (msg_len != len) {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
        return;
    }

    rec = (struct UserInterruptHandlerRecord *)msg.payload.interrupt_complete.handler;

    if (rec) {
        message->Reply(InterruptCompleteUserHandler(rec), IoBuffer::GetEmpty());
    }
    else {
        message->Reply(ERROR_INVALID, IoBuffer::GetEmpty());
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_COMPLETE, HandleInterruptComplete)

static void HandleInterruptDetach (Message * message)
{
    /* TODO */
    message->Reply(ERROR_NO_SYS, IoBuffer::GetEmpty());
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_DETACH, HandleInterruptDetach)

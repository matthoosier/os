#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/thread.hpp>

static void HandleInterruptAttach (
        Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct ProcMgrReply reply;
    struct UserInterruptHandlerRecord * rec;
    Thread * sender;

    memset(&reply, 0, sizeof(reply));

    rec = UserInterruptHandlerRecordAlloc();

    if (!rec) {
        message->Reply(ERROR_NO_MEM, &reply, 0);
    } else {
        sender = message->GetSender();

        rec->handler_info.irq_number = buf->payload.interrupt_attach.irq_number;
        rec->handler_info.pid = sender->process->GetId();
        rec->handler_info.coid = buf->payload.interrupt_attach.connection_id;
        rec->handler_info.param = (uintptr_t)buf->payload.interrupt_attach.param;

        InterruptAttachUserHandler(rec);

        sender->assigned_priority = Thread::PRIORITY_IO;

        reply.payload.interrupt_attach.handler = (uintptr_t)rec;

        message->Reply(ERROR_OK, &reply, sizeof(reply));
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_ATTACH, HandleInterruptAttach)

static void HandleInterruptComplete (
        Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct UserInterruptHandlerRecord * rec;

    rec = (struct UserInterruptHandlerRecord *)buf->payload.interrupt_complete.handler;

    if (rec) {
        message->Reply(InterruptCompleteUserHandler(rec), buf, 0);
    }
    else {
        message->Reply(ERROR_INVALID, buf, 0);
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_COMPLETE, HandleInterruptComplete)

static void HandleInterruptDetach (
        Message * message,
        const struct ProcMgrMessage * buf
        )
{
    /* TODO */
    message->Reply(ERROR_NO_SYS, buf, 0);
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_DETACH, HandleInterruptDetach)

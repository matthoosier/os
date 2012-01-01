#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/interrupt-handler.h>
#include <kernel/message.h>
#include <kernel/process.h>
#include <kernel/procmgr.h>
#include <kernel/thread.h>

static void HandleInterruptAttach (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    struct ProcMgrReply reply;
    struct UserInterruptHandlerRecord * rec;

    memset(&reply, 0, sizeof(reply));

    rec = UserInterruptHandlerRecordAlloc();

    if (!rec) {
        KMessageReply(message, ERROR_NO_MEM, &reply, 0);
    } else {
        rec->func = buf->payload.interrupt_attach.func;
        rec->pid = message->sender->process->pid;

        InterruptAttachUserHandler(
                buf->payload.interrupt_attach.irq_number,
                rec
                );

        reply.payload.interrupt_attach.handler = (uintptr_t)rec;

        KMessageReply(message, ERROR_OK, &reply, sizeof(reply));
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_ATTACH, HandleInterruptAttach)

static void HandleInterruptDetach (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    /* TODO */
    KMessageReply(message, ERROR_NO_SYS, buf, 0);
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_INTERRUPT_DETACH, HandleInterruptDetach)

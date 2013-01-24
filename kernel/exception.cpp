#include <sys/compiler.h>
#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/exception.hpp>
#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/thread.hpp>

void ScheduleSelfAbort ()
{
  #if 0
    struct ProcMgrMessage message;
    RefPtr<Connection> connection;

    assert(THREAD_CURRENT()->process != NULL);
    connection = THREAD_CURRENT()->process->LookupConnection(PROCMGR_CONNECTION_ID);

    message.type = PROC_MGR_MESSAGE_SIGNAL;
    message.payload.signal.signalee_pid = THREAD_CURRENT()->process->GetId();

    connection->SendMessage(IoBuffer(&message, sizeof(message)),
                            IoBuffer::GetEmpty());
  #endif

    /* Deallocate current process */
    Process * process = THREAD_CURRENT()->process;
    RefPtr<Connection> con = process->LookupConnection(PROCMGR_CONNECTION_ID);
    con->SendMessageAsync(PULSE_TYPE_CHILD_FINISH, process->GetId());

    Thread::BeginTransaction();
    Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_FINISHED);
    Thread::RunNextThread();
    Thread::EndTransaction();

    assert(false);
}

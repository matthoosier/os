#include <sys/compiler.h>
#include <sys/procmgr.h>

#include <kernel/assert.h>
#include <kernel/exception.hpp>
#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/reaper.hpp>
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
    Thread * thread = THREAD_CURRENT();
    Reaper::Reap(thread->process);

    assert(false);
}

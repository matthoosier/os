#include <new>

#include <sys/spinlock.h>

#include <kernel/list.hpp>
#include <kernel/reaper.hpp>
#include <kernel/process.hpp>
#include <kernel/semaphore.hpp>
#include <kernel/thread.hpp>

static List<Thread, &Thread::queue_link> sDead;
static List<Thread, &Thread::queue_link> sWaitingReapers;

void Reaper::Reap (Process * aProcess)
{
    Thread * thread = aProcess->GetThread();
    Thread::BeginTransaction();

    sDead.Append(thread);

    if (!sWaitingReapers.Empty()) {
        Thread::MakeReady(sWaitingReapers.PopFirst());
    }

    Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_FINISHED);
    Thread::RunNextThread();
    Thread::EndTransaction();
    assert(false);
    while (true) {}
}

void Reaper::Body (void * ignored)
{
    while (true) {
        Thread * thread;
        Process * process;

        Thread::BeginTransaction();
        
        while (sDead.Empty()) {
            sWaitingReapers.Append(THREAD_CURRENT());
            Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_SEM);
            Thread::RunNextThread();
        }

        thread = sDead.PopFirst();
        process = thread->process;

        Thread::EndTransaction();

        process->GetParent()->ReportChildFinished(process);
    }
}

Thread * Reaper::Start ()
{
    return Thread::Create(&Body, NULL);
}

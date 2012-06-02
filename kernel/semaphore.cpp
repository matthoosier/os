#include <sys/spinlock.h>

#include <kernel/semaphore.hpp>
#include <kernel/thread.hpp>

Semaphore::Semaphore (unsigned int count)
    : mCount(count)
{
};

Semaphore::~Semaphore ()
{
}

void Semaphore::Up ()
{
    Thread::BeginTransaction();

    if (mWaitList.Empty()) {
        ++mCount;
    } else {
        Waiter * w = mWaitList.PopFirst();
        w->mReleased = true;
        Thread::MakeReady(w->mThread);
        Thread::MakeReady(THREAD_CURRENT());
        Thread::RunNextThread();
    }

    Thread::EndTransaction();
}

void Semaphore::Down ()
{
    Thread * current = THREAD_CURRENT();

    Thread::BeginTransaction();

    if (mCount < 1) {

        Waiter w(current);
        mWaitList.Append(&w);

        while (!w.mReleased) {
            Thread::MakeUnready(current, Thread::STATE_SEM);
            Thread::RunNextThread();
        }

    } else {
        --mCount;
    }

    Thread::EndTransaction();
}

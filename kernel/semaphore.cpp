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
        w->mState = Waiter::STATE_RELEASED;
        Thread::MakeReady(w->mThread);
        Thread::MakeReady(THREAD_CURRENT());
        Thread::RunNextThread();
    }

    Thread::EndTransaction();
}

void Semaphore::UpDuringException ()
{
    Thread::BeginTransactionDuringException();

    if (mWaitList.Empty()) {
        ++mCount;
    } else {
        Waiter * w = mWaitList.PopFirst();
        w->mState = Waiter::STATE_RELEASED;
        Thread::MakeReady(w->mThread);
        Thread::SetNeedResched();
    }

    Thread::EndTransaction();
}

bool Semaphore::Down (Thread::State aReasonForWait)
{
    Thread * current = THREAD_CURRENT();
    bool ret;

    Thread::BeginTransaction();

    if (mCount < 1) {

        Waiter w(current);
        mWaitList.Append(&w);

        while (w.mState == Waiter::STATE_WAITING) {
            Thread::MakeUnready(current, aReasonForWait);
            Thread::RunNextThread();
        }

        ret = (w.mState == Waiter::STATE_RELEASED);

    } else {
        ret = false;
        --mCount;
    }

    Thread::EndTransaction();

    return ret;
}

void Semaphore::Disarm ()
{
    Thread::BeginTransaction();

    while (!mWaitList.Empty()) {
        mWaitList.PopFirst();
    }

    Thread::EndTransaction();
}

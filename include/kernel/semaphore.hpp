#ifndef __SEMAPHORE_HPP__
#define __SEMAPHORE_HPP__

#include <sys/spinlock.h>

#include <kernel/list.hpp>
#include <kernel/thread.hpp>

/**
 * \brief Implementation of classical sleeping counted semaphore
 *
 * \class Semaphore semaphore.hpp kernel/semaphore.hpp
 */
class Semaphore
{
private:
    /**
     * \class Waiter semaphore.hpp kernel/semaphore.hpp
     */
    class Waiter
    {
    public:
        enum State
        {
            STATE_WAITING,
            STATE_RELEASED,
            STATE_ABORTED,
        };

        Waiter (Thread * who)
            : mThread(who)
            , mState(STATE_WAITING)
        {
        }

    private:
        //! Prevent heap allocation
        void * operator new (size_t size);

        //! Prevent heap allocation
        void operator delete (void * mem);

    public:
        ListElement mLink;
        Thread *    mThread;
        State       mState;
    };

public:
    /**
     * \brief   Initialize semaphore to indicated count
     */
    Semaphore (unsigned int count = 1);

    /**
     * \brief   Deallocate. Nothing special here.
     */
    ~Semaphore ();

    /**
     * \brief   Increase count by one.
     *
     * If any waiters are queued, wake one of them and allow it
     * to consume the new count.
     */
    void Up ();

    /**
     * \brief   Version of Up() to use when running from
     *          interrupt context
     */
    void UpDuringException ();

    /**
     * \brief   Decrease count by one.
     *
     * If the count is zero, then sleep until count becomes
     * nonzero.
     *
     * \return  true if the wait completes successfully, false
     *          if it was aborted by the infrastructure to
     *          facilitate a process termination
     */
    bool Down (Thread::State aReasonForWait = Thread::STATE_SEM);

    /**
     * \brief   Drop all back-pointers
     */
    void Cancel ();

private:
    unsigned int                    mCount;
    bool                            mCanceled;
    List<Waiter, &Waiter::mLink>    mWaitList;
};

#endif /* __SEMAPHORE_HPP__ */

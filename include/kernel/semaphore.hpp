#ifndef __SEMAPHORE_HPP__
#define __SEMAPHORE_HPP__

#include <sys/spinlock.h>

#include <kernel/list.hpp>
#include <kernel/thread.hpp>

/**
 * \brief Implementation of classical sleeping counted semaphore
 */
class Semaphore
{
private:
    class Waiter
    {
    public:
        Waiter (Thread * who)
            : mThread(who)
            , mReleased(false)
        {
        }

    public:
        ListElement mLink;
        Thread *    mThread;
        bool        mReleased;
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
    void Up();

    /**
     * \brief   Decrease count by one.
     *
     * If the count is zero, then sleep until count becomes
     * nonzero.
     */
    void Down();

private:
    unsigned int                    mCount;
    List<Waiter, &Waiter::mLink>    mWaitList;
};

#endif /* __SEMAPHORE_HPP__ */

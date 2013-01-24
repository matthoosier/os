#ifndef __REAPER_HPP__
#define __REAPER_HPP__

#include <kernel/message.hpp>
#include <kernel/process-types.h>
#include <kernel/smart-ptr.hpp>

/**
 * A handler installed by userspace that keeps track of
 * a child process for whom the owner is willing to perform
 * deallocation.
 */
class Reaper : public RefCounted
{
public:
    /**
     * Do not stack-allocate
     *
     * @param connection    see mConnection
     * @param pid           see mPid
     * @param count         see mCount
     */
    Reaper (RefPtr<Connection> aConnection,
            Pid_t aPid,
            unsigned int aCount)
        : mPid(aPid)
        , mConnection(aConnection)
        , mCount(aCount)
    {
    }

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(Reaper));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    /**
     * Test whether this Reaper is configured to
     * be able to reap a process whose Pid_t is <tt>aPid</tt>
     */
    bool Handles (Pid_t aPid)
    {
        if (mPid == aPid || mPid == ANY_PID) {
            return true;
        }
        else {
            return false;
        }
    }

private:
    //! Only RefPtr will be allowed to run dtor
    virtual ~Reaper ()
    {
    }

    //!< Prevent allocating arrays of Reapers
    void * operator new[] (size_t);

    //!< Prevent allocating arrays of Reapers
    void operator delete[] (void *);

public:
    /**
     * Unique identifier
     */
    int mId;

    /**
     * Intrusive list pointer
     */
    ListElement mLink;

    /**
     * Identifier (or #ANY_PID) of child process to wait for
     */
    Pid_t mPid;

    /**
     * Connection on which a pulse with type PULSE_TYPE_CHILD_FINISH
     * will be delivered when the indicated process is reaped.
     */
    RefPtr<Connection> mConnection;

    /**
     * Number of children that userspace has indicated it's prepared
     * to be reaped. Can be increased with the ChildWaitArm() call.
     */
    unsigned int mCount;

    /**
     * Allocates instances of Reaper
     */
    static SyncSlabAllocator<Reaper> sSlab;

    friend class RefPtr<Reaper>;
};

#endif /* __REAPER_HPP__ */

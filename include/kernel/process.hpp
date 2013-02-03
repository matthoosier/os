#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <new>

#include <muos/decls.h>
#include <muos/error.h>
#include <muos/spinlock.h>

#include <kernel/address-space.hpp>
#include <kernel/assert.h>
#include <kernel/interrupt-handler.hpp>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/process-types.h>
#include <kernel/reaper.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

class Thread;

class Process;

/**
 * \brief   Process control block implementation
 *
 * Classical protected-memory process implementation. Processes have
 * a single thread of execution (whose kernel task is stored in
 * \a thread).
 *
 * \class Process process.hpp kernel/process.hpp
 */
class Process
{
public:
    typedef TreeMap<Channel_t, Channel *>       IdToChannelMap_t;
    typedef TreeMap<Connection_t, Connection *> IdToConnectionMap_t;
    typedef TreeMap<int, Message *>             IdToMessageMap_t;
    typedef TreeMap<Pid_t, Process *>           PidMap_t;
    typedef TreeMap<int, UserInterruptHandler *>    IdToInterruptHandlerMap_t;

public:
    /**
     * \brief   Name of program
     */
    char const * GetName ();

    /**
     * \brief   Fetches the value of the 'pagetable' field on a process object.
     */
    TranslationTable * GetTranslationTable ();

    /**
     * \brief   Fetches the address space (if any) on this process
     */
    AddressSpace * GetAddressSpace ();

    /**
     * \brief   Start up the \c procmgr thread
     *
     * Called one time at startup to initialize the privileged kernel thread
     * that implements the responses to basic process-service message API.
     */
    static Process * StartManager ();

    /**
     * \brief   Fetch the \c procmgr thread
     *
     * Called any time after startup, to fetch the privileged kernel running
     * the ProcMgr.
     */
    static Process * GetManager ();

    /**
     * \brief   Spawn a process from the named executable image
     */
    static Process * Create (const char aExecutableName[],
                             Process * aParent);

    /**
     * \brief   Register a new process in the reverse mapping
     *
     * \return  Any previous process that was known by that PID
     */
    static Process * Register(Pid_t pid, Process * process);

    /**
     * \brief   Fetch the process whose identifier is \a pid
     */
    static Process * Lookup (Pid_t pid);

    /**
     * \brief   Remove a process from the reverse mapping
     *
     * \return  The process that was found mapped to key
     */
    static Process * Remove (Pid_t pid);

    /**
     * \brief   Fetch the identifier of this process
     */
    Pid_t GetId ();

    Channel_t RegisterChannel (RefPtr<Channel> c);

    int UnregisterChannel (Channel_t id);

    RefPtr<Channel> LookupChannel (Channel_t id);

    Connection_t RegisterConnection (RefPtr<Connection> c);

    int UnregisterConnection (Connection_t id);

    RefPtr<Connection> LookupConnection (Connection_t id);

    Message_t RegisterMessage (RefPtr<Message> m);

    int UnregisterMessage (Message_t id);

    RefPtr<Message> LookupMessage (Message_t id);

    int RegisterInterruptHandler (RefPtr<UserInterruptHandler> h);

    int UnregisterInterruptHandler (int handler_id);

    RefPtr<UserInterruptHandler> LookupInterruptHandler (int handler_id);

    int RegisterReaper (RefPtr<Reaper> r);

    int UnregisterReaper (int handler_id);

    RefPtr<Reaper> LookupReaper (int handler_id);

public:
    /**
     * \brief   Instances are allocated from a slab
     */
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(Process));
        return sSlab.AllocateWithThrow();
    }

    /**
     * \brief   Instances are allocated from a slab
     */
    void operator delete (void * mem) throw ()
    {
        return sSlab.Free(mem);
    }

    /**
     * \brief   Tear down a process instance
     */
    virtual ~Process ();

    /**
     * \brief   Get the thread executing inside this process
     */
    Thread * GetThread ();

    Process * GetParent ();

    void TryReapChildren (RefPtr<Reaper> aReaper);

    void ReportChildFinished (Process * aChild);

    void ReapChild (Process * aChild, RefPtr<Connection> aConnection);

private:
    /**
     * \brief   Hidden to prevent the general public from making
     *          instances
     */
    Process (char const aComm[], Process * aParent);

private:
    /**
     * \brief   Find a handler that's willing to reap the
     *          indicated child process.
     */
    RefPtr<Reaper> GetReaperForChild (Pid_t id);

private:
    /**
     * \brief   Hidden to prevent allocating arrays
     */
    void * operator new[] (size_t);

    /**
     * \brief   Hidden to prevent allocating arrays
     */
    void operator delete[] (void *);

    /**
     * \brief   Factory for handling loading ELF image into a freshly spawned
     *          kernel thread
     *
     * Returns NULL if the requested executable can't be found or
     * allocation of memory to back it fails.
     */
    static Process * execIntoCurrent (const char aExecutableName[],
                                      Process * aParent) throw (std::bad_alloc);

    /**
     * \brief   Function executed as main body of the kernel thread backing
     *          a user process.
     */
    static void UserProcessThreadBody (void *);

    /**
     * \brief   Function executed as main body of the Process Manager thread
     */
    static void ManagerThreadBody (void *);

private:
    /**
     * \brief   Allocates instances of Process
     */
    static SyncSlabAllocator<Process> sSlab;

    /**
     * \brief   Allows reverse lookup of integer PIDs to
     *          processes
     */
    static PidMap_t sPidMap;

    /**
     * \brief   Protects access to sPidMap
     */
    static Spinlock_t sPidMapSpinlock;

    /**
     * \brief   Synchronization for this Process instance
     */
    Spinlock_t lock;

    /**
     * \brief   All the virtual memory mappings for this process
     */
    ScopedPtr<AddressSpace> mAddressSpace;

    /**
     * \brief   Initial program counter for the program image loaded
     *          into this process
     */
    VmAddr_t entry;

    /**
     * \brief   Descriptive name of this process
     */
    char comm[16];

    /**
     * \brief   Kernel execution context for this process's system
     *          calls
     */
    Thread * thread;

    /**
     * \brief   Simple integer unique identifier for this process
     */
    Pid_t pid;

    /**
     * \brief   Map integer handles to one of the Channel data
     *          structures owned by this process
     */
    ScopedPtr<IdToChannelMap_t> id_to_channel_map;

    /**
     * \brief   Value of the next handle that will be assigned
     *          to a Channel structure owned by this process
     */
    Channel_t next_chid;

    /**
     * \brief   Map integer handles to one of the Connection data
     *          structures owned by this process
     */
    ScopedPtr<IdToConnectionMap_t> id_to_connection_map;

    /**
     * \brief   Value of the next handle that will be assigned
     *          to a Connection structure owned by this process
     */
    Connection_t next_coid;

    /**
     * \brief   Map integer handles to one of the Message data
     *          structures owned by this process
     */
    ScopedPtr<IdToMessageMap_t> id_to_message_map;

    /**
     * \brief   Value of the next handle that will be assigned
     *          to a Message structure owned by this process
     */
    int next_msgid;

    /**
     * \brief   Map integer handles to one of the UserInterruptHandler
     *          data structure owned by this proces
     */
    ScopedPtr<IdToInterruptHandlerMap_t> id_to_interrupt_handler_map;

    /**
     * \brief   Value of the next handle that will be assigned
     *          to a UserInterruptHandler owned by this process
     */
    int next_interrupt_handler_id;

    /**
     * \brief   Map integer handles to one of the Reaper
     *          data structures owned by this process.
     */
    RefList<Reaper, &Reaper::mLink> mReapers;

    /**
     * \brief   Value of the next handle that will be assigned
     *          to a Reaper owned by this process
     */
    int next_child_wait_handler_id;

    /**
     * \brief   Instrusive node for use in maintaining list of
     *          children
     */
    ListElement mChildrenLink;

    /**
     * \brief   List of all the children spawned by this process
     */
    List<Process, &Process::mChildrenLink> mAliveChildren;

    /**
     * \brief   List of all the terminated children spawned by
     *          this process
     */
    List<Process, &Process::mChildrenLink> mDeadChildren;

    /**
     * \brief   Process that spawned this one
     *
     * Or, if the immediate parent has already terminated),
     * <tt>init</tt>.
     */
    Process * mParent;
};

BEGIN_DECLS

extern TranslationTable * ProcessGetTranslationTable (Process *);

END_DECLS

#endif /* __PROCESS_H__ */

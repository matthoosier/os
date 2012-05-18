#ifndef __THREAD_H__
#define __THREAD_H__

#include <sys/arch.h>
#include <sys/decls.h>

#include <kernel/list.hpp>
#include <kernel/process.hpp>

#define ALIGNED_THREAD_STRUCT_SIZE                                  \
    /* Padded out to multiple of 8 to preserve %sp requirements */  \
    (ALIGN(sizeof(struct Thread), 3))

#define THREAD_STRUCT_FROM_SP(_sp)                                  \
    (                                                               \
    (struct Thread *)                                               \
    (((_sp) & PAGE_MASK) + PAGE_SIZE - ALIGNED_THREAD_STRUCT_SIZE)  \
    )

/*
We leverage knowledge that the kernel stack is only one page long, to
be able to compute the address of the current thread's struct based
solely on the current stack pointer.

Don't blow your thread stack! This will return a bad result.
*/
#define THREAD_CURRENT() \
    (THREAD_STRUCT_FROM_SP(CURRENT_STACK_POINTER()))

/*
The main control and saved-state for kernel threads. Each thread's
instance of this structure is housed inside the top of the VM page
used for the thread's stack. This avoids object allocations and also
makes deducing the current thread easy: just compute the right offset
in the page containing the current stack pointer.
*/
class Thread
{
public:
    enum State
    {
        STATE_SEND,
        STATE_REPLY,
        STATE_RECEIVE,

        STATE_READY,
        STATE_RUNNING,

        STATE_FINISHED,

        /* This isn't a state, just a way to programatically calculate */
        STATE_COUNT,
    };

    enum Priority
    {
        PRIORITY_NORMAL  = 0,
        PRIORITY_IO,

        PRIORITY_COUNT,
    };

    typedef void (*Func)(void * param);

public:
    uint32_t    registers[REGISTER_COUNT];

    struct
    {
        void *  ceiling;
        void *  base;

        /* If non-NULL, stack was dynamically allocated */
        Page * page;
    } kernel_stack;

    State state;

    struct Process * process;

    /* For use in scheduling queues. */
    ListElement queue_link;

    /* Thread that will wait for and reap this one */
    Thread * joiner;

    /* "Natural" priority of this thread. */
    Priority assigned_priority;

    /* Ceiling of the priorities of all threads blocked by this one. */ 
    Priority effective_priority;

public:

    static Thread * Create (
            Func body,
            void * param
            );

    /**
     * Deallocates resources used by @thread. Must not be called while @thread
     * is executing on the processor.
     */
    void Join ();

    /**
     * For use in implementing priority inheritance; install an artifically higher
     * priority for this thread than its natural one.
     */
    void SetEffectivePriority (Thread::Priority priority);

    /**
     * Yield to some other runnable thread. Must not be called with interrupts
     * disabled.
     */
    static void YieldNoRequeue ();

    /**
     * Yield to some other runnable thread, and automatically mark the
     * current thread as ready-to-run.
     */
    static void YieldWithRequeue ();

    static void AddReady (Thread * thread);

    static void AddReadyFirst (Thread * thread);

    static Thread * DequeueReady (void);

    static void SetNeedResched (void);

    static bool GetNeedResched (void);

    static bool ResetNeedResched (void);

private:
    /**
     * \brief   Hidden to prevent static or stack allocation
     */
    Thread ();

    /**
     * \brief   Hidden to prevent copying
     */
    Thread (const Thread & other);

    /**
     * \brief   Hidden to prevent assignment
     */
    const Thread& operator= (const Thread & other);
};

/*----------------------------------------------------------
Convenience routines for use from assembly code
----------------------------------------------------------*/

BEGIN_DECLS

/**
 * A version of THREAD_STRUCT_FROM_SP(), but implemented as a symbol
 * for calling from places where macros aren't available (e.g., assembly).
 */
extern Thread * ThreadStructFromStackPointer (uint32_t sp);

extern void ThreadAddReady(struct Thread *);
extern struct Thread * ThreadDequeueReady (void);

extern bool ThreadResetNeedResched (void);

extern void ThreadSetStateReady (struct Thread *);
extern void ThreadSetStateRunning (struct Thread *);
extern void ThreadSetStateSend (struct Thread *);
extern void ThreadSetStateReply (struct Thread *);
extern void ThreadSetStateReceive (struct Thread *);
extern void ThreadSetStateFinished (struct Thread *);

END_DECLS

#endif /* __THREAD_H__ */

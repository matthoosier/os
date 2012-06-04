#ifndef __THREAD_H__
#define __THREAD_H__

#include <sys/arch.h>
#include <sys/decls.h>

#include <kernel/list.hpp>
#include <kernel/process.hpp>

#define ALIGNED_THREAD_STRUCT_SIZE                                  \
    /* Padded out to multiple of 8 to preserve %sp requirements */  \
    (ALIGN(sizeof(struct Thread), 3))

/**
 * \brief   Given a stack pointer value, compute the #ThreadPtr to
 *          which that stack pointer belongs. This is an
 *          implementation detail of THREAD_CURRENT().
 *
 * \memberof    Thread
 */
#define THREAD_STRUCT_FROM_SP(_sp)                                  \
    (                                                               \
    (struct Thread *)                                               \
    (((_sp) & PAGE_MASK) + PAGE_SIZE - ALIGNED_THREAD_STRUCT_SIZE)  \
    )

/**
 * \brief   Fetch #ThreadPtr of the currently executing thread.
 *
 * We leverage knowledge that the kernel stack is only one page long, to
 * be able to compute the address of the current thread's struct based
 * solely on the current stack pointer.
 *
 * Don't blow your thread stack! This will return a bad result.
 *
 * \memberof    Thread
 */
#define THREAD_CURRENT() \
    (THREAD_STRUCT_FROM_SP(CURRENT_STACK_POINTER()))

/**
 * \brief   The main control and saved-state information for kernel threads.
 *
 * Each thread's instance of this structure is housed inside the top of
 * the VM page used for the thread's stack. This avoids object allocations
 * and also makes deducing the current thread easy: just compute the right
 * offset in the page containing the current stack pointer.
 */
class Thread
{
public:
    enum State
    {
        STATE_SEND,
        STATE_REPLY,
        STATE_RECEIVE,

        STATE_SEM,

        STATE_READY,
        STATE_RUNNING,

        STATE_JOINING,
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

    /**
     * \brief   Signature of functions that are supplied as thread bodies
     */
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
     * \brief   Deallocates resources used by thread a thread.
     *
     * Must not be called while thread is actively executing on the processor.
     */
    void Join ();

    /**
     * \brief   For use in implementing priority inheritance
     *
     * Installs an artifically higher priority for this thread than
     * its natural one.
     */
    void SetEffectivePriority (Thread::Priority priority);

    /**
     * \brief   Remove argument from ready-to-run list
     *
     * Assumes #sched_spinlock is held.
     */
    static void MakeUnready (Thread * thread, State state);

    /**
     * \brief   Add argument to ready-to-run list
     *
     * Assumes #sched_spinlock is held.
     */
    static void MakeReady (Thread * thread);

    /**
     * Assumes #sched_spinlock is held.
     */
    static void RunNextThread ();

    /**
     * Assumes #sched_spinlock is held.
     */
    static Thread * DequeueReady ();

    static void SetNeedResched ();

    static bool GetNeedResched ();

    static bool ResetNeedResched ();

    static void BeginTransaction ();
    static void BeginTransactionDuringIrq ();
    static void EndTransaction ();

private:
    /**
     * \brief   Hidden to prevent static or stack allocation. Use
     *          Create() instead.
     */
    Thread ();

    /**
     * \brief   Hidden to prevent copying. Use Join() from a different
     *          thread's execution to deallocate a thread.
     */
    Thread (const Thread & other);

    /**
     * \brief   Hidden to prevent assignment
     */
    const Thread& operator= (const Thread & other);

    /**
     *
     */
    static void Entry (Func func, void * func_arg);

private:
    State state;
};

/**
 * \brief Type alias for conciseness in writing macro documentation
 *
 * \memberof Thread
 */
typedef Thread * ThreadPtr;

/*----------------------------------------------------------
Convenience routines for use from assembly code
----------------------------------------------------------*/

BEGIN_DECLS

/**
 * A version of THREAD_STRUCT_FROM_SP(), but implemented as a symbol
 * for calling from places where macros aren't available (e.g., assembly).
 */
extern Thread *     ThreadStructFromStackPointer    (uint32_t sp);

extern Process *    ThreadGetProcess                (Thread *);

extern void         ThreadMakeReady                 (Thread *);
extern Thread *     ThreadDequeueReady              (void);
extern void         ThreadBeginTransaction          (void);
extern void         ThreadBeginTransactionDuringIrq (void);
extern void         ThreadBeginTransactionEndingIrq (void);
extern void         ThreadEndTransaction            (void);
extern void         ThreadEndTransactionFromRestart (void);

extern bool         ThreadResetNeedResched          (void);

END_DECLS

#endif /* __THREAD_H__ */

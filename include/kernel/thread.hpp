#ifndef __THREAD_H__
#define __THREAD_H__

#include <sys/arch.h>
#include <sys/decls.h>

#include <kernel/list.hpp>
#include <kernel/process.hpp>
#include <kernel/smart-ptr.hpp>

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
 * \brief   Task control block implementation, and core scheduler
 *          mechanism.
 *
 * Threads in the operating system can either be pure kernel-space
 * entities or can be the stream of control for a classical protected-
 * memory Process. If belonging to a process, then the kernel-space
 * stack is used to host the supervisor-mode backend of system calls.
 *
 * Each thread's instance of this structure is housed inside the top of
 * the VM page used for the thread's kernel stack. This avoids object allocations
 * and also makes deducing the current thread easy: just compute the right
 * offset in the page containing the current stack pointer.
 *
 * To perform a scheduling operation (anything that manipulates the
 * runlists or calls RunNextThread), a systemwide scheduler-protecting
 * lock must be acquired. For example:
 *
 * \code
 * void WaitForProcessTerminated (Process * p) {
 *
 *   Process::WaitRecord r;
 *   r.waiter = THREAD_CURRENT();
 *
 *   // Add myself into p's wait list
 *   p->addTerminationWaiter(&r);
 *
 *   Thread::BeginTransaction();
 *   while (!p->terminated) {
 *     // Remove myself from the runlist
 *     Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_SOMETHING_OR_OTHER);
 *
 *     // Choose and switch into next task. Because I took myself out of the
 *     // run queue above, this will cause me to sleep until some other thread
 *     // performing the code that terminates p removes me from the
 *     // wait-list above and calls Thread::MakeReady() on me.
 *     Thread::RunNextThread();
 *
 *     // For control to reach here, somebody has removed us from p's wait-list
 *     // and made us runnable again.
 *   }
 *   Thread::EndTransaction();
 * }
 * \endcode
 */
class Thread : public WeakPointee
{
public:
    /**
     * \brief   Scheduling status of a task
     */
    enum State
    {
        STATE_SEND,     //!<    Blocked waiting to send a message
        STATE_REPLY,    //!<    Blocked waiting for a reply to a message
        STATE_RECEIVE,  //!<    Blocked waiting to receive a message

        STATE_SEM,      //!<    Blocked waiting on a semaphore

        STATE_READY,    //!<    Ready to run
        STATE_RUNNING,  //!<    Currently using the CPU

        STATE_JOINING,  //!<    Blocked waiting on some other thread to finish
        STATE_FINISHED, //!<    Done, waiting to be reaped

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
     * \brief   Basic data about a stack on which a thread runs
     */
    struct StackData
    {
        /**
         * \brief   Lowest addressable word contained in the stack
         *          (inclusive).
         *
         * The address in virtual memory of the lowest numerical
         * word which belongs to this stack.
         */
        void * base;

        /**
         * \brief   Highest addressable word contained in the
         *          stack (exclusive).
         *
         * The address in virtual memory of the first word which
         * lives just beyond the highest-addressed word contained
         * in this stack.
         */
        void * ceiling;

        /**
         * \brief   Pointer of the page in which the stack lives
         *
         * That is: if non-NULL, stack was dynamically allocated
         */
        Page * page;
    };

    /**
     * \brief   Signature of functions that are supplied as thread bodies
     */
    typedef void (*Func)(void * param);

public:
    /**
     * \brief   Saved contents of kernel-mode registers
     */
    uint32_t    k_reg[REGISTER_COUNT];

    /**
     * \brief   Saved contents of user-mode registers
     */
    uint32_t    u_reg[REGISTER_COUNT];

    /**
     * \brief   Metadata about the stack owned by this thread
     */
    StackData   kernel_stack;

    /**
     * \brief   Owner (if any) of the process
     *
     * The Process referenced by this point is not owned by this
     * thread. Teardown code should not attempt to deallocate
     * \a process during reclamation of the Thread object.
     */
    Process * process;

    /* For use in scheduling queues. */
    ListElement queue_link;

    /* "Natural" priority of this thread. */
    Priority    assigned_priority;

    /* Ceiling of the priorities of all threads blocked by this one. */
    Priority    effective_priority;

public:

    /**
     * \brief   Allocate and begin execution of a new thread
     *
     * \param body  The function which will be executed as the
     *              main lifetime of the new thread
     *
     * \param param The sole parameter passed to \a body
     *
     * Execution of the new thread is asynchronous with respect
     * to the spawner. That is, \a body may or may not have begun
     * executing by the time control returns to the spawner and
     * Create() offers back its return value.
     */
    static Thread * Create (
            Func body,
            void * param
            );

    /**
     * \brief   Patch up the data structure of a statically allocated
     *          thread.
     *
     * This enables after-the-fact decoration of the initial thread of control
     * in the system (whose stack is not dynamically allocated) so that it can
     * survive being put to sleep and inserted into runqueues.
     *
     * \param stack_base
     *          Lowest addressable word in the thread's stack
     * \param stack_ceiling
     *          One word higher than the highest addressable word
     *          in the thread's stack
     */
    static void DecorateStatic (
            Thread * thread,
            VmAddr_t stack_base,
            VmAddr_t stack_ceiling
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
     * Must be performed under the protection of the Thread::BeginTransaction()
     * lock.
     */
    static void MakeUnready (Thread * thread, State state);

    /**
     * \brief   Add argument to ready-to-run list
     *
     * Must be performed under the protection of the Thread::BeginTransaction()
     * lock.
     */
    static void MakeReady (Thread * thread);

    /**
     * \brief   Select and run next thread.
     *
     * This may be a no-op if the current thread is the highest priority
     * runnable task in the system.
     *
     * Must be performed under the protection of the Thread::BeginTransaction()
     * lock.
     */
    static void RunNextThread ();

    /**
     * \brief   Select and remove a thread from the runlist.
     *
     * Must be performed under the protection of the Thread::BeginTransaction()
     * lock.
     */
    static Thread * DequeueReady ();

    /**
     * \brief   Inform the scheduler that some event has happened
     *          that may have added or removed tasks to the runlist.
     *
     * This is useful from interrupt handlers and syscalls, to cause
     * the scheduling algorithm to be re-run as the kernel side of
     * a syscall or IRQ handling unwinds.
     */
    static void SetNeedResched ();

    /**
     * \brief   Query and clear the scheduler flag set by SetNeedResched()
     */
    static bool GetNeedResched ();

    /**
     * \brief   Clear the scheduler flag set by SetNeedResched()
     */
    static bool ResetNeedResched ();

    /**
     * \brief   Begin a scheduling transaction
     *
     * This acquires the global singleton lock that guards access to
     * the runlists during the core context-switching logic. This is
     * used to implement SMP-safe synchronization primitives.
     *
     * Must not be called from interrupt context or with interrupts
     * disabled. Note that this implies no spinlocks may be held when
     * called.
     */
    static void BeginTransaction ();

    /**
     * \brief   Version of BeginTransaction() for use from interrupt
     *          context
     *
     * This version of BeginTransaction() is safe to use when interrupt
     * handling code needs to manipulate runqueues.
     */
    static void BeginTransactionDuringIrq ();

    /**
     * \brief   Finish a scheduling transaction
     *
     * This releases the global singleton lock that guards access to
     * runlists during the core context-switching logic.
     */
    static void EndTransaction ();

private:
    /**
     * \brief   Hidden to prevent static or stack allocation. Use
     *          DecorateStatic() instead.
     */
    Thread (VmAddr_t stack_base, VmAddr_t stack_ceiling);

    /**
     * \brief   Hidden to prevent static or stack allocation. Use
     *          Create() instead.
     */
    Thread (Page * stack_page);

    /**
     * \brief   Hidden to prevent copying. Use Join() from a different
     *          thread's execution to deallocate a thread.
     */
    Thread (const Thread & other);

    /**
     * \brief   Hidden to prevent external deallocation of threads
     *
     * Even though it's buried in private scope and Thread instances
     * are never deallocated with the normal 'delete' operator, the
     * destructor is still useful as a controlled way to automatically
     * tear down member variables during thread reclamation.
     */
    ~Thread ();

    /**
     * \brief   Hidden to prevent assignment
     */
    const Thread& operator= (const Thread & other);

    /**
     * \brief   Entrypoint for all new threads
     */
    static void Entry (Func func, void * func_arg);

private:
    /**
     * \brief   Scheduling status of this task
     */
    State state;

    /**
     * \brief   Thread that will wait for and reap this one
     *
     * If NULL, then no other thread has yet called Join() on this
     * thread.
     */
    Thread * joiner;

    friend void AsmOffsetsMain ();
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

/**
 * \brief   C version of MakeReady() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadMakeReady                 (Thread *);

/**
 * \brief   C version of MakeReady() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern Thread *     ThreadDequeueReady              (void);

/**
 * \brief   C version of BeginTransaction() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadBeginTransaction          (void);

/**
 * \brief   C version of BeginTransactionDuringIrq() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadBeginTransactionDuringIrq (void);

/**
 * \brief   Analagous to BeginTransaction() for use in interrupt handling
 *          code implementing preemption
 *
 * Rather than use the usual SpinlockLock() to grab the global scheduler
 * lock, it uses SpinlockLockNoIrqSave() so that when released, the lock
 * will re-enable interrupts.
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadBeginTransactionEndingIrq (void);

/**
 * \brief   C version of EndTransaction() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadEndTransaction            (void);

/**
 * \brief   Analagous to EndTransaction() for use in special
 *          restart code used to jump back into a task which was
 *          preempted.
 *
 * Like EndTransaction(), this function drops the spinlock held on
 * the global scheduler data structures. But in order to ensure the
 * atomicity of restarting a task that was booted due to preemption,
 * interrupts are NOT re-enabled automatically when the scheduler
 * spinlock is released. The caller is responsible for re-enabling
 * interrupts sometime later.
 *
 * \memberof Thread
 * \private
 */
extern void         ThreadEndTransactionFromRestart (void);

/**
 * \brief   C version of ResetNeedResched() for use from assembly code
 *
 * \memberof Thread
 * \private
 */
extern bool         ThreadResetNeedResched          (void);

END_DECLS

#endif /* __THREAD_H__ */

#include <stdlib.h>

#include <sys/arch.h>
#include <sys/spinlock.h>

#include <kernel/assert.h>
#include <kernel/thread.hpp>
#include <kernel/vm.hpp>

/**
 * \brief   Coordinates context switches. All scheduler-related API
 *          calls must be made with this lock already held.
 *
 * \memberof Thread
 */
BEGIN_DECLS
Spinlock_t sched_spinlock = SPINLOCK_INIT;
END_DECLS

typedef List<Thread, &Thread::queue_link> Queue_t;

static Queue_t normal_ready_queue;
static Queue_t io_ready_queue;

static inline Queue_t * queue_for_thread (Thread * t)
{
    if (t->assigned_priority == Thread::PRIORITY_IO || t->effective_priority == Thread::PRIORITY_IO) {
        return &io_ready_queue;
    }
    else {
        return &normal_ready_queue;
    }
}

void Thread::BeginTransaction ()
{
    ThreadBeginTransaction();
}

void Thread::BeginTransactionDuringIrq ()
{
    ThreadBeginTransactionDuringIrq();
}

void Thread::EndTransaction ()
{
    ThreadEndTransaction();
}

static void SwitchTo (
        Thread * outgoing,
        Thread * incoming
        )
{
    uint32_t next_pc = 0;               /* Used by assembly fragment    */
    uint32_t cpsr_temp = 0;             /* Used by assembly fragment    */

    TranslationTable * incoming_tt = incoming->process
            ? incoming->process->pagetable
            : NULL;

    /*
    Enforce the requirement that this function is called under protection
    of the scheduler lock.
    */
    assert(SpinlockLocked(&sched_spinlock));

    /* Only flushes TLB if the new data structure isn't the same as the old one */
    TranslationTable::SetUser(incoming_tt);

    asm volatile(
        "                                                   \n\t"
        "save_outgoing$:                                    \n\t"
        "    /* Store normal registers */                   \n\t"
        "    stm %[p_outgoing], {r0 - r15}                  \n\t"
        "                                                   \n\t"
        "    /* Store CPSR modulo the IRQ mask */           \n\t"
        "    mrs %[cpsr_temp], cpsr                         \n\t"
        "    str %[cpsr_temp], [%[p_outgoing_cpsr], #0]     \n\t"
        "                                                   \n\t"
        "    /* Patch up stored PC to be at resume point */ \n\t"
        "    ldr %[next_pc], =resume$                       \n\t"
        "    str %[next_pc], [%[p_saved_pc], #0]            \n\t"
        "                                                   \n\t"
        "restore_incoming$:                                 \n\t"
        "    /* Restore saved CPSR into SPSR */             \n\t"
        "    ldr %[cpsr_temp], [%[p_incoming_cpsr], #0]     \n\t"
        "    msr spsr, %[cpsr_temp]                         \n\t"
        "                                                   \n\t"
        "    /* Atomically load normal regs and     */      \n\t"
        "    /* transfer SPSR into CPSR             */      \n\t"
        "    ldm %[p_incoming], {r0 - r15}^                 \n\t"
        "                                                   \n\t"
        "resume$:                                           \n\t"
        "    nop                                            \n\t"
        "    nop                                            \n\t"
        "                                                   \n\t"
        : [next_pc] "+r" (next_pc),
          [cpsr_temp] "+r" (cpsr_temp)
        : [p_outgoing] "r" (&outgoing->k_reg),
          [p_incoming] "r" (&incoming->k_reg),
          [p_saved_pc] "r" (&outgoing->k_reg[REGISTER_INDEX_PC]),
          [p_outgoing_cpsr] "r" (&outgoing->k_reg[REGISTER_INDEX_PSR]),
          [p_incoming_cpsr] "r" (&incoming->k_reg[REGISTER_INDEX_PSR])
        : "memory"
    );
}

void Thread::RunNextThread ()
{
    assert(SpinlockLocked(&sched_spinlock));

    Thread * next = DequeueReady();
    Thread * curr = THREAD_CURRENT();

    if (next != curr) {
        SwitchTo(curr, next);
    } else {
        volatile int x = 0;
        x = x;
    }
}

void Thread::Entry (Thread::Func func, void * param)
{
    /*
    This function is reached by a SwitchTo() call, so this means that
    the thread executing it still holds the scheduler-transaction lock.

    So we have to release that in order to indicate that we're fully
    on the CPU and done messing with scheduler data structures.
    */
    EndTransaction();

    /*
    Main execution is begin, run the user-supplied body for this thread.
    */
    func(param);

    BeginTransaction();

    if (THREAD_CURRENT()->joiner != NULL) {
        MakeReady(THREAD_CURRENT()->joiner);
    }

    MakeUnready(THREAD_CURRENT(), STATE_FINISHED);
    RunNextThread();

    EndTransaction();
}

Thread * Thread::Create (Thread::Func body, void * param)
{
    Page *          stack_page;
    Thread *        descriptor;

    stack_page = Page::Alloc();

    if (!stack_page) {
        /* No memory available to allocate stack */
        return NULL;
    }

    /*
    Carve the thread struct out of the beginning (high addresses)
    of the kernel stack.
    */
    descriptor = THREAD_STRUCT_FROM_SP(stack_page->base_address);

    memset(&descriptor->k_reg[0], 0, sizeof(descriptor->k_reg));

    descriptor->kernel_stack.ceiling = descriptor;
    descriptor->kernel_stack.base = (void *)stack_page->base_address;
    descriptor->kernel_stack.page = stack_page;
    descriptor->process = THREAD_CURRENT()->process;
    descriptor->queue_link.DynamicInit();
    descriptor->state = Thread::STATE_READY;
    descriptor->joiner = NULL;
    descriptor->assigned_priority = Thread::PRIORITY_NORMAL;
    descriptor->effective_priority = Thread::PRIORITY_NORMAL;

    /* Initially only the program and stack counter matter. */
    descriptor->k_reg[REGISTER_INDEX_SP] = (uint32_t)descriptor->kernel_stack.ceiling;

    /* Set up the entrypoint function with argument values */
    descriptor->k_reg[REGISTER_INDEX_PC]    = (uint32_t)Entry;
    descriptor->k_reg[REGISTER_INDEX_ARG0]  = (uint32_t)body;
    descriptor->k_reg[REGISTER_INDEX_ARG1]  = (uint32_t)param;

    /* Thread is initially running in kernel mode */
    descriptor->k_reg[REGISTER_INDEX_PSR] = ARM_SVC_MODE_BITS;

    /* Yield immediately to new thread so that it gets initialized */
    BeginTransaction();
    MakeReady(descriptor);
    MakeReady(THREAD_CURRENT());
    RunNextThread();
    EndTransaction();

    return descriptor;
}

void Thread::Join ()
{
    assert(THREAD_CURRENT() != this);
    assert(this->joiner == NULL);

    this->joiner = THREAD_CURRENT();

    while (this->state != Thread::STATE_FINISHED) {
        BeginTransaction();
        MakeUnready(THREAD_CURRENT(), STATE_JOINING);
        RunNextThread();
        EndTransaction();
    }

    if (this->kernel_stack.page != NULL) {
        Page::Free(this->kernel_stack.page);
    }
}

void Thread::SetEffectivePriority (Thread::Priority priority)
{
    this->effective_priority = priority;
}

Thread * Thread::DequeueReady ()
{
    Thread * next;

    assert(SpinlockLocked(&sched_spinlock));

    if (!io_ready_queue.Empty()) {
        next = io_ready_queue.PopFirst();
    }
    else if (!normal_ready_queue.Empty()) {
        next = normal_ready_queue.PopFirst();
    } else {
        next = NULL;
    }

    return next;
}

void Thread::MakeReady (Thread * thread)
{
    assert(SpinlockLocked(&sched_spinlock));
    assert(thread->queue_link.Unlinked());

    queue_for_thread(thread)->Append(thread);
    thread->state = Thread::STATE_READY;
}

void Thread::MakeUnready (Thread * thread, State state)
{
    assert(SpinlockLocked(&sched_spinlock));
    thread->state = state;
}

/**
 * Set to true by interrupt handlers when something has happened
 * that makes the scheduler algorithm need to be re-run at
 * the time that a syscall is being returned from.
 */
static bool         need_resched        = false;
static Spinlock_t   need_resched_lock   = SPINLOCK_INIT;

void Thread::SetNeedResched ()
{
    SpinlockLock(&need_resched_lock);
    need_resched = true;
    SpinlockUnlock(&need_resched_lock);
}

bool Thread::ResetNeedResched ()
{
    bool ret;

    SpinlockLock(&need_resched_lock);
    ret = need_resched;
    need_resched = false;
    SpinlockUnlock(&need_resched_lock);

    return ret;
}

void ThreadMakeReady (Thread * thread)
{
    Thread::MakeReady(thread);
}

Thread * ThreadDequeueReady (void)
{
    return Thread::DequeueReady();
}

bool ThreadResetNeedResched ()
{
    return Thread::ResetNeedResched();
}

Thread * ThreadStructFromStackPointer (uint32_t sp)
{
    return THREAD_STRUCT_FROM_SP(sp);
}

Process * ThreadGetProcess (Thread * thread)
{
    return thread->process;
}

void ThreadBeginTransaction ()
{
    assert(!InterruptsDisabled());
    SpinlockLock(&sched_spinlock);
}

void ThreadBeginTransactionDuringIrq ()
{
    assert(InterruptsDisabled());
    SpinlockLock(&sched_spinlock);
}

void ThreadBeginTransactionEndingIrq ()
{
    assert(InterruptsDisabled());
    SpinlockLockNoIrqSave(&sched_spinlock);
}

void ThreadEndTransaction ()
{
    SpinlockUnlock(&sched_spinlock);
}

void ThreadEndTransactionFromRestart ()
{
    SpinlockUnlockNoIrqRestore(&sched_spinlock);
}

#include <stdlib.h>

#include <sys/arch.h>
#include <sys/spinlock.h>

#include <kernel/assert.h>
#include <kernel/thread.hpp>
#include <kernel/vm.hpp>

typedef List<Thread, &Thread::queue_link> Queue_t;

static Queue_t normal_ready_queue;
static Queue_t io_ready_queue;

static Spinlock_t ready_queue_lock = SPINLOCK_INIT;

static inline Queue_t * queue_for_thread (Thread * t)
{
    if (t->assigned_priority == Thread::PRIORITY_IO || t->effective_priority == Thread::PRIORITY_IO) {
        return &io_ready_queue;
    }
    else {
        return &normal_ready_queue;
    }
}

static void thread_entry (Thread::Func func, void * param);

void ThreadYieldNoRequeueToSpecific (Thread * next);

typedef void (*ThreadSwitchPreFunc) (void *param);

static void ThreadSwitch (
        Thread * outgoing,
        Thread * incoming,
        ThreadSwitchPreFunc func,
        void * funcParam
        )
{
    uint32_t next_pc = 0;               /* Used by assembly fragment    */
    uint32_t cpsr_temp = 0;             /* Used by assembly fragment    */

    struct TranslationTable * incoming_tt = incoming->process
            ? incoming->process->pagetable
            : NULL;

    /* Turn off interrupts */
    IrqSave_t prev_irq_status = InterruptsDisable();
    assert(prev_irq_status.cpsr_interrupt_flags == 0);

    if (func != NULL) {
        func(funcParam);
    }

    /* Stash prior interrupt state in incoming thread's saved CPSR */
    incoming->registers[REGISTER_INDEX_PSR] &= ~(SETBIT(ARM_CPSR_I_BIT) | SETBIT(ARM_CPSR_F_BIT));
    incoming->registers[REGISTER_INDEX_PSR] |= prev_irq_status.cpsr_interrupt_flags;

    /* Only flushes TLB if the new data structure isn't the same as the old one */
    MmuSetUserTranslationTable(incoming_tt);

    /* Mark incoming thread as running */
    incoming->state = Thread::STATE_RUNNING;

    asm volatile(
        "                                                   \n\t"
        "save_outgoing$:                                    \n\t"
        "    /* Store normal registers */                   \n\t"
        "    stm %[p_outgoing], {r0 - r15}                  \n\t"
        "                                                   \n\t"
        "    /* Store CPSR modulo the IRQ mask */           \n\t"
        "    mrs %[cpsr_temp], cpsr                         \n\t"
        "    bic %[cpsr_temp], %[cpsr_temp], %[int_bits]    \n\t"
        "    orr %[cpsr_temp], %[cpsr_temp], %[prev_irq]    \n\t"
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
        : [p_outgoing] "r" (&outgoing->registers),
          [p_incoming] "r" (&incoming->registers),
          [p_saved_pc] "r" (&outgoing->registers[REGISTER_INDEX_PC]),
          [p_outgoing_cpsr] "r" (&outgoing->registers[REGISTER_INDEX_PSR]),
          [p_incoming_cpsr] "r" (&incoming->registers[REGISTER_INDEX_PSR]),
          [prev_irq] "r" (prev_irq_status.cpsr_interrupt_flags),
          [int_bits] "i" (SETBIT(ARM_CPSR_I_BIT) | SETBIT(ARM_CPSR_F_BIT))
        : "memory"
    );
}

static void thread_entry (Thread::Func func, void * param)
{
    func(param);

    THREAD_CURRENT()->state = Thread::STATE_FINISHED;

    if (THREAD_CURRENT()->joiner != NULL) {
        Thread::AddReady(THREAD_CURRENT()->joiner);
    }

    Thread::YieldNoRequeue();
}

Thread * Thread::Create (Thread::Func body, void * param)
{
    unsigned int    i;
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

    for (i = 0; i < sizeof(descriptor->registers) / sizeof(descriptor->registers[0]); ++i)
    {
        descriptor->registers[i] = 0;
    }

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
    descriptor->registers[REGISTER_INDEX_SP] = (uint32_t)descriptor->kernel_stack.ceiling;

    /* Set up the entrypoint function with argument values */
    descriptor->registers[REGISTER_INDEX_PC]    = (uint32_t)thread_entry;
    descriptor->registers[REGISTER_INDEX_ARG0]  = (uint32_t)body;
    descriptor->registers[REGISTER_INDEX_ARG1]  = (uint32_t)param;

    /* Thread is initially running in kernel mode */
    asm volatile (
        "mov %[cpsr], %[svc_mode_bits]          \n\t"
        : [cpsr] "=r" (descriptor->registers[REGISTER_INDEX_PSR])
        : [svc_mode_bits] "i" (ARM_SVC_MODE_BITS)
    );

    /* Yield immediately to new thread so that it gets initialized */
    ThreadSwitch(THREAD_CURRENT(), descriptor, (ThreadSwitchPreFunc)Thread::AddReady, THREAD_CURRENT());

    return descriptor;
}

void Thread::Join ()
{
    assert(THREAD_CURRENT() != this);
    assert(this->joiner == NULL);

    this->joiner = THREAD_CURRENT();

    while (this->state != Thread::STATE_FINISHED) {
        Thread::YieldNoRequeue();
    }

    if (this->kernel_stack.page != NULL) {
        Page::Free(this->kernel_stack.page);
    }
}

void Thread::SetEffectivePriority (Thread::Priority priority)
{
    this->effective_priority = priority;
}

void Thread::AddReady (Thread * thread)
{
    SpinlockLock(&ready_queue_lock);
    queue_for_thread(thread)->Append(thread);
    thread->state = Thread::STATE_READY;
    SpinlockUnlock(&ready_queue_lock);
}

void Thread::AddReadyFirst (Thread * thread)
{
    SpinlockLock(&ready_queue_lock);
    queue_for_thread(thread)->Prepend(thread);
    thread->state = Thread::STATE_READY;
    SpinlockUnlock(&ready_queue_lock);
}

Thread * Thread::DequeueReady ()
{
    Thread * next;

    SpinlockLock(&ready_queue_lock);

    if (!io_ready_queue.Empty()) {
        next = io_ready_queue.PopFirst();
    }
    else if (!normal_ready_queue.Empty()) {
        next = normal_ready_queue.PopFirst();
    } else {
        next = NULL;
    }

    SpinlockUnlock(&ready_queue_lock);

    return next;
}

void Thread::YieldNoRequeue ()
{
    /* Pop off thread at front of run-queue */
    Thread * next = Thread::DequeueReady();

    /* Since we're not requeuing, there had better be somebody runnable */
    assert(next != NULL);

    ThreadSwitch(THREAD_CURRENT(), next, NULL, NULL);
}

void Thread::YieldWithRequeue ()
{
    /* Pop off thread at front of run-queue */
    Thread * next = Thread::DequeueReady();

    /* Since we're requeuing, it's OK if there were no other runnable threads */
    if (next != NULL) {
        ThreadSwitch(THREAD_CURRENT(), next, (ThreadSwitchPreFunc)Thread::AddReady, THREAD_CURRENT());
    }
}

void ThreadYieldNoRequeueToSpecific (Thread * next)
{
    assert(next->queue_link.Unlinked());

    ThreadSwitch(THREAD_CURRENT(), next, NULL, NULL);
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

void ThreadAddReady (struct Thread * thread)
{
    Thread::AddReady(thread);
}

struct Thread * ThreadDequeueReady (void)
{
    return Thread::DequeueReady();
}

bool ThreadResetNeedResched ()
{
    return Thread::ResetNeedResched();
}

struct Thread * ThreadStructFromStackPointer (uint32_t sp)
{
    return THREAD_STRUCT_FROM_SP(sp);
}

struct Process * ThreadGetProcess (struct Thread * thread)
{
    return thread->process;
}

void ThreadSetStateReady (struct Thread * thread)
{
    thread->state = Thread::STATE_READY;
}

void ThreadSetStateRunning (struct Thread * thread)
{
    thread->state = Thread::STATE_RUNNING;
}

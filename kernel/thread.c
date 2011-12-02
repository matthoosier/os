#include <stdlib.h>

#include <kernel/arch.h>
#include <kernel/assert.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static LIST_HEAD(ready_queue);

static void thread_entry (ThreadFunc func, void * param);

static void ThreadSwitch (struct Thread * outgoing,
                   struct Thread * incoming)
{
    uint32_t next_pc = next_pc;
    uint32_t cpsr_temp = cpsr_temp;

    struct TranslationTable * outgoing_tt = outgoing->process
            ? outgoing->process->pagetable
            : NULL;
    struct TranslationTable * incoming_tt = incoming->process
            ? incoming->process->pagetable
            : NULL;

    if (outgoing_tt != incoming_tt) {
        MmuSetUserTranslationTable(incoming_tt);
        MmuFlushTlb();
    }

    asm volatile(
        "                                               \n"
        "save_outgoing:                                 \n"
        "    stm %[p_outgoing], {r0 - r15}              \n"
        "    mrs %[cpsr_temp], cpsr                     \n"
        "    str %[cpsr_temp], [%[p_outgoing_cpsr], #0] \n"
        "    ldr %[next_pc], =resume                    \n"
        "    str %[next_pc], [%[p_saved_pc], #0]        \n"
        "                                               \n"
        "restore_incoming:                              \n"
        "    ldr %[cpsr_temp], [%[p_incoming_cpsr], #0] \n"
        "    msr cpsr_f, %[cpsr_temp]                   \n"
        "    ldm %[p_incoming], {r0 - r15}              \n"
        "                                               \n"
        "resume:                                        \n"
        "    nop                                        \n"
        "    nop                                        \n"
        "                                               \n"
        : [next_pc]"+r" (next_pc),
          [cpsr_temp]"+r" (cpsr_temp)
        : [p_outgoing]"r" (&outgoing->registers),
          [p_incoming]"r" (&incoming->registers),
          [p_saved_pc]"r" (&outgoing->registers[REGISTER_INDEX_PC]),
          [p_outgoing_cpsr]"r" (&outgoing->registers[REGISTER_INDEX_PSR]),
          [p_incoming_cpsr]"r" (&incoming->registers[REGISTER_INDEX_PSR])
        : "memory"
    );
}

static void thread_entry (ThreadFunc func, void * param)
{
    func(param);

    THREAD_CURRENT()->state = THREAD_STATE_FINISHED;

    if (THREAD_CURRENT()->joiner != NULL) {
        ThreadAddReady(THREAD_CURRENT()->joiner);
    }

    ThreadYieldNoRequeue();
}

struct Thread * ThreadCreate (ThreadFunc body, void * param)
{
    unsigned int    i;
    struct Page *   stack_page;
    struct Thread * descriptor;

    stack_page = VmPageAlloc();

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
    INIT_LIST_HEAD(&descriptor->queue_link);
    descriptor->state = THREAD_STATE_READY;
    descriptor->joiner = NULL;

    /* Initially only the program and stack counter matter. */
    descriptor->registers[REGISTER_INDEX_SP] = (uint32_t)descriptor->kernel_stack.ceiling;

    /* Set up the entrypoint function with argument values */
    descriptor->registers[REGISTER_INDEX_PC]    = (uint32_t)thread_entry;
    descriptor->registers[REGISTER_INDEX_ARG0]  = (uint32_t)body;
    descriptor->registers[REGISTER_INDEX_ARG1]  = (uint32_t)param;

    /* Yield immediately to new thread so that it gets initialized */
    ThreadAddReady(THREAD_CURRENT());
    ThreadYieldNoRequeueToSpecific(descriptor);

    return descriptor;
}

void ThreadJoin (struct Thread * thread)
{
    assert(THREAD_CURRENT() != thread);
    assert(thread->joiner == NULL);

    thread->joiner = THREAD_CURRENT();

    while (thread->state != THREAD_STATE_FINISHED) {
        ThreadYieldNoRequeue();
    }

    if (thread->kernel_stack.page != NULL) {
        VmPageFree(thread->kernel_stack.page);
    }
}

extern void ThreadAddReady (struct Thread * thread)
{
    list_add_tail(&thread->queue_link, &ready_queue);
}

void ThreadYieldNoRequeue (void)
{
    struct Thread * next;

    assert(!list_empty(&ready_queue));

    /* Pop off thread at front of run-queue */
    next = list_first_entry(&ready_queue, struct Thread, queue_link);
    next->state = THREAD_STATE_RUNNING;
    list_del_init(&next->queue_link);

    ThreadSwitch(THREAD_CURRENT(), next);
}

void ThreadYieldNoRequeueToSpecific (struct Thread * next)
{
    assert(list_empty(&next->queue_link));

    ThreadSwitch(THREAD_CURRENT(), next);
}

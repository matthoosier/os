#include <stdlib.h>

#include "arch.h"
#include "thread.h"
#include "vm.h"

static LIST_HEAD(ready_queue);

static void thread_trampoline ();

void ThreadSwitch (struct Thread * outgoing,
                   struct Thread * incoming)
{
    uint32_t next_pc = next_pc;
    uint32_t cpsr_temp = cpsr_temp;

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
    INIT_LIST_HEAD(&descriptor->queue_link);
    descriptor->state = THREAD_STATE_READY;

    /* Initially only the program and stack counter matter. */
    descriptor->registers[REGISTER_INDEX_PC] = (uint32_t)body;
    descriptor->registers[REGISTER_INDEX_SP] = (uint32_t)descriptor->kernel_stack.ceiling;

    /* Set up the argument value */
    descriptor->registers[REGISTER_INDEX_R0] = (uint32_t)param;

    /* Install trampoline in case the thread returns. */
    descriptor->registers[REGISTER_INDEX_LR] = (uint32_t)thread_trampoline;

    list_add_tail(&descriptor->queue_link, &ready_queue);
    ThreadYield();

    return descriptor;
}

static void thread_trampoline ()
{
    while (1) {
    }
}

void ThreadYield (void)
{
    /* No-op if no threads are ready to run. */
    if (!list_empty(&ready_queue)) {
        struct Thread * outgoing;
        struct Thread * next;

        outgoing = THREAD_CURRENT();
        outgoing->state = THREAD_STATE_READY;
        list_add_tail(&outgoing->queue_link, &ready_queue);

        next = list_first_entry(&ready_queue, struct Thread, queue_link);
        list_del_init(&next->queue_link);
        next->state = THREAD_STATE_RUNNING;
        ThreadSwitch(outgoing, next);
    }
}

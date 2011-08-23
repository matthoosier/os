#include <stdlib.h>

#include "arch.h"
#include "scheduler.h"
#include "thread.h"
#include "vm.h"

static void thread_trampoline ();

void thread_switch (struct thread * outgoing,
                    struct thread * incoming)
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

struct thread * thread_create (thread_func body, void * param)
{
    unsigned int    i;
    struct page *   stack_page;
    struct thread * descriptor;

    stack_page = vm_page_alloc();

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

    scheduler_queue_insert(THREAD_STATE_READY, descriptor);
    scheduler_yield();

    return descriptor;
}

static void thread_trampoline ()
{
    while (1) {
    }
}

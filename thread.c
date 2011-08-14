#include <stdlib.h>

#include "arch.h"
#include "scheduler.h"
#include "thread.h"

struct thread * current = NULL;

void thread_switch (struct thread * outgoing,
                    struct thread * incoming)
{
    uint32_t next_pc = next_pc;
    uint32_t cpsr_temp = cpsr_temp;

    /*
    Remember to switch this _before_ the assembly statement!

    Afterward won't happen until after the 'outgoing' thread
    gets scheduled again sometime in the future.
    */
    current = incoming;

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

void thread_create (struct thread *                     descriptor,
                    const struct thread_create_args *   args)
{
    int i;

    for (i = 0; i < sizeof(descriptor->registers) / sizeof(descriptor->registers[0]); ++i)
    {
        descriptor->registers[i] = 0;
    }

    descriptor->stack.ceiling = args->stack.ceiling;
    descriptor->stack.base = args->stack.base;
    descriptor->next = NULL;
    descriptor->state = THREAD_STATE_READY;

    /* Initially only the program and stack counter matter. */
    descriptor->registers[REGISTER_INDEX_PC] = (uint32_t)args->body;
    descriptor->registers[REGISTER_INDEX_SP] = (uint32_t)descriptor->stack.ceiling;

    scheduler_queue_insert(THREAD_STATE_READY, descriptor);
    scheduler_yield();
}

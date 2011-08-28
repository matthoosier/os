#include <stdlib.h>

#include "array.h"
#include "arch.h"
#include "init.h"
#include "mmu.h"
#include "scheduler.h"
#include "thread.h"
#include "vm.h"

uint8_t init_stack[PAGE_SIZE]
    __attribute__ ((aligned (PAGE_SIZE)));

uint8_t * init_stack_ceiling = &init_stack[
    N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE
    ];

/* Retroactively filled in */
static struct thread *first_thread = (struct thread *)&init_stack[N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE];

/* Really forked */
static struct thread *second_thread;

void second_thread_body (void * param)
{
    while (1) {
        scheduler_yield();
    }
}

void init (void)
{
    int mmu_enabled;
    int arch_version;

    /*
    Record the important static bits about this thread. The rest will
    be filled in automatically the first time we context switch.
    */
    first_thread->kernel_stack.ceiling  = init_stack_ceiling;
    first_thread->kernel_stack.base     = &init_stack[0];
    first_thread->kernel_stack.page     = NULL;
    first_thread->state                 = THREAD_STATE_RUNNING;
    INIT_LIST_HEAD(&first_thread->queue_link);

    mmu_enabled = mmu_get_enabled();
    arch_version = arch_get_version();

    mmu_enabled = mmu_enabled;
    arch_version = arch_version;

    second_thread = thread_create(second_thread_body, "Foo!");

    while (1) {
        scheduler_yield();
    }
}

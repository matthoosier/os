#include <stdlib.h>

#include "array.h"
#include "arch.h"
#include "init.h"
#include "mmu.h"
#include "scheduler.h"
#include "thread.h"
#include "vm.h"

uint8_t init_stack[PAGE_SIZE]
    __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

uint8_t * init_stack_ceiling = &init_stack[N_ELEMENTS(init_stack)];

/* Retroactively filled in */
static struct thread first_thread;

/* Really forked */
static struct thread second_thread;

static uint8_t second_thread_stack[PAGE_SIZE]
    __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

void second_thread_body (void * param)
{
    while (1) {
        scheduler_yield();
    }
}

void init (void)
{
    struct thread_create_args args;

    int mmu_enabled;
    int arch_version;

    vm_init();

    /*
    Record the important static bits about this thread. The rest will
    be filled in automatically the first time we context switch.
    */
    first_thread.stack.ceiling  = init_stack_ceiling;
    first_thread.stack.base     = &init_stack[0];
    first_thread.state          = THREAD_STATE_RUNNING;
    INIT_LIST_HEAD(&first_thread.queue_link);

    current                     = &first_thread;

    mmu_enabled = mmu_get_enabled();
    arch_version = arch_get_version();

    mmu_enabled = mmu_enabled;
    arch_version = arch_version;

    args.stack.ceiling = &second_thread_stack[sizeof(second_thread_stack) / sizeof(second_thread_stack[0])];
    args.stack.base = &second_thread_stack[0];
    args.body = second_thread_body;
    args.param = "Foo!";

    thread_create(&second_thread, &args);

    while (1) {
        scheduler_yield();
    }
}

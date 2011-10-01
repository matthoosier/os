#include <stdbool.h>
#include <stdlib.h>

#include "array.h"
#include "arch.h"
#include "assert.h"
#include "init.h"
#include "mmu.h"
#include "object-cache.h"
#include "scheduler.h"
#include "thread.h"
#include "vm.h"

void install_kernel_memory_map();

void run_first_thread ()
    __attribute__((noreturn));

uint8_t init_stack[PAGE_SIZE]
    __attribute__ ((aligned (PAGE_SIZE)));

uint8_t * init_stack_ceiling = &init_stack[
    N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE
    ];

struct object_cache an_object_cache;
enum { AN_OBJECT_CACHE_ELEMENT_SIZE = PAGE_SIZE / 2 };

void init (void)
{
    struct page * one = vm_page_alloc();
    struct page * two = vm_page_alloc();
    struct page * three = vm_page_alloc();
    struct page * four = vm_page_alloc();

    vm_page_free(three);
    three = NULL;
    vm_page_free(four);
    four = NULL;

    one = one;
    two = two;
    three = three;
    four = four;

    void *an_element1;
    void *an_element2;
    void *an_element3;
    void *an_element4;

    object_cache_init(&an_object_cache, AN_OBJECT_CACHE_ELEMENT_SIZE);

    an_element1 = object_cache_alloc(&an_object_cache);
    object_cache_free(&an_object_cache, an_element1);

    an_element2 = object_cache_alloc(&an_object_cache);
    an_element1 = object_cache_alloc(&an_object_cache);
    an_element3 = object_cache_alloc(&an_object_cache);
    an_element4 = object_cache_alloc(&an_object_cache);

    object_cache_free(&an_object_cache, an_element4);
    object_cache_free(&an_object_cache, an_element3);
    object_cache_free(&an_object_cache, an_element1);
    object_cache_free(&an_object_cache, an_element2);

    an_element1 = an_element1;
    an_element2 = an_element2;
    an_element3 = an_element3;
    an_element4 = an_element4;

    install_kernel_memory_map();

    run_first_thread();
}

void install_kernel_memory_map ()
{
    struct translation_table * kernel_tt = translation_table_alloc();
    kernel_tt = kernel_tt;

    /* Map only the kernel high memory (all physical RAM) */
    unsigned int mb_idx = KERNEL_MODE_OFFSET >> MEGABYTE_SHIFT;

    unsigned int mb_idx_bound = (VIRTUAL_HEAP_START + HEAP_SIZE) >> MEGABYTE_SHIFT;;

    for (; mb_idx < mb_idx_bound; mb_idx++)
    {
        bool success = translation_table_map_section(
                kernel_tt,
                mb_idx << MEGABYTE_SHIFT,
                (mb_idx << MEGABYTE_SHIFT) - KERNEL_MODE_OFFSET
                );
        assert(success);
    }

    /* Really just a marker from the linker script */
    extern char __VectorStartPhysical;

    bool success = translation_table_map_page(
                kernel_tt,
                0xffff0000,
                (physaddr_t)&__VectorStartPhysical
                );
    assert(success);

    /*
    Yes, it's actually already on from our crude original setup.
    But there was some additional setup work (like configuring exceptions)
    that didn't happen originally.
    */
    mmu_set_enabled();

    /* Install this fully-fledged pagetable */
    mmu_set_translation_table(kernel_tt);
}

/* Retroactively filled in */
static struct thread *first_thread = (struct thread *)&init_stack[N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE];

/* Really forked */
static struct thread *second_thread;

void second_thread_body (void * param)
{
    uint32_t saved_cpsr = saved_cpsr;

    asm volatile(
        ".include \"arm-defs.inc\"      \n"
        "mrs %[saved_cpsr], cpsr        \n"
        "cps #irq                       \n"
        "cps #und                       \n"
        "cps #svc                       \n"
        "msr cpsr, %[saved_cpsr]        \n"
        : [saved_cpsr]"+r"(saved_cpsr)
    );

    while (true) {
        scheduler_yield();
    }
}

void run_first_thread ()
{
    /*
    Record the important static bits about this thread. The rest will
    be filled in automatically the first time we context switch.
    */
    first_thread->kernel_stack.ceiling  = init_stack_ceiling;
    first_thread->kernel_stack.base     = &init_stack[0];
    first_thread->kernel_stack.page     = NULL;
    first_thread->state                 = THREAD_STATE_RUNNING;
    INIT_LIST_HEAD(&first_thread->queue_link);

    int mmu_enabled;
    int arch_version;

    mmu_enabled = mmu_get_enabled();
    arch_version = arch_get_version();

    mmu_enabled = mmu_enabled;
    arch_version = arch_version;

    second_thread = thread_create(second_thread_body, "Foo!");

    while (true) {
        scheduler_yield();
    }
}

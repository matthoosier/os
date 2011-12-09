#include <stdbool.h>
#include <stdlib.h>

#include <kernel/array.h>
#include <kernel/arch.h>
#include <kernel/assert.h>
#include <kernel/mmu.h>
#include <kernel/object-cache.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

#include "init.h"

void install_kernel_memory_map();

void run_first_thread ()
    __attribute__((noreturn));

uint8_t init_stack[PAGE_SIZE]
    __attribute__ ((aligned (PAGE_SIZE)));

uint8_t * init_stack_ceiling = &init_stack[
    N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE
    ];

struct ObjectCache an_object_cache;
enum { AN_OBJECT_CACHE_ELEMENT_SIZE = PAGE_SIZE / 2 };

void Init (void)
{
    /* Execute global constructors */
    extern char __init_array_start;
    extern char __init_array_end;

    typedef void (*VoidFunc) (void);

    VoidFunc * ctorIter;

    for (ctorIter = (VoidFunc *)&__init_array_start;
         ctorIter < (VoidFunc *)&__init_array_end;
         ctorIter++)
    {
        (*ctorIter)();
    }


    struct Page * one = VmPageAlloc();
    struct Page * two = VmPageAlloc();
    struct Page * three = VmPageAlloc();
    struct Page * four = VmPageAlloc();

    VmPageFree(three);
    three = NULL;
    VmPageFree(four);
    four = NULL;

    one = one;
    two = two;
    three = three;
    four = four;

    void *an_element1;
    void *an_element2;
    void *an_element3;
    void *an_element4;

    ObjectCacheInit(&an_object_cache, AN_OBJECT_CACHE_ELEMENT_SIZE);

    an_element1 = ObjectCacheAlloc(&an_object_cache);
    ObjectCacheFree(&an_object_cache, an_element1);

    an_element2 = ObjectCacheAlloc(&an_object_cache);
    an_element1 = ObjectCacheAlloc(&an_object_cache);
    an_element3 = ObjectCacheAlloc(&an_object_cache);
    an_element4 = ObjectCacheAlloc(&an_object_cache);

    ObjectCacheFree(&an_object_cache, an_element4);
    ObjectCacheFree(&an_object_cache, an_element3);
    ObjectCacheFree(&an_object_cache, an_element1);
    ObjectCacheFree(&an_object_cache, an_element2);

    an_element1 = an_element1;
    an_element2 = an_element2;
    an_element3 = an_element3;
    an_element4 = an_element4;

    install_kernel_memory_map();

    run_first_thread();
}

void install_kernel_memory_map ()
{
    struct TranslationTable * kernel_tt = TranslationTableAlloc();
    kernel_tt = kernel_tt;

    /* Map only the kernel high memory (all physical RAM) */
    unsigned int mb_idx = KERNEL_MODE_OFFSET >> MEGABYTE_SHIFT;

    unsigned int mb_idx_bound = (VIRTUAL_HEAP_START + HEAP_SIZE) >> MEGABYTE_SHIFT;;

    for (; mb_idx < mb_idx_bound; mb_idx++)
    {
        bool success = TranslationTableMapSection(
                kernel_tt,
                mb_idx << MEGABYTE_SHIFT,
                (mb_idx << MEGABYTE_SHIFT) - KERNEL_MODE_OFFSET,
                PROT_KERNEL
                );
        assert(success);
    }

    /* Really just a marker from the linker script */
    extern char __VectorStartPhysical;

    bool success = TranslationTableMapPage(
                kernel_tt,
                0xffff0000,
                (PhysAddr_t)&__VectorStartPhysical,
                PROT_KERNEL
                );
    assert(success);

    /* Install this fully-fledged pagetable */
    MmuSetUserTranslationTable(kernel_tt);
    MmuSetKernelTranslationTable(kernel_tt);

    /*
    Yes, it's actually already on from our crude original setup.
    But there was some additional setup work (like configuring exceptions)
    that didn't happen originally.
    */
    MmuSetEnabled();

    /*
    Crutch user-mode table no longer necessary now that MmuSetEnabled()
    has fully configured the CPU's support for using a separate translation
    table for user- and kernel-mode address ranges.
    */
    MmuSetUserTranslationTable(NULL);
}

/* Retroactively filled in */
static struct Thread *first_thread = (struct Thread *)&init_stack[N_ELEMENTS(init_stack) - ALIGNED_THREAD_STRUCT_SIZE];

/* Really forked */
static struct Thread *second_thread;

void second_thread_body (void * param)
{
    uint32_t saved_cpsr = saved_cpsr;

    while (true) {

        asm volatile(
            ".include \"arm-defs.inc\"      \n"
            "mrs %[saved_cpsr], cpsr        \n"
            "cps #irq                       \n"
            "cps #und                       \n"
            "cps #svc                       \n"
            "msr cpsr, %[saved_cpsr]        \n"
            : [saved_cpsr]"+r"(saved_cpsr)
        );

        ThreadAddReady(THREAD_CURRENT());
        ThreadYieldNoRequeue();
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
    first_thread->process               = NULL;
    first_thread->state                 = THREAD_STATE_RUNNING;
    INIT_LIST_HEAD(&first_thread->queue_link);

    int mmu_enabled;
    int arch_version;

    mmu_enabled = MmuGetEnabled();
    arch_version = ArchGetVersion();

    mmu_enabled = mmu_enabled;
    arch_version = arch_version;

    second_thread = ThreadCreate(second_thread_body, "Foo!");

    ProcessCreate("echo");
    ProcessCreate("syscall-client");

    while (true) {
        ThreadAddReady(THREAD_CURRENT());
        ThreadYieldNoRequeue();
    }
}

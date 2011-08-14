#include "init.h"
#include "mmu.h"

uint8_t init_stack[PAGE_SIZE]
    __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

uint8_t * init_stack_ceiling = &init_stack[PAGE_SIZE];

void init (void)
{
    int mmu_enabled = mmu_get_enabled();

    mmu_enabled = mmu_enabled;

    while (1) {}
}

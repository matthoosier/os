#include "mmu.h"
#include "bits.h"

#include <stdint.h>

#define ARM_MMU_ENABLED_BIT 0

int mmu_get_enabled (void)
{
    uint32_t cp15_r1;

    /*
    p15: coprocessor number
    0: opcode 1; generally 0b000
    %0: destination ARM register
    c1: primary coprocessor register name
    c0: further specification of the coprocessor register name.
        "c0" is a placeholder here and is ignored for cp15 c1
    */
    asm volatile(
        "mrc p15, 0, %0, c1, c0"
        : "=r"(cp15_r1)
        :
        :
    );

    return TESTBIT(cp15_r1, ARM_MMU_ENABLED_BIT);
}

void mmu_set_enabled (int enable)
{
    uint32_t cp15_r1;

    asm volatile(
        "mrc p15, 0, %0, c1, c0"
        : "=r"(cp15_r1)
        :
        :
    );

    cp15_r1 |= SETBIT(ARM_MMU_ENABLED_BIT);

    asm volatile(
        "mcr p15, 0, %0, c1, c0"
        :
        : "r"(cp15_r1)
        :
    );
}

physaddr_t mmu_get_pagetable_base (void)
{
    return 0;
}

void mmu_set_pagetable_base (physaddr_t newbase)
{
}

#include <stdint.h>

#include "array.h"
#include "bits.h"
#include "mmu.h"
#include "vm.h"

/* Some kind of optimization for TLB stuff. Don't care for now. */
#define DOMAIN_DEFAULT          0
#define DOMAIN_ACCESS_MANAGER   0b11

#define BITS_PER_MEGABYTE 20
#define ARM_MMU_ENABLED_BIT 0

/*
 * Each entry of the firstlevel page table describes 1MB of the
 * virtual address space.
 *
 * Using the 'Section' mapping type, each firstlevel table directly
 * describes where to find 1MB of address range. So no second-level
 * tables are necessary.
 *
 * Do NOT put in BSS.
 */
static pt_firstlevel_t early_table[4096]
    __attribute__((aligned(16 * 1024)))
    = {0};

static void _install_pagetable()
{
    uint32_t ttbc = ttbc;
    uint32_t ttb0 = ttb0;

    /*
    Only bits 14 through 31 (that is, the high 18 bits) of the translation
    table base register are usable. Because the hardware requires the
    translation table to start on a 16KB boundary.
    */

    /* Fetch translation base register */
    asm volatile(
        "mrc p15, 0, %[ttb0], c2, c2, 0"
        : [ttb0]"=r" (ttb0)
    );

    /* Set the top 18 bits to be the base of the pagetable. */
    ttb0 &= 0x00003fff;
    ttb0 |= (uintptr_t)&early_table[0];

    /* Store back */
    asm volatile(
        "mcr p15, 0, %[ttb0], c2, c2, 0"
        :
        : [ttb0]"r" (ttb0)
    );

    /* Fetch the Translation Table Base Control register just to have a look */
    asm volatile(
        "mrc p15, 0, %[ttbc], c2, c0, 2"
        : [ttbc]"=r" (ttbc)
    );

    ttbc = ttbc;
}

static void _enable_mmu()
{
    /* Control Register */
    uint32_t cp15_r1;

    /* Domain access control register */
    uint32_t cp15_r3;

    /*
    Our crude early pagetable puts all sections in Domain 0, so we just need
    to set the permissions for only that one domain.

    CP15, Register 3 controls these. Each of the 16 domains gets two bits
    of configuration.
    */
    cp15_r3 = DOMAIN_ACCESS_MANAGER << (2 * DOMAIN_DEFAULT);
    asm volatile (
        "mcr p15, 0, %[cp15_r3], c3, c0, 0\n"
        :
        : [cp15_r3]"r" (cp15_r3)
    );

    /* Read MMU control register */
    asm volatile(
        "mrc p15, 0, %[cp15_r1], c1, c0"
        : [cp15_r1]"=r" (cp15_r1)
    );

    /* Map in the 'enabled' bit */
    cp15_r1 |= SETBIT(ARM_MMU_ENABLED_BIT);

    /* Write back out the MMU control register. */
    asm volatile(
        "mcr p15, 0, %[cp15_r1], c1, c0"
        :
        : [cp15_r1]"r" (cp15_r1)
    );   

    /* No need toFlush TLB; any memory already accessed is still valid. */
}

void early_setup_dual_memory_map (void)
{
    uintptr_t i;
    uintptr_t first_unused_phys_mb;

    /* First set all entries in the page table to unmapped. */
    for (i = 0; i < N_ELEMENTS(early_table); i++) {
        early_table[i] = PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;
    }

    /* Use linker symbol to find out where first unused address is. */
    first_unused_phys_mb = V2P(VIRTUAL_HEAP_START);

    /* Now round up to nearest megabyte */
    first_unused_phys_mb += (1024 * 1024) - 1;
    first_unused_phys_mb &= 0xfff00000;

    /* Now smash this down so that it represents the number of megabytes */
    first_unused_phys_mb = first_unused_phys_mb >> BITS_PER_MEGABYTE;

    /* Now go set up dual mappings for actually occupied pages */
    for (i = 0; i < first_unused_phys_mb; i++) {

        /*
        Install the right attributes for a fully-mapped physical
        one-to-one mapping

        The loop counter already tells us what physical megabyte offset
        should be mapped for the the ith translation table element. Just put
        the loop counter into the top 12 bits of the TT entry.
        */
        early_table[i] =
                PT_FIRSTLEVEL_MAPTYPE_SECTION |
                (DOMAIN_DEFAULT << PT_FIRSTLEVEL_DOMAIN_SHIFT) |
                PT_FIRSTLEVEL_SECTION_AP_FULL |
                (i << PT_FIRSTLEVEL_SECTION_BASE_ADDR_SHIFT);

        /* Do the same as above, but for the virtual memory address... */
        uintptr_t virt_i = i + (KERNEL_MODE_OFFSET >> BITS_PER_MEGABYTE);

        /* The bit pattern in the high mapping is the same, just copy it. */
        early_table[virt_i] = early_table[i];
    }

    /* Set up high mappings alone for all memory not statically allocated */
    for (i = first_unused_phys_mb; i < N_ELEMENTS(early_table) - (KERNEL_MODE_OFFSET >> BITS_PER_MEGABYTE); i++) {
        /*
        The loop counter already tells us what physical megabyte offset
        should be mapped for the the i+(KERNEL_MODE_OFFSET/1MB)th
        translation table element. Just put the loop counter into the top
        12 bits of the TT entry.
        */
        uintptr_t virt_i = i + (KERNEL_MODE_OFFSET >> BITS_PER_MEGABYTE);

        early_table[virt_i] =
            PT_FIRSTLEVEL_MAPTYPE_SECTION |
            (DOMAIN_DEFAULT << PT_FIRSTLEVEL_DOMAIN_SHIFT) |
            PT_FIRSTLEVEL_SECTION_AP_FULL |
            (i << PT_FIRSTLEVEL_SECTION_BASE_ADDR_SHIFT);
    }

    _install_pagetable();
    _enable_mmu();
}

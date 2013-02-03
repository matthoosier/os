#include <stdint.h>
#include <string.h>

#include <muos/arch.h>
#include <muos/array.h>
#include <muos/bits.h>
#include <muos/compiler.h>
#include <muos/error.h>
#include <muos/spinlock.h>

#include <kernel/assert.h>
#include <kernel/minmax.hpp>
#include <kernel/mmu.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

#define ARM_MMU_ENABLED_BIT             0
#define ARM_MMU_EXCEPTION_VECTOR_BIT    13

static inline uint32_t GetTTBR0 ()
{
    uint32_t val;

    asm volatile(
        "mrc p15, 0, %[ttbr0], c2, c2, 0"
        : [ttbr0] "=r" (val)
    );

    return val;
}

static inline void SetTTBR0 (uint32_t val)
{
    asm volatile(
        "mcr p15, 0, %[ttbr0], c2, c2, 0"
        :
        : [ttbr0] "r" (val)
    );
}

static inline uint32_t GetTTBR1 ()
{
    uint32_t val;

    asm volatile(
        "mrc p15, 0, %[ttbr1], c2, c2, 1"
        : [ttbr1] "=r" (val)
    );

    return val;
}

static inline void SetTTBR1 (uint32_t val)
{
    asm volatile(
        "mcr p15, 0, %[ttbr1], c2, c2, 1"
        :
        : [ttbr1] "r" (val)
    );
}

static inline uint32_t GetTTBC ()
{
    uint32_t val;

    asm volatile(
        "mrc p15, 0, %[reg], c2, c2, 2"
        : [reg] "=r" (val)
    );

    return val;
}

static inline void SetTTBC (uint32_t val)
{
    asm volatile(
        "mcr p15, 0, %[reg], c2, c2, 2"
        :
        : [reg] "r" (val)
    );
}

static inline unsigned int ap_from_prot (Prot_t prot)
{
    unsigned int val;

    switch (prot) {
        case PROT_NONE:
            val = PT_FIRSTLEVEL_SECTION_AP_NONE;
            break;
        case PROT_KERNEL:
            val = PT_FIRSTLEVEL_SECTION_AP_PRIV_ONLY;
            break;
        case PROT_USER_READONLY:
            val = PT_FIRSTLEVEL_SECTION_AP_PRIV_AND_USER_READ;
            break;
        case PROT_USER_READWRITE:
            val = PT_FIRSTLEVEL_SECTION_AP_FULL;
            break;
    }

    return (val & PT_FIRSTLEVEL_SECTION_AP_MASK) >> PT_FIRSTLEVEL_SECTION_AP_SHIFT;
}

static inline Prot_t prot_from_ap (uint8_t ap)
{
    Prot_t ret;

    switch (ap & 0b11) {
        case 0b00: /* NONE */
            ret = PROT_NONE;
            break;
        case 0b01: /* PRIV_ONLY */
            ret = PROT_KERNEL;
            break;
        case 0b10: /* PRIV_AND_USER_READ */
            ret = PROT_USER_READONLY;
            break;
        case 0b11: /* FULL */
            ret = PROT_USER_READWRITE;
            break;
    }

    return ret;
}

static inline bool check_access (
        uint8_t ap
        )
{
    return true;
}

int MmuGetEnabled (void)
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

void MmuSetEnabled ()
{
    /* Control Register */
    uint32_t cp15_r1;

    /* Domain access control register */
    uint32_t cp15_r3;

    /*
    The number of leading bits which must be 0 for an address to fall into
    the user (as opposed to kernel) address range.
    */
    int n;

    /* Allow full access to everything in the default domain. */
    cp15_r3 = PT_DOMAIN_ACCESS_LEVEL_CLIENT << (2 * PT_DOMAIN_DEFAULT);

    asm volatile(
        "mcr p15, 0, %[cp15_r3], c3, c0, 0"
        :
        : [cp15_r3] "r" (cp15_r3)
    );

    /*
    Turn on VMSAv6's support for dual translation-table bases.
    */
    #define TTBC_N_MASK     0b111

    /*
    Enfoce that kernel-reserved address range starts on an even power of 2
    */
    assert(1U << (__builtin_ffs(KERNEL_MODE_OFFSET) - 1) == KERNEL_MODE_OFFSET);

    n = 32 - (__builtin_ffs(KERNEL_MODE_OFFSET) - 1);
    uint32_t ttbc = GetTTBC();
    ttbc &= ~TTBC_N_MASK;
    ttbc |= (n & TTBC_N_MASK);
    SetTTBC(ttbc);

    /* Read/modify/write on MMU control register. */
    asm volatile(
        "mrc p15, 0, %[cp15_r1], c1, c0"
        : [cp15_r1] "=r" (cp15_r1)
    );

    /* Turn on MMU-enable bit */
    cp15_r1 |= SETBIT(ARM_MMU_ENABLED_BIT);

    /* Turn on high-vector enable bit */
    cp15_r1 |= SETBIT(ARM_MMU_EXCEPTION_VECTOR_BIT);

    asm volatile(
        "mcr p15, 0, %[cp15_r1], c1, c0"
        :
        : [cp15_r1] "r" (cp15_r1)
    );
}

void MmuFlushTlb (void)
{
    int ignored_register = 0;

    asm volatile(
        "mcr p15, 0, %[ignored_register], c8, c7, 0"
        :
        : [ignored_register] "r" (ignored_register)
    );
}

/* Allocates secondlevel_table's */
SyncSlabAllocator<SecondlevelTable> SecondlevelTable::sSlab;

/* Allocates secondlevel_ptes's */
SyncSlabAllocator<SecondlevelPtes> SecondlevelPtes::sSlab;

SecondlevelTable::SecondlevelTable () throw (std::bad_alloc)
{
    unsigned int i;

    this->ptes = SecondlevelPtes::sSlab.AllocateWithThrow();

    assert(N_ELEMENTS(this->ptes->ptes) == 256);

    for (i = 0; i < N_ELEMENTS(this->ptes->ptes); i++) {
        this->ptes->ptes[i] = PT_SECONDLEVEL_MAPTYPE_UNMAPPED;
    }

    this->num_mapped_pages = 0;
}

SecondlevelTable::~SecondlevelTable () throw ()
{
    if (!this->link.Unlinked()) {
        List<SecondlevelTable, &SecondlevelTable::link>::Remove(this);
    }

    SecondlevelPtes::sSlab.Free(this->ptes);
}

SyncSlabAllocator<TranslationTable> TranslationTable::sSlab;

TranslationTable::TranslationTable () throw (std::bad_alloc)
{
    enum {
        /*
         * log_2 of the number of pages required to hold
         * the hardware translation-table.
         *
         * I.e., the translation table is
         *
         *     PAGE_SIZE * (2^^TRANSLATION_TABLE_PAGES_ORDER)
         *
         * bytes long.
         */
        TRANSLATION_TABLE_PAGES_ORDER = 2,

        TRANSLATION_TABLE_SIZE = PAGE_SIZE * (1 << TRANSLATION_TABLE_PAGES_ORDER),
    };

    /* The ARM MMU hardware requires that a translation table is 16KB long */
    COMPILER_ASSERT(TRANSLATION_TABLE_SIZE == 4096 * 4);

    /* Translation table is 16-KB aligned and 4 pages long. */
    this->firstlevel_ptes_pages = Page::Alloc(TRANSLATION_TABLE_PAGES_ORDER);

    if (!this->firstlevel_ptes_pages) {
        throw std::bad_alloc();
    }

    this->firstlevel_ptes = (pt_firstlevel_t *)this->firstlevel_ptes_pages->base_address;

    this->sparse_secondlevel_map = new SparseSecondlevelMap_t(SparseSecondlevelMap_t::AddressCompareFunc);

    /* Initially make all sections unmapped */
    for (unsigned int i = 0; i < TRANSLATION_TABLE_SIZE / sizeof(pt_firstlevel_t); i++) {
        this->firstlevel_ptes[i] = PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;
    }
}

/*
Use a foreach on the tree to collect any existing nodes into a
list, then blow away the list's elements afterward.
*/
static void func (RawTreeMap::Key_t key, RawTreeMap::Value_t value, void * user_data)
{
    typedef List<SecondlevelTable, &SecondlevelTable::link> list_t;

    list_t *                    head;
    struct SecondlevelTable *   secondlevel_table;

    head                = (List<SecondlevelTable, &SecondlevelTable::link> *)user_data;
    secondlevel_table   = (struct SecondlevelTable *)value;

    head->Append(secondlevel_table);
}

TranslationTable::~TranslationTable ()
{
    /* Aggregates any individual secondlevel_table's that need freed */
    List<SecondlevelTable, &SecondlevelTable::link> head;

    /* Clean out any individual second-level translation tables */

    /* Go ahead and gather up all the nodes */
    this->sparse_secondlevel_map->Foreach(func, &head);

    /* Deallocate everything we found in there */
    while (!head.Empty())
    {
        delete head.PopFirst();
    }

    this->firstlevel_ptes = NULL;
}

static TranslationTable * kernel_translation_table = 0;

TranslationTable * TranslationTable::GetKernel ()
{
    return kernel_translation_table;
}

void TranslationTable::SetKernel (TranslationTable * table)
{
    uint32_t    ttbr1;
    PhysAddr_t  table_phys = V2P((VmAddr_t)&table->firstlevel_ptes[0]);

    /* Sanity check */
    assert((table_phys & 0xffffc000) == table_phys);

    /*
    Only bits 14 through 31 (that is, the high 18 bits) of the translation
    table base register are usable. Because the hardware requires the
    translation table to start on a 16KB boundary.
    */

    /* Fetch translation base register */
    ttbr1 = GetTTBR1();

    /* Set the top 18 bits to encode our translation base address */
    ttbr1 &= 0x00003fff;
    ttbr1 |= (table_phys & 0xffffc000);

    /* Install modified register back */
    SetTTBR1(ttbr1);
    kernel_translation_table = table;
}

static TranslationTable * user_translation_table = 0;

TranslationTable * TranslationTable::GetUser ()
{
    return user_translation_table;
}

void TranslationTable::SetUser (TranslationTable * table)
{
    uint32_t    ttbr0;
    PhysAddr_t  table_phys;

    table_phys = table != NULL
            ? V2P((VmAddr_t)&table->firstlevel_ptes[0])
            : 0;

    /* Sanity check */
    assert((table_phys & 0xffffc000) == table_phys);

    /*
    Only bits 14 through 31 (that is, the high 18 bits) of the translation
    table base register are usable. Because the hardware requires the
    translation table to start on a 16KB boundary.
    */

    /* Fetch translation base register */
    ttbr0 = GetTTBR0();

    /* Set the top 18 bits to encode our translation base address */
    ttbr0 &= 0x00003fff;
    ttbr0 |= (table_phys & 0xffffc000);

    /* Install modified register back */
    SetTTBR0(ttbr0);

    if (user_translation_table != table) {
        MmuFlushTlb();
    }

    user_translation_table = table;
}

TranslationTable * TranslationTableGetUser ()
{
    return TranslationTable::GetUser();
}

void TranslationTableSetUser (TranslationTable * table)
{
    TranslationTable::SetUser (table);
}

bool TranslationTable::MapSection (
        VmAddr_t virt,
        PhysAddr_t phys,
        Prot_t prot
        )
{
    unsigned int virt_idx;
    unsigned int phys_idx;

    assert(virt % SECTION_SIZE == 0);
    assert(phys % SECTION_SIZE == 0);

    virt_idx = virt >> MEGABYTE_SHIFT;
    phys_idx = phys >> MEGABYTE_SHIFT;

    /* Make sure no individual page mappings exist for the VM range */
    if ((this->firstlevel_ptes[virt_idx] & PT_FIRSTLEVEL_MAPTYPE_MASK) != PT_FIRSTLEVEL_MAPTYPE_UNMAPPED) {
        return false;
    }

    /* Install the mapping */
    this->firstlevel_ptes[virt_idx] =
            PT_FIRSTLEVEL_MAPTYPE_SECTION |
            (PT_DOMAIN_DEFAULT << PT_FIRSTLEVEL_DOMAIN_SHIFT) |
            (ap_from_prot(prot) << PT_FIRSTLEVEL_SECTION_AP_SHIFT) |
            ((phys_idx << MEGABYTE_SHIFT) & PT_FIRSTLEVEL_SECTION_BASE_ADDR_MASK);

    return true;
}

bool TranslationTable::UnmapSection (
        VmAddr_t virt
        )
{
    unsigned int virt_idx;

    assert(virt % SECTION_SIZE == 0);

    virt_idx = virt >> MEGABYTE_SHIFT;

    switch (this->firstlevel_ptes[virt_idx] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            this->firstlevel_ptes[virt_idx] = PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;
            return true;
            break;

        case PT_FIRSTLEVEL_MAPTYPE_COARSE:
        case PT_FIRSTLEVEL_MAPTYPE_UNMAPPED:
            return false;
            break;

        /* There are no other defined mapping types */
        default:
            assert(false);
            return false;
            break;
    }
}

bool TranslationTable::MapPage (
        VmAddr_t virt,
        PhysAddr_t phys,
        Prot_t prot
        )
{
    /* VM address rounded down to nearest megabyte */
    uintptr_t virt_mb_rounded;

    /* Page index (within the containing megabyte) of the VM address */
    uintptr_t virt_pg_idx;

    /* Data structure representing secondlevel table */
    struct SecondlevelTable * secondlevel_table = NULL;

    assert(virt % PAGE_SIZE == 0);
    assert(phys % PAGE_SIZE == 0);

    virt_mb_rounded = virt & MEGABYTE_MASK;
    virt_pg_idx = (virt & ~MEGABYTE_MASK) >> PAGE_SHIFT;

    assert(virt_pg_idx < (SECTION_SIZE / PAGE_SIZE));

    /* Make sure no previous mapping exists for the page */
    switch (this->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_UNMAPPED:
            break;

        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            /* This virtual address range already used by a 1MB section map */
            return false;

        case PT_FIRSTLEVEL_MAPTYPE_COARSE:
        {
            secondlevel_table = this->sparse_secondlevel_map->Lookup(virt_mb_rounded);

            if (!secondlevel_table) {
                /* No pages exist in the section. Why's it mapped then?? */
                assert(false);
            }
            else {
                assert(secondlevel_table->ptes != NULL);

                if ((secondlevel_table->ptes->ptes[virt_pg_idx] & PT_SECONDLEVEL_MAPTYPE_MASK) != PT_SECONDLEVEL_MAPTYPE_UNMAPPED) {
                    /* Page already mapped. */
                    return false;
                }
            }

            break;
        }

        default:
            /* No other mapping type should be used */
            assert(false);
            return false;
    }

    /* In case a table didn't exist yet, make it */
    if (!secondlevel_table) {

        try {
            secondlevel_table = new SecondlevelTable();
        }
        catch (std::bad_alloc & exc) {
            // Couldn't allocate memory for the new secondary table
            return false;
        }

        this->sparse_secondlevel_map->Insert(virt_mb_rounded, secondlevel_table);

        this->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] =
                PT_FIRSTLEVEL_MAPTYPE_COARSE |
                (PT_DOMAIN_DEFAULT << PT_FIRSTLEVEL_DOMAIN_SHIFT) |
                (V2P((VmAddr_t)&secondlevel_table->ptes->ptes[0]) & PT_FIRSTLEVEL_COARSE_BASE_ADDR_MASK);
    }

    /* Insert the new page into the secondlevel TT */
    secondlevel_table->ptes->ptes[virt_pg_idx] =
            PT_SECONDLEVEL_MAPTYPE_SMALL_PAGE |
            (ap_from_prot(prot) << PT_SECONDLEVEL_AP0_SHIFT) |
            (ap_from_prot(prot) << PT_SECONDLEVEL_AP1_SHIFT) |
            (ap_from_prot(prot) << PT_SECONDLEVEL_AP2_SHIFT) |
            (ap_from_prot(prot) << PT_SECONDLEVEL_AP3_SHIFT) |
            (phys & PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_MASK);

    secondlevel_table->num_mapped_pages++;

    return true;
}

bool TranslationTable::UnmapPage (
        VmAddr_t virt
        )
{
    /* VM address rounded down to nearest megabyte */
    uintptr_t virt_mb_rounded;

    /* Page index (within the containing megabyte) of the VM address */
    uintptr_t virt_pg_idx;

    /* Data structure representing secondlevel table */
    struct SecondlevelTable * secondlevel_table = NULL;

    assert(virt % PAGE_SIZE == 0);

    virt_mb_rounded = virt & MEGABYTE_MASK;
    virt_pg_idx = (virt & ~MEGABYTE_MASK) >> PAGE_SHIFT;

    assert(virt_pg_idx < (SECTION_SIZE / PAGE_SIZE));

    /* Make sure no previous mapping exists for the page */
    switch (this->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_UNMAPPED:
        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            return false;
            break;

        case PT_FIRSTLEVEL_MAPTYPE_COARSE:

            secondlevel_table = this->sparse_secondlevel_map->Lookup(virt_mb_rounded);

            if (!secondlevel_table) {
                /* No pages exist in the section. Why's it mapped then?? */
                assert(false);
                return false;
            }

            break;

        /* There are no other defined mapping types */
        default:
            assert(false);
            return false;
            break;
    }

    if ((secondlevel_table->ptes->ptes[virt_pg_idx] & PT_SECONDLEVEL_MAPTYPE_MASK) != PT_SECONDLEVEL_MAPTYPE_SMALL_PAGE) {
        return false;
    }

    secondlevel_table->ptes->ptes[virt_pg_idx] = PT_SECONDLEVEL_MAPTYPE_UNMAPPED;
    secondlevel_table->num_mapped_pages--;

    /* If no pages are used in the secondlevel table, clean it up */
    if (secondlevel_table->num_mapped_pages < 1) {
        this->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] &= ~PT_FIRSTLEVEL_MAPTYPE_MASK;
        this->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] |= PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;

        secondlevel_table = this->sparse_secondlevel_map->Remove(virt_mb_rounded);

        assert(secondlevel_table != NULL);
    }

    return true;
}

ssize_t TranslationTable::CopyWithAddressSpaces (
        TranslationTable *  source_tt,
        const void *        source_buf,
        size_t              source_len,
        TranslationTable *  dest_tt,
        void *              dest_buf,
        size_t              dest_len
        )
{
    size_t len;

    len = MIN(source_len, dest_len);

    if (source_tt == dest_tt && false) {
        /* Sender and receiver both in same address space. */
        memcpy(dest_buf, source_buf, len);
    }
    else {

        size_t      remaining;
        VmAddr_t    src_cursor  = (VmAddr_t)source_buf;
        VmAddr_t    dst_cursor  = (VmAddr_t)dest_buf;

        /*
        Loop once for each contiguous chunk that can be copied from a
        source-address-space page to a destination-address-space page.
        */
        for (remaining = len; remaining > 0;) {

            VmAddr_t src_mb;
            VmAddr_t dst_mb;

            PhysAddr_t src_phys;
            PhysAddr_t dst_phys;

            size_t src_valid_len;
            size_t dst_valid_len;

            pt_firstlevel_t src_firstlevel_pte;
            pt_firstlevel_t dst_firstlevel_pte;

            pt_secondlevel_t *src_secondlevel_base;
            pt_secondlevel_t *dst_secondlevel_base;

            pt_secondlevel_t src_secondlevel_pte;
            pt_secondlevel_t dst_secondlevel_pte;

            bool src_access;
            bool dst_access;

            src_mb = src_cursor & MEGABYTE_MASK;
            dst_mb = dst_cursor & MEGABYTE_MASK;

            src_firstlevel_pte = source_tt->firstlevel_ptes[src_mb >> MEGABYTE_SHIFT];
            dst_firstlevel_pte = dest_tt->firstlevel_ptes[dst_mb >> MEGABYTE_SHIFT];

            /* Figure out physical address of source buffer chunk */
            switch (src_firstlevel_pte & PT_FIRSTLEVEL_MAPTYPE_MASK) {

                case PT_FIRSTLEVEL_MAPTYPE_SECTION:

                    src_phys = (src_firstlevel_pte & PT_FIRSTLEVEL_SECTION_BASE_ADDR_MASK) + (src_cursor & ~MEGABYTE_MASK);

                    src_valid_len = (1 << MEGABYTE_SHIFT) - (src_cursor & ~MEGABYTE_MASK);

                    src_access = check_access(
                            (src_firstlevel_pte & PT_FIRSTLEVEL_SECTION_AP_MASK) >> PT_FIRSTLEVEL_SECTION_AP_SHIFT
                            );
                    break;

                case PT_FIRSTLEVEL_MAPTYPE_COARSE:

                    src_secondlevel_base = (pt_secondlevel_t *)P2V(
                            src_firstlevel_pte &
                            PT_FIRSTLEVEL_COARSE_BASE_ADDR_MASK
                            );

                    assert((VmAddr_t)src_secondlevel_base != P2V((PhysAddr_t)NULL));

                    src_secondlevel_pte = src_secondlevel_base[(src_cursor & ~MEGABYTE_MASK) >> PAGE_SHIFT];

                    src_phys = (src_secondlevel_pte & PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_MASK) + (src_cursor & ~PAGE_MASK);

                    src_valid_len = PAGE_SIZE - (src_cursor & ~PAGE_MASK);

                    src_access = check_access(
                            (src_secondlevel_pte & PT_SECONDLEVEL_AP0_MASK) >> PT_SECONDLEVEL_AP0_SHIFT
                            );
                    break;

                default:
                    src_access = false;
                    break;
            }

            /* Figure out physical address of destination buffer chunk */
            switch (dst_firstlevel_pte & PT_FIRSTLEVEL_MAPTYPE_MASK) {

                case PT_FIRSTLEVEL_MAPTYPE_SECTION:

                    dst_phys = (dst_firstlevel_pte & PT_FIRSTLEVEL_SECTION_BASE_ADDR_MASK) + (dst_cursor & ~MEGABYTE_MASK);

                    dst_valid_len = (1 << MEGABYTE_SHIFT) - (dst_cursor & ~MEGABYTE_MASK);

                    dst_access = check_access(
                            (dst_firstlevel_pte & PT_FIRSTLEVEL_SECTION_AP_MASK) >> PT_FIRSTLEVEL_SECTION_AP_SHIFT
                            );
                    break;

                case PT_FIRSTLEVEL_MAPTYPE_COARSE:

                    dst_secondlevel_base = (pt_secondlevel_t *)P2V(
                            dst_firstlevel_pte &
                            PT_FIRSTLEVEL_COARSE_BASE_ADDR_MASK
                            );

                    assert((VmAddr_t)dst_secondlevel_base != P2V((PhysAddr_t)NULL));

                    dst_secondlevel_pte = dst_secondlevel_base[(dst_cursor & ~MEGABYTE_MASK) >> PAGE_SHIFT];

                    dst_phys = (dst_secondlevel_pte & PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_MASK) + (dst_cursor & ~PAGE_MASK);

                    dst_valid_len = PAGE_SIZE - (dst_cursor & ~PAGE_MASK);

                    dst_access = check_access(
                            (dst_secondlevel_pte & PT_SECONDLEVEL_AP0_MASK) >> PT_SECONDLEVEL_AP0_SHIFT
                            );

                    break;

                default:
                    dst_access = false;
                    break;
            }

            if (!src_access || !dst_access) {
                return -ERROR_FAULT;
            }

            size_t chunk_size = MIN(
                    remaining,
                    MIN(src_valid_len, dst_valid_len)
                    );

            /*
            The kernel memory map contains all of physical memory, so we can
            just use plain memcpy() now that we've figured out the appropriate
            segments of physical memory to more to/from.
            */
            memcpy((void *)P2V(dst_phys), (void *)P2V(src_phys), chunk_size);

            remaining   -= chunk_size;
            src_cursor  += chunk_size;
            dst_cursor  += chunk_size;
        }
    }

    return len;
}

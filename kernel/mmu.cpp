#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <sys/arch.h>
#include <sys/bits.h>
#include <sys/compiler.h>
#include <sys/error.h>
#include <sys/spinlock.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/minmax.h>
#include <kernel/mmu.hpp>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

#define ARM_MMU_ENABLED_BIT             0
#define ARM_MMU_EXCEPTION_VECTOR_BIT    13

static struct SecondlevelTable * secondlevel_table_alloc ();
static void secondlevel_table_free (struct SecondlevelTable *);

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
    assert(1 << (ffs(KERNEL_MODE_OFFSET) - 1) == KERNEL_MODE_OFFSET);

    n = 32 - (ffs(KERNEL_MODE_OFFSET) - 1);
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

/* Allocates translation_table's */
static struct ObjectCache   translation_table_cache;
static Spinlock_t           translation_table_cache_lock = SPINLOCK_INIT;

/* Allocates secondlevel_table's */
static struct ObjectCache   secondlevel_table_cache;
static Spinlock_t           secondlevel_table_cache_lock = SPINLOCK_INIT;

/* Allocates secondlevel_ptes's */
static struct ObjectCache   secondlevel_ptes_cache;
static Spinlock_t           secondlevel_ptes_cache_lock = SPINLOCK_INIT;

static Once_t mmu_init_control = ONCE_INIT;

static void mmu_static_init (void * ignored)
{
    ObjectCacheInit(&translation_table_cache, sizeof(struct TranslationTable));
    ObjectCacheInit(&secondlevel_table_cache, sizeof(struct SecondlevelTable));
    ObjectCacheInit(&secondlevel_ptes_cache, sizeof(struct SecondlevelPtes));
}

static struct SecondlevelTable * secondlevel_table_alloc ()
{
    struct SecondlevelTable * table;

    SpinlockLock(&secondlevel_table_cache_lock);
    table = (struct SecondlevelTable *)ObjectCacheAlloc(&secondlevel_table_cache);
    SpinlockUnlock(&secondlevel_table_cache_lock);

    if (table)
    {
        unsigned int i;

        table->link.DynamicInit();

        SpinlockLock(&secondlevel_ptes_cache_lock);
        table->ptes = (struct SecondlevelPtes *)ObjectCacheAlloc(&secondlevel_ptes_cache);
        SpinlockUnlock(&secondlevel_ptes_cache_lock);

        if (!table->ptes)
        {
            SpinlockLock(&secondlevel_table_cache_lock);
            ObjectCacheFree(&secondlevel_table_cache, table);
            SpinlockUnlock(&secondlevel_table_cache_lock);

            table = NULL;
        }
        else
        {
            assert(N_ELEMENTS(table->ptes->ptes) == 256);

            for (i = 0; i < N_ELEMENTS(table->ptes->ptes); i++) {
                table->ptes->ptes[i] = PT_SECONDLEVEL_MAPTYPE_UNMAPPED;
            }

            table->num_mapped_pages = 0;
        }
    }

    return table;
}

static void secondlevel_table_free (struct SecondlevelTable * table)
{
    if (!table->link.Unlinked()) {
        List<SecondlevelTable, &SecondlevelTable::link>::Remove(table);
    }

    SpinlockLock(&secondlevel_ptes_cache_lock);
    ObjectCacheFree(&secondlevel_ptes_cache, table->ptes);
    SpinlockUnlock(&secondlevel_ptes_cache_lock);

    SpinlockLock(&secondlevel_table_cache_lock);
    ObjectCacheFree(&secondlevel_table_cache, table);
    SpinlockUnlock(&secondlevel_table_cache_lock);
}

struct TranslationTable * TranslationTableAlloc (void)
{
    enum {
        /**
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

    COMPILER_ASSERT(TRANSLATION_TABLE_SIZE == 4096 * 4);

    struct TranslationTable * table;
    unsigned int i;

    Once(&mmu_init_control, mmu_static_init, NULL);

    SpinlockLock(&translation_table_cache_lock);
    table = (struct TranslationTable *)ObjectCacheAlloc(&translation_table_cache);
    SpinlockUnlock(&translation_table_cache_lock);

    if (!table) {
        return NULL;
    }

    /* Translation table is 16-KB aligned and 4 pages long. */
    table->firstlevel_ptes_pages = Page::Alloc(TRANSLATION_TABLE_PAGES_ORDER);

    if (!table->firstlevel_ptes_pages) {
        goto cleanup_table;
    }

    table->firstlevel_ptes = (pt_firstlevel_t *)table->firstlevel_ptes_pages->base_address;

    table->sparse_secondlevel_map = new TranslationTable::SparseSecondlevelMap_t(TranslationTable::SparseSecondlevelMap_t::AddressCompareFunc);

    if (!table->sparse_secondlevel_map) {
        goto cleanup_pages;
    }

    /* Initially make all sections unmapped */
    for (i = 0; i < TRANSLATION_TABLE_SIZE / sizeof(pt_firstlevel_t); i++) {
        table->firstlevel_ptes[i] = PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;
    }

    /* Mark address of first unused page */
    table->first_unmapped_page = 0;

    /* Successful case */
    return table;

    /* Failure cases */
cleanup_pages:
    Page::Free(table->firstlevel_ptes_pages);

cleanup_table:
    SpinlockLock(&translation_table_cache_lock);
    ObjectCacheFree(&translation_table_cache, table);
    SpinlockUnlock(&translation_table_cache_lock);

    return NULL;
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

void TranslationTableFree (struct TranslationTable * table)
{
    /* Aggregates any individual secondlevel_table's that need freed */
    List<SecondlevelTable, &SecondlevelTable::link> head;

    Once(&mmu_init_control, mmu_static_init, NULL);

    /* Clean out any individual second-level translation tables */

    /* Go ahead and gather up all the nodes */
    table->sparse_secondlevel_map->Foreach(func, &head);

    /* Deallocate everything we found in there */
    while (!head.Empty())
    {
        secondlevel_table_free(head.PopFirst());
    }

    table->firstlevel_ptes = NULL;
    delete table->sparse_secondlevel_map;
    Page::Free(table->firstlevel_ptes_pages);

    SpinlockLock(&translation_table_cache_lock);
    ObjectCacheFree(&translation_table_cache, table);
    SpinlockUnlock(&translation_table_cache_lock);
}

static struct TranslationTable * kernel_translation_table = NULL;

struct TranslationTable * MmuGetKernelTranslationTable ()
{
    return kernel_translation_table;
}

void MmuSetKernelTranslationTable (struct TranslationTable * table)
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

static struct TranslationTable * user_translation_table = NULL;

struct TranslationTable * MmuGetUserTranslationTable ()
{
    return user_translation_table;
}

void MmuSetUserTranslationTable (struct TranslationTable * table)
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

bool TranslationTableMapSection (
        struct TranslationTable * table,
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
    if ((table->firstlevel_ptes[virt_idx] & PT_FIRSTLEVEL_MAPTYPE_MASK) != PT_FIRSTLEVEL_MAPTYPE_UNMAPPED) {
        return false;
    }

    /* Install the mapping */
    table->firstlevel_ptes[virt_idx] =
            PT_FIRSTLEVEL_MAPTYPE_SECTION |
            (PT_DOMAIN_DEFAULT << PT_FIRSTLEVEL_DOMAIN_SHIFT) |
            (ap_from_prot(prot) << PT_FIRSTLEVEL_SECTION_AP_SHIFT) |
            ((phys_idx << MEGABYTE_SHIFT) & PT_FIRSTLEVEL_SECTION_BASE_ADDR_MASK);

    return true;
}

bool TranslationTableUnmapSection (
        struct TranslationTable * table,
        VmAddr_t virt
        )
{
    unsigned int virt_idx;

    assert(virt % SECTION_SIZE == 0);

    virt_idx = virt >> MEGABYTE_SHIFT;

    switch (table->firstlevel_ptes[virt_idx] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            table->firstlevel_ptes[virt_idx] = PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;
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

bool TranslationTableMapNextPage (
        struct TranslationTable * table,
        VmAddr_t * pVirt,
        PhysAddr_t phys,
        Prot_t prot
        )
{
    bool        ret;
    VmAddr_t    vmaddr;

    vmaddr = table->first_unmapped_page;

    ret = TranslationTableMapPage(
            table,
            vmaddr,
            phys,
            prot
            );

    if (ret) {
        *pVirt = vmaddr;
    }

    return ret;
}

bool TranslationTableMapPage (
        struct TranslationTable * table,
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
    switch (table->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_UNMAPPED:
            break;

        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            /* This virtual address range already used by a 1MB section map */
            return false;

        case PT_FIRSTLEVEL_MAPTYPE_COARSE:
        {
            secondlevel_table = table->sparse_secondlevel_map->Lookup(virt_mb_rounded);

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

        if ((secondlevel_table = secondlevel_table_alloc()) == NULL) {
            return false;
        }

        table->sparse_secondlevel_map->Insert(virt_mb_rounded, secondlevel_table);

        table->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] =
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

    /* Update cursor to next available page */
    if (virt >= table->first_unmapped_page) {
        table->first_unmapped_page = virt + PAGE_SIZE;
    }

    return true;
}

bool TranslationTableUnmapPage (
        struct TranslationTable * table,
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
    switch (table->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] & PT_FIRSTLEVEL_MAPTYPE_MASK)
    {
        case PT_FIRSTLEVEL_MAPTYPE_UNMAPPED:
        case PT_FIRSTLEVEL_MAPTYPE_SECTION:
            return false;
            break;

        case PT_FIRSTLEVEL_MAPTYPE_COARSE:

            secondlevel_table = table->sparse_secondlevel_map->Lookup(virt_mb_rounded);

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
        table->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] &= ~PT_FIRSTLEVEL_MAPTYPE_MASK;
        table->firstlevel_ptes[virt_mb_rounded >> MEGABYTE_SHIFT] |= PT_FIRSTLEVEL_MAPTYPE_UNMAPPED;

        secondlevel_table = table->sparse_secondlevel_map->Remove(virt_mb_rounded);

        assert(secondlevel_table != NULL);
    }

    /* If this was the highest-mapped page, then decrement the next-page pointer */
    if (table->first_unmapped_page == virt + PAGE_SIZE) {
        table->first_unmapped_page = virt;
    }

    return true;
}

ssize_t CopyWithAddressSpaces (
        struct TranslationTable *   source_tt,
        const void *                source_buf,
        size_t                      source_len,
        struct TranslationTable *   dest_tt,
        void *                      dest_buf,
        size_t                      dest_len
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

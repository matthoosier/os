#ifndef __MMU_HPP__
#define __MMU_HPP__

#include <stdint.h>
#include <stdbool.h>

#include <sys/decls.h>

#include <kernel/list.hpp>
#include <kernel/mmu-defs.h>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

BEGIN_DECLS

struct SecondlevelTable;

struct TranslationTable
{
    /* Provides the storage pointed at by 'translation_base' below */
    Page * firstlevel_ptes_pages;

    /*
     * Points to a pt_firstlevel_t[4096].
     *
     * First element must be 16KB-aligned.
     */
    pt_firstlevel_t * firstlevel_ptes;

    /*
     * Sparse map of beginning virtual address of each section
     * to the SecondlevelTable instance that fills in the individual
     * pages for that section.
     *
     * Each struct contained in this list controls the mappings for
     * 1MB worth of individual pages.
     *
     * A sample couple entries in this map might be:
     *
     *    First section (0x00000000 - 0x000fffff):
     *        0x00000000 -> (SecondlevelTable *)<struct address>
     *    ...
     *
     *    Fifteenth section (0x00e00000 - 0x00efffff)
     *        0x00e00000 -> (SecondlevelTable *)<struct address>
     */
    typedef TreeMap<VmAddr_t, struct SecondlevelTable *> SparseSecondlevelMap_t;
    SparseSecondlevelMap_t * sparse_secondlevel_map;

    /*
     * Virtual address of the first byte of the first page in the virtual
     * address space, that is not mapped.
     */
    VmAddr_t first_unmapped_page;
};

struct SecondlevelTable
{
    /*
     * The array of individual pagetable entries used by the MMU.
     *
     * Allocated separately because of strict 1KB alignment-boundary
     * requirements. Logically it's just part of this data structure
     */
    struct SecondlevelPtes * ptes;

    /*
     * Utility field for inserting into whatever list is needed
     */
    ListElement link;

    unsigned int num_mapped_pages;
};

struct SecondlevelPtes
{
    /*
    Each element describes one 4KB page.

    256 of these for each 1MB section mapping.
    First element out of each 256 must be 1KB-aligned
    */
    pt_secondlevel_t ptes[256];
};

END_DECLS

#endif /* __MMU_HPP__ */

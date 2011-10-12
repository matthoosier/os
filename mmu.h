#ifndef __MMU_H__
#define __MMU_H__

#include <stdint.h>
#include <stdbool.h>
#include "decls.h"
#include "tree-map.h"
#include "vm.h"

/*
 * Domains are a coarse-grained way to control access to great swaths of
 * memory at one time.
 *
 * Each 1MB chunk of the virtual address space can be configured to
 * be accessible only when the processor is configured to be operating
 * in a certain "domain". By switching the single register controlling
 * the processor's current domain, all pages in that domain instantly
 * become valid. This with no cost to flushing the TLB.
 *
 * We don't really care about this model for a protected-mode multitasker,
 * so we'll just put everything in the same domain.
 */
#define PT_DOMAIN_DEFAULT           0

#define PT_DOMAIN_ACCESS_LEVEL_ALL  0b11

extern int MmuGetEnabled (void);
extern void MmuSetEnabled ();

extern void MmuFlushTlb (void);

typedef uint32_t pt_firstlevel_t;
typedef uint32_t pt_secondlevel_t;

struct TranslationTable;

extern struct TranslationTable * TranslationTableAlloc (void);
extern void TranslationTableFree (struct TranslationTable * table);

extern struct TranslationTable * MmuGetKernelTranslationTable ();
extern void MmuSetKernelTranslationTable (struct TranslationTable * table);

extern struct TranslationTable * MmuGetUserTranslationTable ();
extern void MmuSetUserTranslationTable (struct TranslationTable * table);

extern bool TranslationTableMapPage (
        struct TranslationTable * table,
        VmAddr_t virt,
        PhysAddr_t phys
        );

extern bool TranslationTableMapSection (
        struct TranslationTable * table,
        VmAddr_t virt,
        PhysAddr_t phys
        );

struct TranslationTable
{
    /* Provides the storage pointed at by 'translation_base' below */
    struct Page * firstlevel_ptes_pages;

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
    struct TreeMap * sparse_secondlevel_map;
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
    struct list_head link;
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

/*
 * Common pieces for all firstlevel translation-table entries
 */
enum
{
    PT_FIRSTLEVEL_MAPTYPE_SHIFT = 0,
    PT_FIRSTLEVEL_MAPTYPE_BITS  = 2,
    PT_FIRSTLEVEL_MAPTYPE_MASK  = (0b11 << PT_FIRSTLEVEL_MAPTYPE_SHIFT),

    PT_FIRSTLEVEL_MAPTYPE_UNMAPPED  = (0b00 << PT_FIRSTLEVEL_MAPTYPE_SHIFT),
    PT_FIRSTLEVEL_MAPTYPE_SECTION   = (0b10 << PT_FIRSTLEVEL_MAPTYPE_SHIFT),
    PT_FIRSTLEVEL_MAPTYPE_COARSE    = (0b01 << PT_FIRSTLEVEL_MAPTYPE_SHIFT),
    PT_FIRSTLEVEL_MAPTYPE_FINE      = (0b11 << PT_FIRSTLEVEL_MAPTYPE_SHIFT),

    PT_FIRSTLEVEL_DOMAIN_SHIFT  = 5,
    PT_FIRSTLEVEL_DOMAIN_BITS   = 4,
    PT_FIRSTLEVEL_DOMAIN_MASK   = (0b1111 << PT_FIRSTLEVEL_DOMAIN_SHIFT),
};


/*
 * Pieces specific to a firstlevel translation-table section entry
 */
enum
{
    PT_FIRSTLEVEL_SECTION_BASE_ADDR_SHIFT = 20,
    PT_FIRSTLEVEL_SECTION_BASE_ADDR_BITS  = 12,
    PT_FIRSTLEVEL_SECTION_BASE_ADDR_MASK  = (0xfff << PT_FIRSTLEVEL_SECTION_BASE_ADDR_SHIFT),

    PT_FIRSTLEVEL_SECTION_AP_SHIFT              = 10,
    PT_FIRSTLEVEL_SECTION_AP_BITS               = 2,
    PT_FIRSTLEVEL_SECTION_AP_MASK               = (0b11 << PT_FIRSTLEVEL_SECTION_AP_SHIFT),

    PT_FIRSTLEVEL_SECTION_AP_NONE               = 0b00 << PT_FIRSTLEVEL_SECTION_AP_SHIFT,
    PT_FIRSTLEVEL_SECTION_AP_PRIV_ONLY          = 0b01 << PT_FIRSTLEVEL_SECTION_AP_SHIFT,
    PT_FIRSTLEVEL_SECTION_AP_PRIV_AND_USER_READ = 0b10 << PT_FIRSTLEVEL_SECTION_AP_SHIFT,
    PT_FIRSTLEVEL_SECTION_AP_FULL               = 0b11 << PT_FIRSTLEVEL_SECTION_AP_SHIFT,
};

/*
 * Pieces specific to a firstlevel translation-table coarse pagetable descriptor
 */
enum
{
    PT_FIRSTLEVEL_COARSE_BASE_ADDR_SHIFT    = 10,
    PT_FIRSTLEVEL_COARSE_BASE_ADDR_BITS     = 22,
    PT_FIRSTLEVEL_COARSE_BASE_ADDR_MASK     = (0x3fffff << PT_FIRSTLEVEL_COARSE_BASE_ADDR_SHIFT),
};

/*
 * Common pieces for all secondlevel translation-table entries
 */
enum
{
    /* Maptype */
    PT_SECONDLEVEL_MAPTYPE_SHIFT        = 0,
    PT_SECONDLEVEL_MAPTYPE_BITS         = 2,
    PT_SECONDLEVEL_MAPTYPE_MASK         = (0b11 << PT_SECONDLEVEL_MAPTYPE_SHIFT),

    PT_SECONDLEVEL_MAPTYPE_UNMAPPED     = (0b00 << PT_SECONDLEVEL_MAPTYPE_SHIFT),
    PT_SECONDLEVEL_MAPTYPE_SMALL_PAGE   = (0b10 << PT_SECONDLEVEL_MAPTYPE_SHIFT),

    /* Access permissions */
    PT_SECONDLEVEL_AP_SHIFT                 = 4,
    PT_SECONDLEVEL_AP_BITS                  = 2,
    PT_SECONDLEVEL_AP_MASK                  = (0b11 << PT_SECONDLEVEL_AP_SHIFT),

    PT_SECONDLEVEL_AP_NONE                  = 0b00 << PT_SECONDLEVEL_AP_SHIFT,
    PT_SECONDLEVEL_AP_PRIV_ONLY             = 0b01 << PT_SECONDLEVEL_AP_SHIFT,
    PT_SECONDLEVEL_AP_PRIV_AND_USER_READ    = 0b10 << PT_SECONDLEVEL_AP_SHIFT,
    PT_SECONDLEVEL_AP_FULL                  = 0b11 << PT_SECONDLEVEL_AP_SHIFT,
};

/*
 * Pieces specific to a secondlevel translation-table "small" (4KB) page entry
 */
enum
{
    PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_SHIFT   = 12,
    PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_BITS    = 20,
    PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_MASK    = (0xfffff << PT_SECONDLEVEL_SMALL_PAGE_BASE_ADDR_SHIFT),
};

#endif /* __MMU_H__ */

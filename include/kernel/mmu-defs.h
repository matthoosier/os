#ifndef __MMU_DEFS_H__
#define __MMU_DEFS_H__

#include <stdint.h>
#include <stdbool.h>

#include <sys/decls.h>

#include <kernel/vm-defs.h>

BEGIN_DECLS

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

#define PT_DOMAIN_ACCESS_LEVEL_ALL      0b11
#define PT_DOMAIN_ACCESS_LEVEL_CLIENT   0b01

typedef enum
{
    PROT_NONE           = 0b000,
    PROT_KERNEL         = 0b001,
    PROT_USER_READONLY  = 0b010,
    PROT_USER_READWRITE = 0b100,
} Prot_t;

typedef enum
{
    ADDRESSING_MODE_USER,
    ADDRESSING_MODE_KERNEL,
} AddressingMode_t;

extern int MmuGetEnabled (void);
extern void MmuSetEnabled ();

extern void MmuFlushTlb (void);

typedef uint32_t pt_firstlevel_t;
typedef uint32_t pt_secondlevel_t;

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

    /* First subpage Access permissions */
    PT_SECONDLEVEL_AP0_SHIFT                = 4,
    PT_SECONDLEVEL_AP0_BITS                 = 2,
    PT_SECONDLEVEL_AP0_MASK                 = (0b11 << PT_SECONDLEVEL_AP0_SHIFT),

    PT_SECONDLEVEL_AP0_NONE                 = 0b00 << PT_SECONDLEVEL_AP0_SHIFT,
    PT_SECONDLEVEL_AP0_PRIV_ONLY            = 0b01 << PT_SECONDLEVEL_AP0_SHIFT,
    PT_SECONDLEVEL_AP0_PRIV_AND_USER_READ   = 0b10 << PT_SECONDLEVEL_AP0_SHIFT,
    PT_SECONDLEVEL_AP0_FULL                 = 0b11 << PT_SECONDLEVEL_AP0_SHIFT,

    /* Second subpage Access permissions */
    PT_SECONDLEVEL_AP1_SHIFT                = PT_SECONDLEVEL_AP0_SHIFT + PT_SECONDLEVEL_AP0_BITS,
    PT_SECONDLEVEL_AP1_BITS                 = 2,
    PT_SECONDLEVEL_AP1_MASK                 = (0b11 << PT_SECONDLEVEL_AP1_SHIFT),

    PT_SECONDLEVEL_AP1_NONE                 = 0b00 << PT_SECONDLEVEL_AP1_SHIFT,
    PT_SECONDLEVEL_AP1_PRIV_ONLY            = 0b01 << PT_SECONDLEVEL_AP1_SHIFT,
    PT_SECONDLEVEL_AP1_PRIV_AND_USER_READ   = 0b10 << PT_SECONDLEVEL_AP1_SHIFT,
    PT_SECONDLEVEL_AP1_FULL                 = 0b11 << PT_SECONDLEVEL_AP1_SHIFT,

    /* Third subpage Access permissions */
    PT_SECONDLEVEL_AP2_SHIFT                = PT_SECONDLEVEL_AP1_SHIFT + PT_SECONDLEVEL_AP1_BITS,
    PT_SECONDLEVEL_AP2_BITS                 = 2,
    PT_SECONDLEVEL_AP2_MASK                 = (0b11 << PT_SECONDLEVEL_AP2_SHIFT),

    PT_SECONDLEVEL_AP2_NONE                 = 0b00 << PT_SECONDLEVEL_AP2_SHIFT,
    PT_SECONDLEVEL_AP2_PRIV_ONLY            = 0b01 << PT_SECONDLEVEL_AP2_SHIFT,
    PT_SECONDLEVEL_AP2_PRIV_AND_USER_READ   = 0b10 << PT_SECONDLEVEL_AP2_SHIFT,
    PT_SECONDLEVEL_AP2_FULL                 = 0b11 << PT_SECONDLEVEL_AP2_SHIFT,

    /* Fourth subpage Access permissions */
    PT_SECONDLEVEL_AP3_SHIFT                = PT_SECONDLEVEL_AP2_SHIFT + PT_SECONDLEVEL_AP2_BITS,
    PT_SECONDLEVEL_AP3_BITS                 = 2,
    PT_SECONDLEVEL_AP3_MASK                 = (0b11 << PT_SECONDLEVEL_AP3_SHIFT),

    PT_SECONDLEVEL_AP3_NONE                 = 0b00 << PT_SECONDLEVEL_AP3_SHIFT,
    PT_SECONDLEVEL_AP3_PRIV_ONLY            = 0b01 << PT_SECONDLEVEL_AP3_SHIFT,
    PT_SECONDLEVEL_AP3_PRIV_AND_USER_READ   = 0b10 << PT_SECONDLEVEL_AP3_SHIFT,
    PT_SECONDLEVEL_AP3_FULL                 = 0b11 << PT_SECONDLEVEL_AP3_SHIFT,
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

END_DECLS

#endif /* __MMU_DEFS_H__ */

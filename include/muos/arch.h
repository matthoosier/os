#ifndef __MUOS_ARCH_H__
#define __MUOS_ARCH_H__

/*! \file */

#include <stdint.h>

#include <muos/decls.h>

#ifdef __arm__

    /**
     * \brief   Base-2 log of the number bytes in one MMU page.
     *
     * That is, one page is 2<sup>#PAGE_SIZE</sup> bytes long.
     */
    #define PAGE_SHIFT          12

    /**
     * \brief   Mask with which an address can be bitwise-ANDed
     *          in order to round an address down to the nearest
     *          #PAGE_SIZE boundary. 
     */
    #define PAGE_MASK           0xfffff000  /* The 20 most sig. bits */

    /**
     * \brief   The number of bytes contained in the linear range
     *          of memory addressable by the ARM MMU as one "section"
     */
    #define SECTION_SIZE        (1 << MEGABYTE_SHIFT)

    /**
     * \brief   Number of registers kept in context-save area for ARM
     */
    #define REGISTER_COUNT      (16 + 1)

    #define REGISTER_INDEX_R0           0
    #define REGISTER_INDEX_ARG0         (REGISTER_INDEX_R0 + 0)
    #define REGISTER_INDEX_ARG1         (REGISTER_INDEX_R0 + 1)
    #define REGISTER_INDEX_SP           (REGISTER_INDEX_R0 + 13)
    #define REGISTER_INDEX_LR           (REGISTER_INDEX_R0 + 14)
    #define REGISTER_INDEX_PC           (REGISTER_INDEX_R0 + 15)
    #define REGISTER_INDEX_PSR          16

    /**
     * \brief   Fetch the current stack pointer value
     */
    #define CURRENT_STACK_POINTER()                     \
        ({                                              \
        uint32_t sp;                                    \
        asm volatile("mov %[sp], sp": [sp]"=r"(sp));    \
        sp;                                             \
        })

    #define ARM_PSR_I_BIT   7   /* If set, disables normal IRQs */
    #define ARM_PSR_F_BIT   6   /* If set, disables fast IRQs   */

    #define ARM_PSR_I_VALUE (1 << ARM_PSR_I_BIT)
    #define ARM_PSR_F_VALUE (1 << ARM_PSR_F_VALUE)

    #define ARM_PSR_MODE_MASK       0b11111

    #define ARM_PSR_MODE_USR_BITS   0b10000
    #define ARM_PSR_MODE_FIQ_BITS   0b10001
    #define ARM_PSR_MODE_IRQ_BITS   0b10010
    #define ARM_PSR_MODE_SVC_BITS   0b10011
    #define ARM_PSR_MODE_ABT_BITS   0b10111
    #define ARM_PSR_MODE_UND_BITS   0b11011
    #define ARM_PSR_MODE_SYS_BITS   0b11111

    #define ARM_VECTOR_START_VIRTUAL    0xffff0000

#else
    #error
#endif

/**
 * \brief   The number of bytes contained in one MMU page.
 *
 * This is the fundamental unit of currency for all virtual memory
 * management. All memory is allocated, deallocated, mapped, and manipulated
 * in PAGE_SIZE chunks.
 *
 * See #Page for the basic API to manipulate virtual memory pages.
 */
#define PAGE_SIZE                   (1 << PAGE_SHIFT)

/**
 * \brief   Compute the number of pages completely filled
 *          by \em _sz bytes
 */
#define PAGE_COUNT_FROM_SIZE(_sz)   ((_sz) >> PAGE_SHIFT)

/**
 * \brief   Base-2 log of one binary megabyte (2<sup>20</sup> bytes)
 */
#define MEGABYTE_SHIFT              20

/**
 * \brief   Mask with which an address can be bitwise-ANDed
 *          in order to round an address down to the nearest
 *          one-megabyte boundary. 
 */
#define MEGABYTE_MASK               0xfff00000

/**
 * \brief   Compute the nearest address not less than \em _val
 *          which is a multiple of 2<sup>\em _pow</sup>
 */
#define ALIGN(_val, _pow)                                       \
        ((((_val) + ((1 << (_pow)) - 1)) >> (_pow)) << (_pow))

#endif /* __MUOS_ARCH_H__ */

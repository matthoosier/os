#ifndef __ARCH_H__
#define __ARCH_H__

#include <stdint.h>

#include <sys/decls.h>

#ifdef __arm__
    #define PAGE_SHIFT          12          /* One page is 2 ** 12 bytes */
    #define PAGE_MASK           0xfffff000  /* The 20 most sig. bits */

    #define SECTION_SIZE        (1 << MEGABYTE_SHIFT)

    #define REGISTER_COUNT      (16 + 1)

    #define REGISTER_INDEX_ARG0  0
    #define REGISTER_INDEX_ARG1  1
    #define REGISTER_INDEX_SP   13
    #define REGISTER_INDEX_LR   14
    #define REGISTER_INDEX_PC   15
    #define REGISTER_INDEX_PSR  16

    #define CURRENT_STACK_POINTER()                     \
        ({                                              \
        uint32_t sp;                                    \
        asm volatile("mov %[sp], sp": [sp]"=r"(sp));    \
        sp;                                             \
        })

    #define ARM_CPSR_I_BIT  7   /* If set, disables normal IRQs */
    #define ARM_CPSR_F_BIT  6   /* If set, disables fast IRQs   */

#else
    #error
#endif

#define PAGE_SIZE                   (1 << PAGE_SHIFT)
#define PAGE_COUNT_FROM_SIZE(_sz)   ((_sz) >> PAGE_SHIFT)

#define MEGABYTE_SHIFT              20
#define MEGABYTE_MASK               0xfff00000

#define ALIGN(_val, _pow)                                       \
        ((((_val) + ((1 << (_pow)) - 1)) >> (_pow)) << (_pow))

#endif /* __ARCH_H__ */

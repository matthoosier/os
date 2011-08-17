#ifndef __ARCH_H__
#define __ARCH_H__

#include <stdint.h>
#include "decls.h"

#ifdef __arm__
    #define PAGE_SHIFT          12          /* One page is 2 ** 12 bytes */
    #define PAGE_MASK           0xfffff000  /* The 20 most sig. bits */

    #define REGISTER_COUNT      (16 + 1)

    #define REGISTER_INDEX_R0   0
    #define REGISTER_INDEX_SP   13
    #define REGISTER_INDEX_LR   14
    #define REGISTER_INDEX_PC   15
    #define REGISTER_INDEX_PSR  16
#else
    #error
#endif

#define PAGE_SIZE                   (1 << PAGE_SHIFT)
#define PAGE_COUNT_FROM_SIZE(_sz)   ((_sz) >> PAGE_SHIFT)

BEGIN_DECLS

extern int arch_get_version (void);

END_DECLS

#endif /* __ARCH_H__ */

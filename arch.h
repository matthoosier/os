#ifndef __ARCH_H__
#define __ARCH_H__

#include <stdint.h>
#include "decls.h"

#ifdef __arm__
    #define REGISTER_COUNT (16 + 1)

    #define REGISTER_INDEX_R0   0
    #define REGISTER_INDEX_SP   13
    #define REGISTER_INDEX_LR   14
    #define REGISTER_INDEX_PC   15
    #define REGISTER_INDEX_PSR  16
#else
    #error
#endif

BEGIN_DECLS

extern int arch_get_version (void);

END_DECLS

#endif /* __ARCH_H__ */

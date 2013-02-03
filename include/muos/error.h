#ifndef __MUOS_ERROR_H__
#define __MUOS_ERROR_H__

#include <muos/decls.h>

BEGIN_DECLS

typedef enum
{
    ERROR_OK         = 0,
    ERROR_NO_SYS,
    ERROR_INVALID,
    ERROR_NO_MEM,
    ERROR_FAULT,
    ERROR_EXITING,
} Error_t;

END_DECLS

#endif /* __MUOS_ERROR_H__ */

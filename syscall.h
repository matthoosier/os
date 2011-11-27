#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "decls.h"

BEGIN_DECLS

#define SYS_BASE        0x100
#define SYS_NUM_ECHO    (SYS_BASE + 0)

/* Prototypes for userspace syscall stubs */
extern int syscall0 (unsigned int number);
extern int syscall1 (unsigned int number, int arg0);

END_DECLS

#endif /* __SYSCALL_H__ */

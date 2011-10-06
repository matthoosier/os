#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "decls.h"

BEGIN_DECLS

extern int syscall0 (int number);
extern int syscall1 (int number, int arg0);

END_DECLS

#endif /* __SYSCALL_H__ */

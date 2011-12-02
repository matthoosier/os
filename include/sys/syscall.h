#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <sys/decls.h>

BEGIN_DECLS

#define SYS_BASE        0x100
#define SYS_CHANNEL_CREATE  (SYS_BASE + 0)
#define SYS_CHANNEL_DESTROY (SYS_BASE + 1)
#define SYS_CONNECT         (SYS_BASE + 2)
#define SYS_DISCONNECT      (SYS_BASE + 3)
#define SYS_MSGSEND         (SYS_BASE + 4)
#define SYS_MSGRECV         (SYS_BASE + 5)
#define SYS_MSGREPLY        (SYS_BASE + 6)
#define SYS_NUM_ECHO        (SYS_BASE + 7)

/* Prototypes for userspace syscall stubs */
extern int syscall0 (unsigned int number);
extern int syscall1 (unsigned int number, int arg0);
extern int syscall2 (unsigned int number, int arg0, int arg1);
extern int syscall3 (unsigned int number, int arg0, int arg1, int arg2);
extern int syscall4 (unsigned int number, int arg0, int arg1, int arg2, int arg3);
extern int syscall5 (unsigned int number, int arg0, int arg1, int arg2, int arg3, int arg4);

END_DECLS

#endif /* __SYSCALL_H__ */

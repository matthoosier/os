#ifndef __ONCE_H__
#define __ONCE_H__

#include <stdbool.h>

#include <sys/decls.h>
#include <sys/spinlock.h>

BEGIN_DECLS

typedef struct
{
    Spinlock_t  lock;
    bool        done;
} Once_t;

#define ONCE_INIT { SPINLOCK_INIT, false }

typedef void (*OnceFunc) (void * param);

extern void Once (Once_t * control, OnceFunc func, void * param);

END_DECLS

#endif /* __ONCE_H__ */

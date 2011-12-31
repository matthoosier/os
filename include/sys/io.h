#ifndef __IO_H__
#define __IO_H__

#include <stddef.h>
#include <stdint.h>

#include <sys/decls.h>

BEGIN_DECLS

struct IoNotificationSink
{
    int         connection_id;
    void *      arg;
};

typedef const struct IoNotificationSink * (*InterruptHandlerFunc) (void);

typedef int InterruptHandler_t;

InterruptHandler_t InterruptAttach (
        InterruptHandlerFunc func,
        int irq_number
        );

int InterruptDetach (
        InterruptHandler_t id
        );

void * MapPhysical (
        uintptr_t physaddr,
        size_t  len
        );

END_DECLS

#endif /* __IO_H__ */

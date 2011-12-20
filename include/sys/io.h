#ifndef __IO_H__
#define __IO_H__

#include <stdint.h>

#include <sys/decls.h>

BEGIN_DECLS

struct IoNotificationSink
{
    int         pid;
    int         connection_id;
    uint32_t    payload;
};

typedef struct IoNotificationSink * (*InterruptHandlerFunc) (void);

typedef int InterruptHandler_t;

InterruptHandler_t InterruptAttach (
        InterruptHandlerFunc func,
        int irq_number
        );

int InterruptDetach (
        InterruptHandler_t id
        );

END_DECLS

#endif /* __IO_H__ */

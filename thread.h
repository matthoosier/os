#ifndef __THREAD_H__
#define __THREAD_H__

#include "decls.h"
#include "arch.h"
#include "list.h"

BEGIN_DECLS

typedef enum
{
    THREAD_STATE_READY = 0,
    THREAD_STATE_RUNNING,

    /* This isn't a state, just a way to programatically calculate */
    THREAD_STATE_COUNT,
} thread_state;

struct thread
{
    uint32_t    registers[REGISTER_COUNT];

    struct
    {
        void *  ceiling;
        void *  base;
    } stack;

    thread_state state;

    /* For use in scheduling queues. */
    struct list_head queue_link;
};

struct thread_create_args
{
    struct
    {
        void *  ceiling;
        void *  base;
    } stack;

    void (*body) (void);
};

extern void thread_create (struct thread *                      descriptor,
                           const struct thread_create_args *    args);

extern void thread_switch (struct thread *  outgoing,
                           struct thread *  incoming);

extern struct thread * current;

END_DECLS

#endif /* __THREAD_H__ */

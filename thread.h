#ifndef __THREAD_H__
#define __THREAD_H__

#include "decls.h"
#include "arch.h"
#include "list.h"

#define ALIGNED_THREAD_STRUCT_SIZE                                  \
    /* Padded out to multiple of 8 to preserve %sp requirements */  \
    (ALIGN(sizeof(struct thread), 3))

#define THREAD_STRUCT_FROM_SP(_sp)                                  \
    (                                                               \
    (struct thread *)                                               \
    (((_sp) & PAGE_MASK) + PAGE_SIZE - ALIGNED_THREAD_STRUCT_SIZE)  \
    )

/*
We leverage knowledge that the kernel stack is only one page long, to
be able to compute the address of the current thread's struct based
solely on the current stack pointer.

Don't blow your thread stack! This will return a bad result.
*/
#define THREAD_CURRENT() \
    (THREAD_STRUCT_FROM_SP(CURRENT_STACK_POINTER()))

BEGIN_DECLS

typedef enum
{
    THREAD_STATE_READY = 0,
    THREAD_STATE_RUNNING,

    /* This isn't a state, just a way to programatically calculate */
    THREAD_STATE_COUNT,
} thread_state;

/*
The main control and saved-state for kernel threads. Each thread's
instance of this structure is housed inside the top of the VM page
used for the thread's stack. This avoids object allocations and also
makes deducing the current thread easy: just compute the right offset
in the page containing the current stack pointer.
*/
struct thread
{
    uint32_t    registers[REGISTER_COUNT];

    struct
    {
        void *  ceiling;
        void *  base;

        /* If non-NULL, stack was dynamically allocated */
        struct page * page;
    } kernel_stack;

    thread_state state;

    /* For use in scheduling queues. */
    struct list_head queue_link;
};

typedef void (*thread_func)(void * param);

extern struct thread * thread_create (thread_func body, void * param);

extern void thread_switch (struct thread *  outgoing,
                           struct thread *  incoming);

END_DECLS

#endif /* __THREAD_H__ */

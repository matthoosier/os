#ifndef __THREAD_H__
#define __THREAD_H__

#include "decls.h"
#include "arch.h"
#include "list.h"

#define ALIGNED_THREAD_STRUCT_SIZE                                  \
    /* Padded out to multiple of 8 to preserve %sp requirements */  \
    (ALIGN(sizeof(struct Thread), 3))

#define THREAD_STRUCT_FROM_SP(_sp)                                  \
    (                                                               \
    (struct Thread *)                                               \
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
} ThreadState;

/*
The main control and saved-state for kernel threads. Each thread's
instance of this structure is housed inside the top of the VM page
used for the thread's stack. This avoids object allocations and also
makes deducing the current thread easy: just compute the right offset
in the page containing the current stack pointer.
*/
struct Thread
{
    uint32_t    registers[REGISTER_COUNT];

    struct
    {
        void *  ceiling;
        void *  base;

        /* If non-NULL, stack was dynamically allocated */
        struct Page * page;
    } kernel_stack;

    ThreadState state;

    /* For use in scheduling queues. */
    struct list_head queue_link;
};

typedef void (*ThreadFunc)(void * param);

extern struct Thread * ThreadCreate (
        ThreadFunc body,
        void * param
        );

extern void ThreadSwitch (
        struct Thread *  outgoing,
        struct Thread *  incoming
        );

extern void ThreadYield (void);

END_DECLS

#endif /* __THREAD_H__ */

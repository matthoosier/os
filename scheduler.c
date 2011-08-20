#include <stdlib.h>

#include "list.h"
#include "scheduler.h"
#include "thread.h"

#define READY_QUEUE (queue_by_thread_state[THREAD_STATE_READY])

static struct list_head queue_by_thread_state[THREAD_STATE_COUNT] = {
    [THREAD_STATE_READY] = LIST_HEAD_INIT(queue_by_thread_state[THREAD_STATE_READY]),
};

void scheduler_yield (void)
{
    /* No-op if no threads are ready to run. */
    if (!list_empty(&READY_QUEUE)) {
        struct thread * outgoing;
        struct thread * next;

        outgoing = current;
        outgoing->state = THREAD_STATE_READY;
        list_add_tail(&outgoing->queue_link, &READY_QUEUE);

        next = list_first_entry(&READY_QUEUE, struct thread, queue_link);
        list_del_init(&next->queue_link);
        next->state = THREAD_STATE_RUNNING;
        thread_switch(outgoing, next);
    }
}

/* Insert at tail */
void scheduler_queue_insert (thread_state queue_id,
                             struct thread * thread)
{
    list_add_tail(&thread->queue_link, &queue_by_thread_state[queue_id]);
}

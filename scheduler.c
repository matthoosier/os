#include <stdlib.h>

#include "scheduler.h"
#include "thread.h"

#define READY_QUEUE queue_by_thread_state[THREAD_STATE_READY]

static struct thread * queue_by_thread_state[THREAD_STATE_COUNT] = {
    [THREAD_STATE_READY] = NULL,
};

/* Insert at tail */
struct thread * queue_insert (struct thread * head,
                              struct thread * element)
{
    if (!head) {
        return element;
    }
    else {
        head->next = queue_insert(head->next, element);
        return head;
    }
}

/* Remove from head */
struct thread * queue_remove (struct thread *   head,
                              struct thread **  element)
{
    if (!head) {
        *element = NULL;
        return NULL;
    }
    else {
        struct thread * ret;

        *element = head;
        ret = head->next;
        head->next = NULL;
        return ret;
    }
}

int queue_empty (struct thread *    queue_head)
{
    return !queue_head;
}

void scheduler_yield (void)
{
    /* No-op if no threads are ready to run. */
    if (!queue_empty(READY_QUEUE)) {
        struct thread * outgoing;
        struct thread * next;

        outgoing = current;
        outgoing->state = THREAD_STATE_READY;
        READY_QUEUE = queue_insert(READY_QUEUE, outgoing);

        READY_QUEUE = queue_remove(READY_QUEUE, &next);
        next->state = THREAD_STATE_RUNNING;
        thread_switch(outgoing, next);
    }
}

void scheduler_queue_insert (thread_state queue_id,
                             struct thread * thread)
{
    queue_by_thread_state[queue_id] = queue_insert(queue_by_thread_state[queue_id], thread);
}

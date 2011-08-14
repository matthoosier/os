#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "decls.h"
#include "thread.h"

BEGIN_DECLS

extern void scheduler_queue_insert (thread_state queue_id,
                                    struct thread * thread);

extern struct thread * scheduler_queue_remove (thread_state queue_id);

extern void scheduler_yield (void);

END_DECLS

#endif /* __SCHEDULER_H__ */

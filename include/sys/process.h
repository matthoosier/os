#ifndef __SYS_PROCESS_H__
#define __SYS_PROCESS_H__

/*! \file */

#include <sys/decls.h>

BEGIN_DECLS

int GetPid (void);
void Exit (void);
int Spawn (char const path[]);

/**
 * Request the kernel to send a #PULSE_TYPE_CHILD_FINISH
 * message when a child finishes.
 *
 * A freshly installed wait handler will not be active. In order
 * to allow the kernel to reap a process and inform the parent,
 * ChildWaitArm() must be called.
 *
 * \param connection_id     the connection the message should be
 *                          sent from
 *
 * \param pid               the id of the child whose termination
 *                          should be waited on, or #ANY_PID if
 *                          any available child should be waited on
 *
 * \return                  the integer identifier of the resulting
 *                          wait handler. This is used to incrementally
 *                          "reload" the wait handler with ChildWaitArm()
 *                          and eventually to remove the handler with
 *                          ChildWaitDetach().
 */
int ChildWaitAttach (int connection_id, int pid);

/**
 * Instruct the kernel that up to <tt>count</tt> child processes
 * may be reaped by the handler <tt>handler</tt> set up by
 * ChildWaitAttach().
 */
int ChildWaitArm (int handler_id, unsigned int count);

/**
 * Remove a child termination handler installed by ChildWaitAttach().
 */
int ChildWaitDetach (int handler_id);

END_DECLS

#endif /* __SYS_PROCESS_H__ */

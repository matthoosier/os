#ifndef __SYS_PROCESS_H__
#define __SYS_PROCESS_H__

#include <sys/decls.h>

BEGIN_DECLS

int GetPid (void);
void Exit (void);
int Spawn (char const path[]);

int ChildWaitAttach (int connection_id, int pid);

int ChildWaitArm (int handler_id, unsigned int count);

int ChildWaitDetach (int handler_id);

END_DECLS

#endif /* __SYS_PROCESS_H__ */

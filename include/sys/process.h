#ifndef __SYS_PROCESS_H__
#define __SYS_PROCESS_H__

#include <sys/decls.h>

BEGIN_DECLS

int GetPid (void);
void Exit (void);
int Spawn (char const path[]);

END_DECLS

#endif /* __SYS_PROCESS_H__ */

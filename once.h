#ifndef __ONCE_H__
#define __ONCE_H__

typedef int once_t;

#define ONCE_INIT 0

typedef void (*once_func) (void * param);

extern void once (once_t * control, once_func func, void * param);

#endif /* __ONCE_H__ */

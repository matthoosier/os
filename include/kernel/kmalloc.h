#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <stddef.h>

#include <sys/decls.h>

BEGIN_DECLS

void * kmalloc (size_t size);
void kfree (void * ptr, size_t size);

END_DECLS

#endif /* __KMALLOC_H__ */

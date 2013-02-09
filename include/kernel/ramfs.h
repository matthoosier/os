#ifndef __RAMFS_H__
#define __RAMFS_H__

#include <stdbool.h>
#include <stdint.h>

#include <muos/decls.h>

typedef uint8_t const * RamFsBufferPtr;

extern bool RamFsGetImage (const char name[],
                           RamFsBufferPtr * buffer,
                           size_t * len);

#endif /* __RAMFS_H__ */

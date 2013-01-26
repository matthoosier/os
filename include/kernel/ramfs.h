#ifndef __RAMFS_H__
#define __RAMFS_H__

#include <muos/decls.h>

#include <kernel/image.h>

BEGIN_DECLS

extern const struct ImageEntry * RamFsGetImage (const char name[]);

END_DECLS

#endif /* __RAMFS_H__ */

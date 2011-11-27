#ifndef __RAMFS_H__
#define __RAMFS_H__

#include "image.h"
#include "decls.h"

BEGIN_DECLS

extern const struct ImageEntry * RamFsGetImage (const char name[]);

END_DECLS

#endif /* __RAMFS_H__ */

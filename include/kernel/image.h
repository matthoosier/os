#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include <stdint.h>
#include <stddef.h>

#include <sys/decls.h>

BEGIN_DECLS

struct ImageEntry {
    const uint8_t * fileStart;
    size_t          fileLen;
    const char    * fileName;
};

struct Image {
    size_t                      numEntries;
    const struct ImageEntry   * entriesBase;
};

END_DECLS

#endif /* __ARCHIVE_H__ */

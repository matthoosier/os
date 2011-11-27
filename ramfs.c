#include <string.h>
#include "ramfs.h"

extern const struct Image RamFsImage;

const struct ImageEntry * RamFsGetImage (const char name[])
{
    unsigned int i;

    for (i = 0; i < RamFsImage.numEntries; i++) {
        if (strcmp(name, RamFsImage.entriesBase[i].fileName) == 0) {
            return &RamFsImage.entriesBase[i];
        }
    }

    return NULL;
}

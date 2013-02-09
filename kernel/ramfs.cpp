#include <string.h>

#include <kernel/ramfs.h>

bool RamFsGetImage (const char name[],
                    RamFsBufferPtr * buffer,
                    size_t * len)
{
    extern char __RamFsStart;
    extern char __RamFsEnd;

    uint8_t const * cursor = (uint8_t const *)&__RamFsStart;
    uint8_t const * end = (uint8_t const *)&__RamFsEnd;

    while (cursor < end) {

        // Source is unaligned, and big-endian
        uint32_t name_len = (cursor[0] << 24) + (cursor[1] << 16) +
                            (cursor[2] << 8) + cursor[3];
        cursor += sizeof(name_len);

        char const * entry_name = (char const *)cursor;

        cursor += name_len;

        uint32_t payload_len = (cursor[0] << 24) + (cursor[1] << 16) +
                               (cursor[2] << 8) + cursor[3];
        cursor += sizeof(payload_len);

        if (strncmp(name, entry_name, name_len) == 0) {
            *buffer = cursor;
            *len = payload_len;
            return true;
        }

        cursor += payload_len;
    }

    return false;
}

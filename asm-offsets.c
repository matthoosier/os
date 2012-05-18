#include <stddef.h>

#include <kernel/thread.hpp>

#define DEFINE(_id, _val)               \
    asm volatile (                      \
        "\n#define " #_id " %0\n" \
        :                               \
        : "i" (_val)                    \
    )

#define OFFSET(_type, _member)          \
    DEFINE(_type ## __ ## _member, offsetof(_type, _member))

__attribute__((used))
static void unused ()
{
    OFFSET(Thread, process);
}

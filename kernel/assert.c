#include <kernel/assert.h>

void failed_assert (void)
{
}

void assert (bool predicate)
{
    if (!predicate) {
        failed_assert();
    }
}

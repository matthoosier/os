#include <muos/decls.h>

#include <kernel/assert.h>

BEGIN_DECLS

void failed_assert (void);

END_DECLS

void failed_assert (void)
{
}

void assert (bool predicate)
{
    if (!predicate) {
        failed_assert();
    }
}

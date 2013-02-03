#include <errno.h>
#include <unistd.h>

void * _sbrk (int nbytes)
{
    errno = ENOMEM;
    return (void *)-1;
}


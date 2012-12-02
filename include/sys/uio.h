#ifndef __SYS_UIO_H__
#define __SYS_UIO_H__

#include <stddef.h>

struct iovec
{
    void *  iov_base;
    size_t  iov_len;
};

#endif

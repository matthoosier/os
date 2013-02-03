#if defined(__KERNEL__)
    #include <kernel/assert.h>
#else
    #include <muos/process.h>
#endif

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int _read (int file, char * ptr, int len)
{
    errno = EBADF;
    return -1;
}

int _write (int file, char * ptr, int len)
{
    errno = EBADF;
    return -1;
}

int _close (int file)
{
    errno = EBADF;
    return -1;
}

int _fstat (int file, struct stat *st)
{
    errno = EBADF;
    return -1;
}

int _lseek (int file, int offset, int whence)
{
    errno = EBADF;
    return -1;
}

void _exit (int rc)
{
    #if defined(__KERNEL__)
        assert(0);
    #else
        Exit();
    #endif

    while (1) {}
}

int _kill (int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

int _getpid ()
{
    #if defined(__KERNEL__)
        return 1;
    #else
        return GetPid();
    #endif
}

int _isatty (int file)
{
    return 1;
}

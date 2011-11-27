#include <stdint.h>

#include "error.h"
#include "syscall.h"

static int DoEcho (int arg)
{
    return arg;
}

void do_syscall (uint32_t * p_regs)
{
    switch (p_regs[8]) {
        case SYS_NUM_ECHO:
            p_regs[0] = DoEcho(p_regs[0]);
            break;
        default:
            p_regs[0] = -ERROR_NO_SYS;
            break;
    }
}

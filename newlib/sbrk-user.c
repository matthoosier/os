#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include <muos/arch.h>
#include <muos/error.h>
#include <muos/procmgr.h>
#include <muos/message.h>

static uintptr_t user_heap_size = 0;
static uintptr_t kernel_heap_size = 0;
static uint8_t * base;

static void * SbrkUser (int increment)
{
    struct ProcMgrMessage msg;
    struct ProcMgrReply reply;
    int ret;

    intptr_t kernel_increment;

    if (increment <= kernel_heap_size - user_heap_size) {
        user_heap_size += increment;
        return &base[user_heap_size - increment];
    }

    /* Kernel expects increments to be whole-page sized */
    kernel_increment = increment - (kernel_heap_size - user_heap_size);
    kernel_increment = (kernel_increment + PAGE_SIZE - 1);
    kernel_increment /= PAGE_SIZE;
    kernel_increment *= PAGE_SIZE;

    msg.type = PROC_MGR_MESSAGE_SBRK;
    msg.payload.sbrk.increment = kernel_increment;

    ret = MessageSend(PROCMGR_CONNECTION_ID, &msg, sizeof(msg), &reply, sizeof(reply));

    if (ret != sizeof(reply)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    else {

        if (base == NULL) {
            /* First time */
            base = (uint8_t *)reply.payload.sbrk.previous;
        }

        user_heap_size += increment;
        kernel_heap_size += kernel_increment;

        return base + user_heap_size - increment;
    }
}

void * _sbrk (int increment)
{
    /*
    Perform the actual work in a function whose symbolname won't
    have an alter-ego in the kernel. Makes breaking in a debugger
    must easiser.
    */
    return SbrkUser(increment);
}

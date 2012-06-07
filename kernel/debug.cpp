#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#include <kernel/assert.h>
#include <kernel/debug.hpp>
#include <kernel/once.h>

static DebugDriver * gDriver = 0;

void Debug::RegisterDriver (DebugDriver * driver)
{
    assert(gDriver == 0);
    gDriver = driver;
}

static Once_t init_control = ONCE_INIT;

static void do_init (void * ignored)
{
    assert(gDriver != 0);
    gDriver->Init();
}

void Debug::PrintMessage (const char  message[])
{
    assert(gDriver != 0);
    Once(&init_control, do_init, NULL);
    gDriver->PrintMessage(message);
}

const char DEBUG_KERNEL_INTERRUPTED_MESSAGE[] =
        "Interrupting Kernel thread\n"
        "\tPC was 0x%08x\n"
        "\tSP was 0x%08x\n";

const char DEBUG_USER_INTERRUPTED_MESSAGE[] = "Interrupting User process\n";

void printk (const char * format, ...)
{
    static char buf[128];
    static Spinlock_t lock = SPINLOCK_INIT;

    va_list list;

    va_start(list, format);
    SpinlockLock(&lock);
    vsnprintf(buf, sizeof(buf), format, list);
    SpinlockUnlock(&lock);
    va_end(list);

    Debug::PrintMessage(buf);
}

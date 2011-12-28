#include <stdint.h>

#include <sys/error.h>
#include <sys/syscall.h>
#include <sys/procmgr.h>

#include <kernel/message.h>
#include <kernel/process.h>
#include <kernel/thread.h>

static Channel_t DoChannelCreate ()
{
    struct Channel * c = KChannelAlloc();
    Channel_t ret;

    if (c) {
        ret = ProcessRegisterChannel(THREAD_CURRENT()->process, c);

        if (ret < 0) {
            KChannelFree(c);
        }
    } else {
        ret = -ERROR_NO_MEM;
    }

    return ret;
}

static int DoChannelDestroy (Channel_t chid)
{
    struct Channel * c = ProcessLookupChannel(THREAD_CURRENT()->process, chid);
    int ret;

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = ProcessUnregisterChannel(THREAD_CURRENT()->process, chid);

    if (ret >= 0) {
        KChannelFree(c);
    }

    return ret;
}

static Connection_t DoConnect (Pid_t pid, Channel_t chid)
{
    struct Process * other;
    struct Connection * conn;
    struct Channel * chan;
    Connection_t ret;

    other = ProcessLookup(pid);

    if (!other) {
        return -ERROR_INVALID;
    }

    chan = ProcessLookupChannel(other, chid);

    if (!chan) {
        return -ERROR_INVALID;
    }

    conn = KConnect(chan);

    if (!conn) {
        return -ERROR_NO_MEM;
    }

    ret = ProcessRegisterConnection(THREAD_CURRENT()->process, conn);

    if (ret < 0) {
        KDisconnect(conn);
    }

    return ret;
}

static int DoDisconnect (Connection_t coid)
{
    int ret;

    /* Don't allow the process's connection to the Process Manager to be closed */
    if (coid == PROCMGR_CONNECTION_ID) {
        ret = -ERROR_INVALID;
    }
    else {
        ret = ProcessUnregisterConnection(THREAD_CURRENT()->process, coid);
    }
    return ret;
}

static ssize_t DoMessageSend (
        Connection_t coid,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        )
{
    struct Connection * c;
    int ret;

    c = ProcessLookupConnection(THREAD_CURRENT()->process, coid);

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = KMessageSend(c, msgbuf, msgbuf_len, replybuf, replybuf_len);

    return ret;
}

static ssize_t DoMessageReceive (
        Channel_t chid,
        uintptr_t * msgid,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    struct Channel * c;
    struct Message * m;
    int ret;

    c = ProcessLookupChannel(THREAD_CURRENT()->process, chid);

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = KMessageReceive(c, &m, msgbuf, msgbuf_len);

    if (ret < 0) {
        *msgid = -1;
    } else {
        if (m == NULL) {
            *msgid = 0;
        }
        else {
            *msgid = ProcessRegisterMessage(THREAD_CURRENT()->process, m);
        }
    }

    return ret;
}

static ssize_t DoMessageReply (
        uintptr_t msgid,
        unsigned int status,
        void * replybuf,
        size_t replybuf_len
        )
{
    struct Message * m;
    int ret;

    m = ProcessLookupMessage(THREAD_CURRENT()->process, msgid);
    ProcessUnregisterMessage(THREAD_CURRENT()->process, msgid);

    ret = KMessageReply(m, status, replybuf, replybuf_len);

    return ret;
}

static int DoEcho (int arg)
{
    return arg;
}

void do_syscall (uint32_t * p_regs)
{
    switch (p_regs[8]) {

        case SYS_CHANNEL_CREATE:
            p_regs[0] = DoChannelCreate();
            break;

        case SYS_CHANNEL_DESTROY:
            p_regs[0] = DoChannelDestroy(p_regs[0]);
            break;

        case SYS_CONNECT:
            p_regs[0] = DoConnect(p_regs[0], p_regs[1]);
            break;

        case SYS_DISCONNECT:
            p_regs[0] = DoDisconnect(p_regs[0]);
            break;

        case SYS_MSGSEND:
            p_regs[0] = DoMessageSend(
                    p_regs[0],
                    (const void *)p_regs[1],
                    p_regs[2],
                    (void *)p_regs[3],
                    p_regs[4]
                    );
            break;

        case SYS_MSGRECV:
            p_regs[0] = DoMessageReceive(
                    p_regs[0],
                    (uintptr_t *)p_regs[1],
                    (void *)p_regs[2],
                    p_regs[3]
                    );
            break;

        case SYS_MSGREPLY:
            p_regs[0] = DoMessageReply(
                    p_regs[0],
                    p_regs[1],
                    (void *)p_regs[2],
                    p_regs[3]
                    );
            break;

        case SYS_NUM_ECHO:
            p_regs[0] = DoEcho(p_regs[0]);
            break;

        default:
            p_regs[0] = -ERROR_NO_SYS;
            break;
    }

    /*
    Check whether any interrupts handlers decided that the scheduling
    algorithm needs to run and pick a new next task.
    */
    if (ThreadResetNeedResched()) {
        ThreadAddReady(THREAD_CURRENT());
        ThreadYieldNoRequeue();
    }
}


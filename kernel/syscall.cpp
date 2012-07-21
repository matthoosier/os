#include <stdint.h>

#include <sys/error.h>
#include <sys/syscall.h>
#include <sys/procmgr.h>

#include <kernel/message.hpp>
#include <kernel/process.hpp>
#include <kernel/thread.hpp>

static Channel_t DoChannelCreate ()
{
    Channel_t ret;

    try {
        Channel * c = new Channel();

        ret = THREAD_CURRENT()->process->RegisterChannel(c);

        if (ret < 0) {
            delete c;
        }

        return ret;
    }
    catch (std::bad_alloc) {
        return -ERROR_NO_MEM;
    }
}

static int DoChannelDestroy (Channel_t chid)
{
    Channel * c = THREAD_CURRENT()->process->LookupChannel(chid);
    int ret;

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = THREAD_CURRENT()->process->UnregisterChannel(chid);

    if (ret >= 0) {
        delete c;
    }

    return ret;
}

static Connection_t DoConnect (Pid_t pid, Channel_t chid)
{
    Process * other;
    Connection * conn;
    Channel * chan;
    Connection_t ret;

    if (pid == SELF_PID) {
        other = THREAD_CURRENT()->process;
    } else {
        other = Process::Lookup(pid);
    }

    if (!other) {
        return -ERROR_INVALID;
    }

    chan = other->LookupChannel(chid);

    if (!chan) {
        return -ERROR_INVALID;
    }

    try {
        conn = new Connection(chan);
    } catch (std::bad_alloc) {
        return -ERROR_NO_MEM;
    }

    ret = THREAD_CURRENT()->process->RegisterConnection(conn);

    if (ret < 0) {
        delete conn;
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
        ret = THREAD_CURRENT()->process->UnregisterConnection(coid);
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
    Connection * c;
    int ret;

    c = THREAD_CURRENT()->process->LookupConnection(coid);

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = c->SendMessage(msgbuf, msgbuf_len, replybuf, replybuf_len);

    return ret;
}

static ssize_t DoMessageReceive (
        Channel_t chid,
        uintptr_t * msgid,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    Channel * c;
    Message * m;
    int ret;

    c = THREAD_CURRENT()->process->LookupChannel(chid);

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = c->ReceiveMessage(&m, msgbuf, msgbuf_len);

    if (ret < 0) {
        *msgid = -1;
    } else {
        if (m == NULL) {
            *msgid = 0;
        }
        else {
            *msgid = THREAD_CURRENT()->process->RegisterMessage(m);
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
    Message * m;
    int ret;

    m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (m) {
        THREAD_CURRENT()->process->UnregisterMessage(msgid);
        ret = m->Reply(status, replybuf, replybuf_len);
    } else {
        ret = -ERROR_INVALID;
    }

    return ret;
}

static int DoEcho (int arg)
{
    return arg;
}

BEGIN_DECLS
void do_syscall (Thread * current);
END_DECLS

void do_syscall (Thread * current)
{

    #define p_regs  (current->u_reg)

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
    Thread::BeginTransaction();
    if (Thread::ResetNeedResched()) {
        Thread::MakeReady(THREAD_CURRENT());
        Thread::RunNextThread();
    }
    Thread::EndTransaction();
}


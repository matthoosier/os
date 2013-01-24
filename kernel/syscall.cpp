#include <stdint.h>

#include <sys/error.h>
#include <sys/syscall.h>
#include <sys/procmgr.h>

#include <kernel/kmalloc.h>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/process.hpp>
#include <kernel/thread.hpp>

static bool CopyIoVecToIoBuffer (TranslationTable * user_pagetable,
                                 struct iovec const * user_iovec,
                                 TranslationTable * kernel_pagetable,
                                 IoBuffer * kernel_iobuf)
{
    struct iovec k_iovec;

    ssize_t n = TranslationTable::CopyWithAddressSpaces(
            user_pagetable,
            user_iovec,
            sizeof(*user_iovec),
            kernel_pagetable,
            &k_iovec,
            sizeof(k_iovec)
            );

    if (n != sizeof(k_iovec)) {
        return false;
    }

    *kernel_iobuf = IoBuffer(k_iovec.iov_base, k_iovec.iov_len);

    return true;
}

static void checkExit (int messaging_result_code)
{
    if (messaging_result_code == -ERROR_EXITING)
    {
        Process * process = THREAD_CURRENT()->process;
        process->LookupConnection(PROCMGR_CONNECTION_ID)->SendMessageAsync(PULSE_TYPE_CHILD_FINISH, process->GetId());

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_FINISHED);
        Thread::RunNextThread();
        Thread::EndTransaction();

        assert(false);
    }
}

static Channel_t DoChannelCreate ()
{
    try {
        RefPtr<Channel> c(new Channel());

        return THREAD_CURRENT()->process->RegisterChannel(c);
    }
    catch (std::bad_alloc) {
        return -ERROR_NO_MEM;
    }
}

static int DoChannelDestroy (Channel_t chid)
{
    RefPtr<Channel> c = THREAD_CURRENT()->process->LookupChannel(chid);
    int ret;

    if (!c) {
        return -ERROR_INVALID;
    }

    ret = THREAD_CURRENT()->process->UnregisterChannel(chid);
    c->Dispose();

    return ret;
}

static Connection_t DoConnect (Pid_t pid, Channel_t chid)
{
    Process * other;
    RefPtr<Connection> conn;
    RefPtr<Channel> chan;
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
        conn.Reset(new Connection(chan));
    } catch (std::bad_alloc) {
        return -ERROR_NO_MEM;
    }

    ret = THREAD_CURRENT()->process->RegisterConnection(conn);

    return ret;
}

static int DoDisconnect (Connection_t coid)
{
    RefPtr<Connection> con;

    /* Don't allow the process's connection to the Process Manager to be closed */
    if (coid == PROCMGR_CONNECTION_ID) {
        return -ERROR_INVALID;
    }

    con = THREAD_CURRENT()->process->LookupConnection(coid);

    if (!con) {
        return -ERROR_INVALID;
    }

    int ret = THREAD_CURRENT()->process->UnregisterConnection(coid);
    con->Dispose();
    return ret;
}

static ssize_t DoMessageSend (
        Connection_t coid,
        void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        )
{
    RefPtr<Connection> c = THREAD_CURRENT()->process->LookupConnection(coid);

    if (!c) {
        return -ERROR_INVALID;
    }

    int ret = c->SendMessage(IoBuffer(msgbuf, msgbuf_len), IoBuffer(replybuf, replybuf_len));

    checkExit(ret);

    return ret;
}

static ssize_t DoMessageSendV (
        Connection_t coid,
        struct iovec const * user_msgv,
        size_t msgv_count,
        struct iovec const * user_replyv,
        size_t replyv_count
        )
{
    int ret;

    TranslationTable * user_tt;
    TranslationTable * kernel_tt;

    RefPtr<Connection> c = THREAD_CURRENT()->process->LookupConnection(coid);

    if (!c) {
        return -ERROR_INVALID;
    }

    size_t k_msgv_sz = sizeof(IoBuffer) * msgv_count;
    size_t k_replyv_sz = sizeof(IoBuffer) * replyv_count;

    IoBuffer * k_msgv = (IoBuffer *)kmalloc(k_msgv_sz);
    IoBuffer * k_replyv = (IoBuffer *)kmalloc(k_replyv_sz);

    if (!k_msgv || !k_replyv) {
        ret = -ERROR_NO_MEM;
        goto free_bufs;
    }

    user_tt = TranslationTable::GetUser();
    kernel_tt = TranslationTable::GetKernel();

    for (size_t i = 0; i < msgv_count; ++i) {
        if (!CopyIoVecToIoBuffer(user_tt, &user_msgv[i],
                                 kernel_tt, &k_msgv[i]))
        {
            ret = -ERROR_INVALID;
            goto free_bufs;
        }
    }

    for (size_t i = 0; i < replyv_count; ++i) {
        if (!CopyIoVecToIoBuffer(user_tt, &user_replyv[i],
                                 kernel_tt, &k_replyv[i]))
        {
            ret = -ERROR_INVALID;
            goto free_bufs;
        }
    }

    ret = c->SendMessage(k_msgv, msgv_count,
                         k_replyv, replyv_count);

free_bufs:
    if (k_msgv)     kfree(k_msgv, k_msgv_sz);
    if (k_replyv)   kfree(k_replyv, k_replyv_sz);


    checkExit(ret);

    return ret;
}

static ssize_t DoMessageReceive (
        Channel_t chid,
        uintptr_t * msgid,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    RefPtr<Message> m;

    RefPtr<Channel> c = THREAD_CURRENT()->process->LookupChannel(chid);

    if (!c) {
        return -ERROR_INVALID;
    }

    int ret = c->ReceiveMessage(m, msgbuf, msgbuf_len);

    if (ret < 0) {
        *msgid = -1;
    } else {
        if (!m) {
            *msgid = 0;
        }
        else {
            *msgid = THREAD_CURRENT()->process->RegisterMessage(m);
        }
    }

    checkExit(ret);

    return ret;
}

static ssize_t DoMessageReceiveV (
        Channel_t chid,
        uintptr_t * msgid,
        struct iovec const * user_msgv,
        size_t msgv_count
        )
{
    int ret;
    RefPtr<Message> m;
    RefPtr<Channel> c = THREAD_CURRENT()->process->LookupChannel(chid);

    TranslationTable * user_tt = TranslationTable::GetUser();
    TranslationTable * kernel_tt = TranslationTable::GetKernel();

    if (!c) {
        return -ERROR_INVALID;
    }

    size_t k_msgv_sz = sizeof(IoBuffer) * msgv_count;
    IoBuffer * k_msgv = (IoBuffer *)kmalloc(k_msgv_sz);

    if (!k_msgv) {
        ret = -ERROR_NO_MEM;
        goto free_buffers;
    }

    for (size_t i = 0; i < msgv_count; ++i) {
        if (!CopyIoVecToIoBuffer(user_tt, &user_msgv[i],
                                 kernel_tt, &k_msgv[i]))
        {
            ret = -ERROR_INVALID;
            goto free_buffers;
        }
    }

    ret = c->ReceiveMessage(m, k_msgv, msgv_count);

    if (ret < 0) {
        *msgid = -1;
    } else {
        if (!m) {
            *msgid = 0;
        }
        else {
            *msgid = THREAD_CURRENT()->process->RegisterMessage(m);
        }
    }

free_buffers:

    if (k_msgv) {
        kfree(k_msgv, k_msgv_sz);
    }

    checkExit(ret);

    return ret;
}

static size_t DoMessageGetLength (uintptr_t msgid)
{
    RefPtr<Message> m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (m) {
        return m->GetLength();
    } else {
        return -ERROR_INVALID;
    }
}

static ssize_t DoMessageRead (
        uintptr_t msgid,
        size_t src_offset,
        void * dest,
        size_t len
        )
{
    RefPtr<Message> m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (m) {
        return m->Read(src_offset, dest, len);
    } else {
        return -ERROR_INVALID;
    }
}

static ssize_t DoMessageReadV (
        uintptr_t msgid,
        size_t src_offset,
        struct iovec const * user_destv,
        size_t destv_count
        )
{
    int ret;
    TranslationTable * user_tt;
    TranslationTable * kernel_tt;
    size_t k_destv_sz;
    IoBuffer * k_destv = NULL;

    RefPtr<Message> m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (!m) {
        ret = -ERROR_INVALID;
        goto free_buffers;
    }

    k_destv_sz = sizeof(IoBuffer) * destv_count;
    k_destv = (IoBuffer *)kmalloc(k_destv_sz);

    if (!k_destv) {
        ret = -ERROR_NO_MEM;
        goto free_buffers;
    }

    user_tt = TranslationTable::GetUser();
    kernel_tt = TranslationTable::GetKernel();

    for (size_t i = 0; i < destv_count; ++i) {
        if (!CopyIoVecToIoBuffer(user_tt, &user_destv[i],
                                 kernel_tt, &k_destv[i]))
        {
            ret = -ERROR_INVALID;
            goto free_buffers;
        }
    }

    ret = m->Read(src_offset, k_destv, destv_count);

free_buffers:

    if (k_destv) {
        kfree(k_destv, k_destv_sz);
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
    RefPtr<Message> m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (!m) {
        return -ERROR_INVALID;
    }

    THREAD_CURRENT()->process->UnregisterMessage(msgid);
    int ret = m->Reply(status, replybuf, replybuf_len);

    checkExit(ret);

    return ret;
}

static ssize_t DoMessageReplyV (
        uintptr_t msgid,
        unsigned int status,
        struct iovec const * user_replyv,
        size_t replyv_count
        )
{
    int ret;
    RefPtr<Message> m;
    IoBuffer * k_replyv = NULL;
    size_t k_replyv_sz;
    TranslationTable * user_tt;
    TranslationTable * kernel_tt;

    m = THREAD_CURRENT()->process->LookupMessage(msgid);

    if (!m) {
        ret = -ERROR_INVALID;
        goto free_buffers;
    }

    k_replyv_sz = sizeof(IoBuffer) * replyv_count;
    k_replyv = (IoBuffer *)kmalloc(k_replyv_sz);

    if (!k_replyv) {
        ret = -ERROR_NO_MEM;
        goto free_buffers;
    }

    user_tt = TranslationTable::GetUser();
    kernel_tt = TranslationTable::GetKernel();

    for (size_t i = 0; i < replyv_count; ++i) {
        if (!CopyIoVecToIoBuffer (user_tt, &user_replyv[i],
                                  kernel_tt, &k_replyv[i]))
        {
            ret = -ERROR_INVALID;
            goto free_buffers;
        }
    }

    THREAD_CURRENT()->process->UnregisterMessage(msgid);
    ret = m->Reply(status, k_replyv, replyv_count);

free_buffers:

    if (k_replyv) {
        kfree(k_replyv, k_replyv_sz);
    }

    checkExit(ret);

    return ret;
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
                    (Connection_t)p_regs[0],
                    (void *)p_regs[1],
                    (size_t)p_regs[2],
                    (void *)p_regs[3],
                    (size_t)p_regs[4]
                    );
            break;

        case SYS_MSGSENDV:
            p_regs[0] = DoMessageSendV(
                    (Connection_t)p_regs[0],
                    (struct iovec const *)p_regs[1],
                    (size_t)p_regs[2],
                    (struct iovec const *)p_regs[3],
                    (size_t)p_regs[4]
                    );
            break;

        case SYS_MSGRECV:
            p_regs[0] = DoMessageReceive(
                    (Channel_t)p_regs[0],
                    (uintptr_t *)p_regs[1],
                    (void *)p_regs[2],
                    (size_t)p_regs[3]
                    );
            break;

        case SYS_MSGRECVV:
            p_regs[0] = DoMessageReceiveV(
                    (Channel_t)p_regs[0],
                    (uintptr_t *)p_regs[1],
                    (struct iovec const *)p_regs[2],
                    (size_t)p_regs[3]
                    );

        case SYS_MSGGETLEN:
            p_regs[0] = DoMessageGetLength(p_regs[0]);
            break;

        case SYS_MSGREAD:
            p_regs[0] = DoMessageRead(
                    (uintptr_t)p_regs[0],
                    (size_t)p_regs[1],
                    (void *)p_regs[2],
                    (size_t)p_regs[3]
                    );
            break;

        case SYS_MSGREADV:
            p_regs[0] = DoMessageReadV(
                    (uintptr_t)p_regs[0],
                    (size_t)p_regs[1],
                    (struct iovec const *)p_regs[2],
                    (size_t)p_regs[3]
                    );
            break;

        case SYS_MSGREPLY:
            p_regs[0] = DoMessageReply(
                    (uintptr_t)p_regs[0],
                    p_regs[1],
                    (void *)p_regs[2],
                    (size_t)p_regs[3]
                    );
            break;

        case SYS_MSGREPLYV:
            p_regs[0] = DoMessageReplyV(
                    (uintptr_t)p_regs[0],
                    p_regs[1],
                    (struct iovec const *)p_regs[2],
                    (size_t)p_regs[3]
                    );
            break;

        default:
            p_regs[0] = -ERROR_NO_SYS;
            break;
    }
}


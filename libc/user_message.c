#include <sys/message.h>

#include <sys/syscall.h>

int ChannelCreate ()
{
    return syscall0(SYS_CHANNEL_CREATE);
}

int ChannelDestroy (int chid)
{
    return syscall1(SYS_CHANNEL_DESTROY, chid);
}

int Connect (int pid, int chid)
{
    return syscall2(SYS_CONNECT, pid, chid);
}

int Disconnect (int coid)
{
    return syscall1(SYS_DISCONNECT, coid);
}

int MessageSend (
        int coid,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        )
{
    return syscall5(SYS_MSGSEND, coid, (int)msgbuf, msgbuf_len, (int)replybuf, replybuf_len);
}

int MessageReceive (
        int chid,
        int * rcvid,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    return syscall4(SYS_MSGRECV, chid, (int)rcvid, (int)msgbuf, msgbuf_len);
}

int MessageGetLength (int rcvid)
{
    return syscall1(SYS_MSGGETLEN, rcvid);
}

int MessageRead (
        int rcvid,
        size_t src_offset,
        void * dest,
        size_t len)
{
    return syscall4(SYS_MSGREAD, rcvid, (int)src_offset, (int)dest, (int)len);
}

int MessageReply (
        int rcvid,
        unsigned int status,
        void * replybuf,
        size_t replybuf_len
        )
{
    return syscall4(SYS_MSGREPLY, rcvid, status, (int)replybuf, replybuf_len);
}

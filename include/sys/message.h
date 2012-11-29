#ifndef __SYS_MESSAGE_H__
#define __SYS_MESSAGE_H__

#include <sys/types.h>

#include <sys/decls.h>

BEGIN_DECLS

#define SELF_PID            0
#define PROCMGR_PID         1
#define FIRST_CHANNEL_ID    0
#define FIRST_CONNECTION_ID 0

int ChannelCreate ();

int ChannelDestroy (int chid);

int Connect (int pid, int chid);

int Disconnect (int coid);

int MessageSend (
        int coid,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        );

int MessageReceive (
        int chid,
        int * msgid,
        void * msgbuf,
        size_t msgbuf_len
        );

int MessageGetLength (int rcvid);

int MessageRead (
        int msgid,
        size_t src_offset,
        void * dest,
        size_t len);

int MessageReply (
        int msgid,
        unsigned int status,
        void * replybuf,
        size_t replybuf_len
        );

END_DECLS

#endif /* __SYS_MESSAGE_H__ */

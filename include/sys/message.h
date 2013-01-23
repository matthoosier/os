#ifndef __SYS_MESSAGE_H__
#define __SYS_MESSAGE_H__

#include <stdint.h>
#include <sys/types.h>

#include <sys/decls.h>
#include <sys/uio.h>

BEGIN_DECLS

#define SELF_PID            0
#define ANY_PID             -1
#define PROCMGR_PID         1
#define FIRST_CHANNEL_ID    0
#define FIRST_CONNECTION_ID 0

struct Pulse
{
    int8_t      type;
    uint8_t     pad[3];
    uintptr_t   value;
};

#define PULSE_TYPE_MIN_USER 0
#define PULSE_TYPE_MAX_USER __INT8_MAX__

#define PULSE_TYPE_INTERRUPT        (PULSE_TYPE_MIN_USER - 1)
#define PULSE_TYPE_CHILD_FINISH     (PULSE_TYPE_MIN_USER - 2)

int ChannelCreate ();

int ChannelDestroy (int chid);

int Connect (int pid, int chid);

int Disconnect (int coid);

int MessageSend (int coid,
                 const void * msgbuf,
                 size_t msgbuf_len,
                 void * replybuf,
                 size_t replybuf_len);

int MessageSendV (int coid,
                  struct iovec const msgv[],
                  size_t msgv_count,
                  struct iovec const replyv[],
                  size_t replyv_count);

int MessageReceive (int chid,
                    int * msgid,
                    void * msgbuf,
                    size_t msgbuf_len);

int MessageReceiveV (int chid,
                     int * msgid,
                     struct iovec const msgv[],
                     size_t msgv_count);

int MessageGetLength (int rcvid);

int MessageRead (int msgid,
                 size_t src_offset,
                 void * dest,
                 size_t len);

int MessageReadV (int msgid,
                  size_t src_offset,
                  struct iovec const destv[],
                  size_t destv_count);

int MessageReply (int msgid,
                  unsigned int status,
                  void * replybuf,
                  size_t replybuf_len);

int MessageReplyV (int msgid,
                   unsigned int status,
                   struct iovec const replyv[],
                   size_t replyv_count);

END_DECLS

#endif /* __SYS_MESSAGE_H__ */

#ifndef __MUOS_MESSAGE_H__
#define __MUOS_MESSAGE_H__

/*! \file */

#include <stdint.h>
#include <sys/types.h>

#include <muos/decls.h>
#include <muos/uio.h>

BEGIN_DECLS

/**
 * \brief   Process id value that always refers to the currently
 *          executing process
 */
#define SELF_PID            0

/**
 * \brief   Process id value that match any process. Mostly used for
 *          selecting suitable children to watch in ChildWaitAttach().
 */
#define ANY_PID             -1

/**
 * \brief   Process id of the in-kernel process manager who handles
 *          all the messages used to request OS services
 */
#define PROCMGR_PID         1

/**
 * \brief   Numeric value of the first channel opened by any particular
 *          process.
 *
 * Mainly useful in order for user-space C library to
 * know which channel on the procmgr (#PROCMGR_PID) to send OS
 * service messages to.
 */
#define FIRST_CHANNEL_ID    0

#define FIRST_CONNECTION_ID 0

/**
 * \brief   The fixed-size content of an asynchronous message
 */
struct Pulse
{
    /**
     * The type of message. User-defined pulses can use any value
     * between #PULSE_TYPE_MIN_USER and #PULSE_TYPE_MAX_USER; all
     * others are reserved for system-defined asynchronous message
     * types.
     */
    int8_t      type;

    /**
     * Blank space used to make alignment of subsequent values
     * come out on convenient boundaries. Do not use.
     */
    uint8_t     pad[3];

    /**
     * Payload. For system-defined values of <tt>type</tt>, the
     * documentation will indicate the interpretation of the value.
     * For user-defined values of <tt>type</tt>, the interpretation
     * is at the user's discretion.
     */
    uintptr_t   value;
};

/**
 * Low end of value range users are allowed to use for the <tt>type</tt>
 * field of a pulse message.
 */
#define PULSE_TYPE_MIN_USER 0

/**
 * High end of value range users are allowed to use for the <tt>type</tt>
 * field of a pulse message.
 */
#define PULSE_TYPE_MAX_USER __INT8_MAX__

/**
 * Value of the <tt>type</tt> field of a pulse message received when
 * an interrupt configured with InterruptAttach() is delivered to
 * the program.
 */
#define PULSE_TYPE_INTERRUPT        (PULSE_TYPE_MIN_USER - 1)

/**
 * Value of the <tt>type</tt> field of a pulse message delivered
 * to a program who's called ChildWaitAttach(), to indicate that
 * a child has exited.
 *
 * The <tt>type</tt> field of the pulse contains the process ID of the
 * child that exited.
 */
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

#endif /* __MUOS_MESSAGE_H__ */

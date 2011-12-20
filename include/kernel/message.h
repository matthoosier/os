#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <sys/decls.h>

#include <kernel/list.h>

BEGIN_DECLS

struct Thread;
struct Process;

typedef int Channel_t;
typedef int Connection_t;
typedef int Message_t;

/**
 * Server object on which MsgReceive() is performed
 */
struct Channel
{
    /* List of Message.queue_link nodes: one for each sender blocked on this channel */
    struct list_head send_blocked_head;

    /* List of Message.queue_link nodes; one for each receiver blocked on this channel */
    struct list_head receive_blocked_head;

    /*
     * Utility field for inserting into whatever list is needed
     */
    struct list_head link;
};

/**
 * Client object on which MessageSend() is performed
 */
struct Connection
{
    struct Channel * channel;

    /*
     * Utility field for inserting into whatever list is needed
     */
    struct list_head link;
};

/**
 * Represents the sender, receiver, and parameters of a message..
 */
struct Message
{
    struct Connection * connection;

    /**
     * If non-NULL, a synchronous message that must be replied to. Message
     * and reply buffer are in send_data.sync.
     *
     * If NULL, an asychronous message that accepts to reply. Message payload
     * is in send_data.async.
     */
    struct Thread     * sender;

    struct Thread     * receiver;

    union
    {
        struct
        {
            const void * sender_msgbuf;
            size_t sender_msgbuf_len;

            void * sender_replybuf;
            size_t sender_replybuf_len;
        } sync;
        struct
        {
            uint32_t payload;
        } async;
    } send_data;

    void * receiver_msgbuf;
    size_t receiver_msgbuf_len;

    const void * receiver_replybuf;
    size_t receiver_replybuf_len;

    ssize_t result;

    struct list_head queue_link;
};

extern struct Channel * KChannelAlloc (void);
extern void KChannelFree (struct Channel * channel);

extern struct Connection * KConnect (struct Channel * channel);
extern void KDisconnect (struct Connection * connection);

extern struct Message * KMessageAlloc (void);
extern void KMessageFree (struct Message * message);

/**
 * @return number of bytes written to replybuf, or (-status) if error
 */
extern ssize_t KMessageSend (
        struct Connection * connection,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        );

/**
 * @return (-status) if error, or 0 on success
 */
extern ssize_t KMessageSendAsync (
        struct Connection * connection,
        uint32_t payload
        );

/**
 * @return the number of bytes written to msgbuf, or (-status) if error
 */
extern ssize_t KMessageReceive (
        struct Channel * channel,
        struct Message ** context,
        void * msgbuf,
        size_t msgbuf_len
        );

/**
 * @return the number of bytes transmitted to replybuf, or (-status) if error
 */
extern ssize_t KMessageReply (
        struct Message * context,
        unsigned int status,
        void * replybuf,
        size_t replybuf_len
        );

END_DECLS

#endif /* __MESSAGE_H__ */

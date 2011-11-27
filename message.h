#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <string.h>

#include "decls.h"
#include "list.h"

BEGIN_DECLS

struct Channel;
struct Connection;
struct Message;

/**
 * Server object on which MsgReceive() is performed
 */
struct Channel
{
    /* Nodes are embedded inside 'struct Message' instances */
    struct list_head send_blocked_head;

    /* Nodes are embedded inside 'struct Message' instances */
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
};

/**
 * Represents the sender, receiver, and parameters of a message..
 */
struct Message
{
    struct Connection * connection;
    struct Thread     * sender;
    struct Thread     * receiver;

    const void * sender_msgbuf;
    size_t sender_msgbuf_len;

    void * sender_replybuf;
    size_t sender_replybuf_len;

    void * receiver_msgbuf;
    size_t receiver_msgbuf_len;

    const void * receiver_replybuf;
    size_t receiver_replybuf_len;

    int result;

    struct list_head queue_link;
};

extern struct Channel * ChannelAlloc (void);
extern void ChannelFree (struct Channel * channel);

extern struct Connection * Connect (struct Channel * channel);
extern void Disconnect (struct Connection * connection);

/**
 * @return number of bytes written to replybuf, or negative if error
 */
extern int MessageSend (
        struct Connection * connection,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        );

/**
 * @return the number of bytes written to msgbuf, or negative if error
 */
extern int MessageReceive (
        struct Channel * channel,
        struct Message ** context,
        void * msgbuf,
        size_t msgbuf_len
        );

/**
 * @return the number of bytes transmitted to replybuf, or negative if error
 */
extern int MessageReply (
        struct Message * context,
        void * replybuf,
        size_t replybuf_len
        );

END_DECLS

#endif /* __MESSAGE_H__ */

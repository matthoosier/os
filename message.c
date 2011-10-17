#include <string.h>

#include "assert.h"
#include "list.h"
#include "message.h"
#include "object-cache.h"
#include "once.h"
#include "thread.h"

/**
 * Server object on which MsgReceive() is performed
 */
struct Channel
{
    /* Nodes are embedded inside 'struct Message' instances */
    struct list_head send_blocked_head;

    /* Nodes are embedded inside 'struct Message' instances */
    struct list_head receive_blocked_head;
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

static Once_t inited = ONCE_INIT;

static struct ObjectCache channel_cache;
static struct ObjectCache connection_cache;
static struct ObjectCache message_cache;

static struct Connection * ConnectionAlloc (void);
static void ConnectionFree (struct Connection * connection);

static struct Message * MessageAlloc (void);
static void MessageFree (struct Message * context);

static size_t TransferPayload (
        struct Thread * source,
        const void *    source_buf,
        size_t          source_len,
        struct Thread * dest,
        void *          dest_buf,
        size_t          dest_len
        );

static void init (void * ignored)
{
    ObjectCacheInit(&channel_cache, sizeof(struct Channel));
    ObjectCacheInit(&connection_cache, sizeof(struct Connection));
    ObjectCacheInit(&message_cache, sizeof(struct Message));
}

struct Channel * ChannelAlloc (void)
{
    struct Channel * result;

    Once(&inited, init, NULL);

    result = ObjectCacheAlloc(&channel_cache);

    if (result) {
        INIT_LIST_HEAD(&result->send_blocked_head);
        INIT_LIST_HEAD(&result->receive_blocked_head);
    }

    return result;
}

void ChannelFree (struct Channel * channel)
{
    Once(&inited, init, NULL);

    assert(list_empty(&channel->send_blocked_head));
    assert(list_empty(&channel->receive_blocked_head));

    ObjectCacheFree(&channel_cache, channel);
}

static struct Connection * ConnectionAlloc (void)
{
    Once(&inited, init, NULL);

    return ObjectCacheAlloc(&connection_cache);
}

static void ConnectionFree (struct Connection * connection)
{
    Once(&inited, init, NULL);

    ObjectCacheFree(&connection_cache, connection);
}

struct Connection * Connect (struct Channel * channel)
{
    struct Connection * result = ConnectionAlloc();

    if (result) {
        result->channel = channel;
    }

    return result;
}

void Disconnect (struct Connection * connection)
{
    ConnectionFree(connection);
}

static struct Message * MessageAlloc (void)
{
    struct Message * message;

    Once(&inited, init, NULL);

    message = ObjectCacheAlloc(&message_cache);

    if (message) {
        INIT_LIST_HEAD(&message->queue_link);
    }

    return message;
}

static void MessageFree (struct Message * context)
{
    Once(&inited, init, NULL);
    ObjectCacheFree(&message_cache, context);
}

int MessageSend (
        struct Connection * connection,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        )
{
    struct Message * message;
    int result;

    Once(&inited, init, NULL);

    if (list_empty(&connection->channel->receive_blocked_head)) {
        /* No receiver thread is waiting on the channel at the moment */
        message = MessageAlloc();

        message->connection = connection;
        message->sender = THREAD_CURRENT();
        message->sender_msgbuf = msgbuf;
        message->sender_msgbuf_len = msgbuf_len;
        message->sender_replybuf = replybuf;
        message->sender_replybuf_len = replybuf_len;

        message->receiver = NULL;

        /* Enqueue as blocked on the channel */
        list_add_tail(
                &message->queue_link,
                &connection->channel->send_blocked_head
                );

        message->sender->state = THREAD_STATE_SEND;
        ThreadYieldNoRequeue();
    }
    else {
        /* Receiver thread is ready to go */
        message = list_first_entry(
                &connection->channel->receive_blocked_head,
                struct Message,
                queue_link
                );
        list_del_init(&message->queue_link);

        assert(message->receiver != NULL);

        message->connection = connection;
        message->sender = THREAD_CURRENT();
        message->sender_msgbuf = msgbuf;
        message->sender_msgbuf_len = msgbuf_len;
        message->sender_replybuf = replybuf;
        message->sender_replybuf_len = replybuf_len;

        /*
        Don't explicity add THREAD_CURRENT (the sender) to any scheduling
        queue. The receiver will automatically re-insert us into the
        ready queue when the message reply gets posted.
        */

        message->receiver->state = THREAD_STATE_READY;
        ThreadAddReady(message->receiver);

        message->sender->state = THREAD_STATE_REPLY;
        ThreadYieldNoRequeue();
    }

    /*
    By the time that the receiver wakes us back up, the reply payload
    has already been copied to replybuf. All we have to do is to chain
    the return value along.
    */

    result = message->result;
    MessageFree(message);

    return result;
}

int MessageReceive (
        struct Channel * channel,
        struct Message ** context,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    struct Message * message;
    int result;

    Once(&inited, init, NULL);

    if (list_empty(&channel->send_blocked_head)) {
        /* No sender thread is waiting on the channel at the moment */
        message = MessageAlloc();

        message->receiver = THREAD_CURRENT();
        message->receiver_msgbuf = msgbuf;
        message->receiver_msgbuf_len = msgbuf_len;

        message->sender = NULL;
        message->connection = NULL;

        /* Enqueue as blocked on the channel */
        list_add_tail(
                &message->queue_link,
                &channel->receive_blocked_head
                );

        message->receiver->state = THREAD_STATE_RECEIVE;
        ThreadYieldNoRequeue();
    }
    else {
        /* Sender thread is ready to go */
        message = list_first_entry(
                &channel->send_blocked_head,
                struct Message,
                queue_link
                );
        list_del_init(&message->queue_link);

        assert(message->sender != NULL);

        message->receiver = THREAD_CURRENT();
        message->receiver_msgbuf = msgbuf;
        message->receiver_msgbuf_len = msgbuf_len;
    }

    *context = message;

    result = TransferPayload(
            message->sender,
            message->sender_msgbuf,
            message->sender_msgbuf_len,
            message->receiver,
            message->receiver_msgbuf,
            message->receiver_msgbuf_len
            );

    /* Mark sender as reply-blocked */
    message->sender->state = THREAD_STATE_REPLY;

    return result;
}

int MessageReply (
        struct Message * context,
        void * replybuf,
        size_t replybuf_len
        )
{
    int result;

    Once(&inited, init, NULL);

    assert(context->receiver == THREAD_CURRENT());

    context->receiver_replybuf = replybuf;
    context->receiver_replybuf_len = replybuf_len;

    result = context->result = TransferPayload (
            context->receiver,
            context->receiver_replybuf,
            context->receiver_replybuf_len,
            context->sender,
            context->sender_replybuf,
            context->sender_replybuf_len
            );

    /* Sender will get to run again whenever a scheduling decision happens */
    context->sender->state = THREAD_STATE_READY;
    ThreadAddReady(context->sender);

    /* Sender frees the message after fetching the return value from it */
    return result;
}

static size_t TransferPayload (
        struct Thread * source,
        const void *    source_buf,
        size_t          source_len,
        struct Thread * dest,
        void *          dest_buf,
        size_t          dest_len
        )
{
    size_t len;

    assert(source->user_address_space == dest->user_address_space);

    len = source_len < dest_len ? source_len : dest_len;

    memcpy(dest_buf, source_buf, len);

    return len;
}
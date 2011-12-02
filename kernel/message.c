#include <string.h>

#include <sys/error.h>

#include <kernel/assert.h>
#include <kernel/list.h>
#include <kernel/message.h>
#include <kernel/mmu.h>
#include <kernel/object-cache.h>
#include <kernel/once.h>
#include <kernel/thread.h>

static Once_t inited = ONCE_INIT;

static struct ObjectCache channel_cache;
static struct ObjectCache connection_cache;
static struct ObjectCache message_cache;

static struct Connection * ConnectionAlloc (void);
static void ConnectionFree (struct Connection * connection);

static ssize_t TransferPayload (
        struct Thread * source_thread,
        const void *    source_buf,
        size_t          source_len,
        struct Thread * dest_thread,
        void *          dest_buf,
        size_t          dest_len
        );

static void init (void * ignored)
{
    ObjectCacheInit(&channel_cache, sizeof(struct Channel));
    ObjectCacheInit(&connection_cache, sizeof(struct Connection));
    ObjectCacheInit(&message_cache, sizeof(struct Message));
}

struct Channel * KChannelAlloc (void)
{
    struct Channel * result;

    Once(&inited, init, NULL);

    result = ObjectCacheAlloc(&channel_cache);

    if (result) {
        memset(result, 0, sizeof(*result));
        INIT_LIST_HEAD(&result->send_blocked_head);
        INIT_LIST_HEAD(&result->receive_blocked_head);
        INIT_LIST_HEAD(&result->link);
    }

    return result;
}

void KChannelFree (struct Channel * channel)
{
    Once(&inited, init, NULL);

    assert(list_empty(&channel->send_blocked_head));
    assert(list_empty(&channel->receive_blocked_head));
    assert(list_empty(&channel->link));

    ObjectCacheFree(&channel_cache, channel);
}

static struct Connection * ConnectionAlloc (void)
{
    struct Connection * c;

    Once(&inited, init, NULL);

    c = ObjectCacheAlloc(&connection_cache);

    if (c) {
        memset(c, 0, sizeof(*c));
        INIT_LIST_HEAD(&c->link);
    }

    return c;
}

static void ConnectionFree (struct Connection * connection)
{
    Once(&inited, init, NULL);

    ObjectCacheFree(&connection_cache, connection);
}

struct Connection * KConnect (struct Channel * channel)
{
    struct Connection * result = ConnectionAlloc();

    if (result) {
        result->channel = channel;
    }

    return result;
}

void KDisconnect (struct Connection * connection)
{
    ConnectionFree(connection);
}

struct Message * KMessageAlloc (void)
{
    struct Message * message;

    Once(&inited, init, NULL);

    message = ObjectCacheAlloc(&message_cache);

    if (message) {
        memset(message, 0, sizeof(*message));
        INIT_LIST_HEAD(&message->queue_link);
    }

    return message;
}

void KMessageFree (struct Message * context)
{
    Once(&inited, init, NULL);
    ObjectCacheFree(&message_cache, context);
}

ssize_t KMessageSend (
        struct Connection * connection,
        const void * msgbuf,
        size_t msgbuf_len,
        void * replybuf,
        size_t replybuf_len
        )
{
    struct Message * message;
    ssize_t result;

    Once(&inited, init, NULL);

    if (list_empty(&connection->channel->receive_blocked_head)) {
        /* No receiver thread is waiting on the channel at the moment */
        message = KMessageAlloc();

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
    KMessageFree(message);

    return result;
} /* KMessageSend() */

ssize_t KMessageReceive (
        struct Channel * channel,
        struct Message ** context,
        void * msgbuf,
        size_t msgbuf_len
        )
{
    struct Message * message;
    ssize_t num_copied;

    Once(&inited, init, NULL);

    if (list_empty(&channel->send_blocked_head)) {
        /* No sender thread is waiting on the channel at the moment */
        message = KMessageAlloc();

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

    num_copied = TransferPayload(
            message->sender,
            message->sender_msgbuf,
            message->sender_msgbuf_len,
            message->receiver,
            message->receiver_msgbuf,
            message->receiver_msgbuf_len
            );

    /* Mark sender as reply-blocked */
    message->sender->state = THREAD_STATE_REPLY;

    return num_copied;
} /* KMessageReceive() */

ssize_t KMessageReply (
        struct Message * context,
        unsigned int status,
        void * replybuf,
        size_t replybuf_len
        )
{
    size_t result;

    Once(&inited, init, NULL);

    assert(context->receiver == THREAD_CURRENT());

    context->receiver_replybuf = replybuf;
    context->receiver_replybuf_len = replybuf_len;

    /*
    Releasing this thread to run again might make the receiver's reply
    buffer invalid, so do the transfer before returning control. The sender
    will wake up with the reply message already copied into his address
    space's replybuf.
    */
    if (status == ERROR_OK) {
        result = TransferPayload (
                context->receiver,
                context->receiver_replybuf,
                context->receiver_replybuf_len,
                context->sender,
                context->sender_replybuf,
                context->sender_replybuf_len
                );
        context->result = result;
    } else {
        context->result = -status;
    }

    result = status == ERROR_OK ? context->result : ERROR_OK;

    /* Sender will get to run again whenever a scheduling decision happens */
    context->sender->state = THREAD_STATE_READY;
    ThreadAddReady(context->sender);

    /* Sender frees the message after fetching the return value from it */
    return result;
} /* KMessageReply() */

static ssize_t TransferPayload (
        struct Thread * source_thread,
        const void *    source_buf,
        size_t          source_len,
        struct Thread * dest_thread,
        void *          dest_buf,
        size_t          dest_len
        )
{
    struct TranslationTable *src_tt;
    struct TranslationTable *dst_tt;

    if ((VmAddr_t)source_buf >= KERNEL_MODE_OFFSET) {
        src_tt = MmuGetKernelTranslationTable();
    } else {
        assert(source_thread->process != NULL);
        src_tt = source_thread->process->pagetable;
    }

    if ((VmAddr_t)dest_buf >= KERNEL_MODE_OFFSET) {
        dst_tt = MmuGetKernelTranslationTable();
    } else {
        assert(dest_thread->process != NULL);
        dst_tt = dest_thread->process->pagetable;
    }

    return CopyWithAddressSpaces(
            src_tt,
            source_buf,
            source_len,
            dst_tt,
            dest_buf,
            dest_len
            );
}

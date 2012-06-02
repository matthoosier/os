#include <string.h>

#include <sys/error.h>
#include <sys/spinlock.h>

#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>
#include <kernel/thread.hpp>

static Once_t inited = ONCE_INIT;

static struct ObjectCache   channel_cache;
static Spinlock_t           channel_cache_lock = SPINLOCK_INIT;

static struct ObjectCache   connection_cache;
static Spinlock_t           connection_cache_lock = SPINLOCK_INIT;

static struct ObjectCache   message_cache;
static Spinlock_t           message_cache_lock = SPINLOCK_INIT;

static struct Connection * ConnectionAlloc (void);
static void ConnectionFree (struct Connection * connection);

static ssize_t TransferPayload (
        Thread *        source_thread,
        const void *    source_buf,
        size_t          source_len,
        Thread *        dest_thread,
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

    SpinlockLock(&channel_cache_lock);
    result = (struct Channel*)ObjectCacheAlloc(&channel_cache);
    SpinlockUnlock(&channel_cache_lock);

    if (result) {
        memset(result, 0, sizeof(*result));
        result->send_blocked_head.DynamicInit();
        result->receive_blocked_head.DynamicInit();
        result->link.DynamicInit();
    }

    return result;
}

void KChannelFree (struct Channel * channel)
{
    Once(&inited, init, NULL);

    assert(channel->send_blocked_head.Empty());
    assert(channel->receive_blocked_head.Empty());
    assert(channel->link.Unlinked());

    SpinlockLock(&channel_cache_lock);
    ObjectCacheFree(&channel_cache, channel);
    SpinlockUnlock(&channel_cache_lock);
}

static struct Connection * ConnectionAlloc (void)
{
    struct Connection * c;

    Once(&inited, init, NULL);

    SpinlockLock(&connection_cache_lock);
    c = (struct Connection *)ObjectCacheAlloc(&connection_cache);
    SpinlockUnlock(&connection_cache_lock);

    if (c) {
        memset(c, 0, sizeof(*c));
        c->link.DynamicInit();
    }

    return c;
}

static void ConnectionFree (struct Connection * connection)
{
    Once(&inited, init, NULL);

    SpinlockLock(&connection_cache_lock);
    ObjectCacheFree(&connection_cache, connection);
    SpinlockUnlock(&connection_cache_lock);
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

    SpinlockLock(&message_cache_lock);
    message = (struct Message *)ObjectCacheAlloc(&message_cache);
    SpinlockUnlock(&message_cache_lock);

    if (message) {
        memset(message, 0, sizeof(*message));
        message->queue_link.DynamicInit();
    }

    return message;
}

void KMessageFree (struct Message * context)
{
    Once(&inited, init, NULL);

    SpinlockLock(&message_cache_lock);
    ObjectCacheFree(&message_cache, context);
    SpinlockUnlock(&message_cache_lock);
}

ssize_t KMessageSendAsync (
        struct Connection * connection,
        uintptr_t payload
        )
{
    struct Message * message;

    Once(&inited, init, NULL);

    if (connection->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        message = KMessageAlloc();

        if (message == NULL) {
            return -ERROR_NO_MEM;
        }

        message->connection = connection;
        message->sender = NULL;
        message->send_data.async.payload = payload;

        message->receiver = NULL;

        /* Enqueue message for delivery whenever receiver asks for it */
        connection->channel->send_blocked_head.Append(message);
    }
    else {
        /* Receiver thread is ready to go */
        message = connection->channel->receive_blocked_head.PopFirst();

        assert(message->receiver != NULL);

        message->connection = connection;
        message->sender = NULL;
        message->send_data.async.payload = payload;

        /* Allow receiver to wake up */
        Thread::BeginTransactionDuringIrq();
        Thread::MakeReady(message->receiver);
        Thread::SetNeedResched();
        Thread::EndTransaction();
    }

    return ERROR_OK;
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

    if (connection->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        message = KMessageAlloc();

        message->connection = connection;
        message->sender = THREAD_CURRENT();
        message->send_data.sync.sender_msgbuf = msgbuf;
        message->send_data.sync.sender_msgbuf_len = msgbuf_len;
        message->send_data.sync.sender_replybuf = replybuf;
        message->send_data.sync.sender_replybuf_len = replybuf_len;

        message->receiver = NULL;

        /* Enqueue as blocked on the channel */
        connection->channel->send_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_SEND);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }
    else {
        /* Receiver thread is ready to go */
        message = connection->channel->receive_blocked_head.PopFirst();

        assert(message->receiver != NULL);

        message->connection = connection;
        message->sender = THREAD_CURRENT();
        message->send_data.sync.sender_msgbuf = msgbuf;
        message->send_data.sync.sender_msgbuf_len = msgbuf_len;
        message->send_data.sync.sender_replybuf = replybuf;
        message->send_data.sync.sender_replybuf_len = replybuf_len;

        /* Temporarily gift our priority to the message-handling thread */
        message->receiver->SetEffectivePriority(THREAD_CURRENT()->effective_priority);

        /* Now allow handler to run */
        Thread::BeginTransaction();
        Thread::MakeReady(message->receiver);
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_REPLY);
        Thread::RunNextThread();
        Thread::EndTransaction();
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

    if (channel->send_blocked_head.Empty()) {
        /* No message is waiting in the channel at the moment */
        message = KMessageAlloc();

        message->receiver = THREAD_CURRENT();
        message->receiver_msgbuf = msgbuf;
        message->receiver_msgbuf_len = msgbuf_len;

        message->sender = NULL;
        message->connection = NULL;

        /* Enqueue as blocked on the channel */
        channel->receive_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_RECEIVE);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }
    else {
        /* Some message is waiting in the channel already */
        message = channel->send_blocked_head.PopFirst();

        message->receiver = THREAD_CURRENT();
        message->receiver_msgbuf = msgbuf;
        message->receiver_msgbuf_len = msgbuf_len;
    }

    if (message->sender != NULL) {
        /* Synchronous message */
        *context = message;

        num_copied = TransferPayload(
                message->sender,
                message->send_data.sync.sender_msgbuf,
                message->send_data.sync.sender_msgbuf_len,
                message->receiver,
                message->receiver_msgbuf,
                message->receiver_msgbuf_len
                );
    }
    else {
        /* Asynchronous message */
        *context = NULL;

        /*
        Slight hack. Just need to nominate some thread (the current one
        will do) whose pagetable can be used for the upcoming TransferPayload()
        call.
        */
        Thread * sender_pagetable_thread = THREAD_CURRENT();

        num_copied = TransferPayload(
                sender_pagetable_thread,
                &message->send_data.async.payload,
                sizeof(message->send_data.async.payload),
                message->receiver,
                message->receiver_msgbuf,
                message->receiver_msgbuf_len
                );

        /* There will be no reply, so free the Message struct now */
        KMessageFree(message);
    }

    return num_copied;
} /* KMessageReceive() */

ssize_t KMessageReply (
        struct Message * context,
        unsigned int status,
        const void * replybuf,
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
                context->send_data.sync.sender_replybuf,
                context->send_data.sync.sender_replybuf_len
                );
        context->result = result;
    } else {
        context->result = -status;
    }

    result = status == ERROR_OK ? context->result : ERROR_OK;

    Thread::BeginTransaction();

    /* Sender will get to run again whenever a scheduling decision happens */
    Thread::MakeReady(context->sender);

    /* Abandon any temporary priority boost we had now that the sender is unblocked */
    THREAD_CURRENT()->SetEffectivePriority(THREAD_CURRENT()->assigned_priority);

    /* Current thread (replier) remains runnable. */
    Thread::MakeReady(THREAD_CURRENT());

    Thread::RunNextThread();

    Thread::EndTransaction();

    /* Sender frees the message after fetching the return value from it */
    return result;
} /* KMessageReply() */

static ssize_t TransferPayload (
        Thread *        source_thread,
        const void *    source_buf,
        size_t          source_len,
        Thread *        dest_thread,
        void *          dest_buf,
        size_t          dest_len
        )
{
    TranslationTable *src_tt;
    TranslationTable *dst_tt;

    if ((VmAddr_t)source_buf >= KERNEL_MODE_OFFSET) {
        src_tt = TranslationTable::GetKernel();
    } else {
        assert(source_thread->process != NULL);
        src_tt = source_thread->process->pagetable;
    }

    if ((VmAddr_t)dest_buf >= KERNEL_MODE_OFFSET) {
        dst_tt = TranslationTable::GetKernel();
    } else {
        assert(dest_thread->process != NULL);
        dst_tt = dest_thread->process->pagetable;
    }

    return TranslationTable::CopyWithAddressSpaces(
            src_tt,
            source_buf,
            source_len,
            dst_tt,
            dest_buf,
            dest_len
            );
}

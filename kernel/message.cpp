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
    ObjectCacheInit(&channel_cache, sizeof(Channel));
    ObjectCacheInit(&connection_cache, sizeof(Connection));
    ObjectCacheInit(&message_cache, sizeof(Message));
}

void * Channel::operator new (size_t size) throw (std::bad_alloc)
{
    void * ret;

    Once(&inited, init, NULL);

    SpinlockLock(&channel_cache_lock);
    ret = ObjectCacheAlloc(&channel_cache);
    SpinlockUnlock(&channel_cache_lock);

    if (!ret) {
        throw std::bad_alloc();
    }

    return ret;
}

void Channel::operator delete (void * mem) throw ()
{
    Once(&inited, init, NULL);

    SpinlockLock(&channel_cache_lock);
    ObjectCacheFree(&channel_cache, mem);
    SpinlockUnlock(&channel_cache_lock);
}

Channel::Channel ()
{
}

Channel::~Channel ()
{
    assert(this->send_blocked_head.Empty());
    assert(this->receive_blocked_head.Empty());
    assert(this->link.Unlinked());
}

void * Connection::operator new (size_t size) throw (std::bad_alloc)
{
    void * ret;

    Once(&inited, init, NULL);

    SpinlockLock(&connection_cache_lock);
    ret = ObjectCacheAlloc(&connection_cache);
    SpinlockUnlock(&connection_cache_lock);

    if (!ret) {
        throw std::bad_alloc();
    }

    return ret;
}

void Connection::operator delete (void * mem) throw ()
{
    Once(&inited, init, NULL);

    SpinlockLock(&connection_cache_lock);
    ObjectCacheFree(&connection_cache, mem);
    SpinlockUnlock(&connection_cache_lock);
}

Connection::Connection (Channel * server)
    : channel(server)
{
}

Connection::~Connection ()
{
}

void * Message::operator new (size_t size) throw (std::bad_alloc)
{
    void * ret;

    Once(&inited, init, NULL);

    SpinlockLock(&message_cache_lock);
    ret = ObjectCacheAlloc(&message_cache);
    SpinlockUnlock(&message_cache_lock);

    if (!ret) {
        throw std::bad_alloc();
    }

    return ret;
}

void Message::operator delete (void * mem) throw ()
{
    Once(&inited, init, NULL);

    SpinlockLock(&message_cache_lock);
    ObjectCacheFree(&message_cache, mem);
    SpinlockUnlock(&message_cache_lock);
}

Message::Message ()
    : connection (NULL)
    , sender (NULL)
    , receiver (NULL)
    , result (0)
{
    memset(&this->send_data, 0, sizeof(this->send_data));
    memset(&this->receive_data, 0, sizeof(this->receive_data));
}

Message::~Message ()
{
}

ssize_t Connection::SendMessageAsync (uintptr_t payload)
{
    Message * message;

    Once(&inited, init, NULL);

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->connection = this;
        message->sender = NULL;
        message->send_data.async.payload = payload;

        message->receiver = NULL;

        /* Enqueue message for delivery whenever receiver asks for it */
        this->channel->send_blocked_head.Append(message);
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->receiver != NULL);

        message->connection = this;
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

ssize_t Connection::SendMessage (
        const void    * msgbuf,
        size_t          msgbuf_len,
        void          * replybuf,
        size_t          replybuf_len
        )
{
    Message * message;
    ssize_t result;

    Once(&inited, init, NULL);

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->connection = this;
        message->sender = THREAD_CURRENT();
        message->send_data.sync.sender_msgbuf = msgbuf;
        message->send_data.sync.sender_msgbuf_len = msgbuf_len;
        message->send_data.sync.sender_replybuf = replybuf;
        message->send_data.sync.sender_replybuf_len = replybuf_len;

        message->receiver = NULL;

        /* Enqueue as blocked on the channel */
        this->channel->send_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_SEND);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->receiver != NULL);

        message->connection = this;
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
    delete message;

    return result;
} /* Connection::SendMessage() */

ssize_t Channel::ReceiveMessage (
        Message  ** context,
        void      * msgbuf,
        size_t      msgbuf_len
        )
{
    Message * message;
    ssize_t num_copied;

    Once(&inited, init, NULL);

    if (this->send_blocked_head.Empty()) {
        /* No message is waiting in the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->receiver = THREAD_CURRENT();
        message->receive_data.receiver_msgbuf = msgbuf;
        message->receive_data.receiver_msgbuf_len = msgbuf_len;

        message->sender = NULL;
        message->connection = NULL;

        /* Enqueue as blocked on the channel */
        this->receive_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_RECEIVE);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }
    else {
        /* Some message is waiting in the channel already */
        message = this->send_blocked_head.PopFirst();

        message->receiver = THREAD_CURRENT();
        message->receive_data.receiver_msgbuf = msgbuf;
        message->receive_data.receiver_msgbuf_len = msgbuf_len;
    }

    if (message->sender != NULL) {
        /* Synchronous message */
        *context = message;

        num_copied = TransferPayload(
                message->sender,
                message->send_data.sync.sender_msgbuf,
                message->send_data.sync.sender_msgbuf_len,
                message->receiver,
                message->receive_data.receiver_msgbuf,
                message->receive_data.receiver_msgbuf_len
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
                message->receive_data.receiver_msgbuf,
                message->receive_data.receiver_msgbuf_len
                );

        /* There will be no reply, so free the Message struct now */
        delete message;
    }

    return num_copied;
} /* Channel::ReceiveMessage() */

ssize_t Message::Reply (
        unsigned int status,
        const void * replybuf,
        size_t replybuf_len
        )
{
    size_t result;

    Once(&inited, init, NULL);

    assert(this->receiver == THREAD_CURRENT());

    this->receive_data.receiver_replybuf = replybuf;
    this->receive_data.receiver_replybuf_len = replybuf_len;

    /*
    Releasing this thread to run again might make the receiver's reply
    buffer invalid, so do the transfer before returning control. The sender
    will wake up with the reply message already copied into his address
    space's replybuf.
    */
    if (status == ERROR_OK) {
        result = TransferPayload (
                this->receiver,
                this->receive_data.receiver_replybuf,
                this->receive_data.receiver_replybuf_len,
                this->sender,
                this->send_data.sync.sender_replybuf,
                this->send_data.sync.sender_replybuf_len
                );
        this->result = result;
    } else {
        this->result = -status;
    }

    result = status == ERROR_OK ? this->result : ERROR_OK;

    Thread::BeginTransaction();

    /* Sender will get to run again whenever a scheduling decision happens */
    Thread::MakeReady(this->sender);

    /* Abandon any temporary priority boost we had now that the sender is unblocked */
    THREAD_CURRENT()->SetEffectivePriority(THREAD_CURRENT()->assigned_priority);

    /* Current thread (replier) remains runnable. */
    Thread::MakeReady(THREAD_CURRENT());

    Thread::RunNextThread();

    Thread::EndTransaction();

    /* Sender frees the message after fetching the return value from it */
    return result;
} /* Message::Reply() */

Thread * Message::GetSender ()
{
    return this->sender;
}

Thread * Message::GetReceiver ()
{
    return this->receiver;
}

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
        src_tt = source_thread->process->GetTranslationTable();
    }

    if ((VmAddr_t)dest_buf >= KERNEL_MODE_OFFSET) {
        dst_tt = TranslationTable::GetKernel();
    } else {
        assert(dest_thread->process != NULL);
        dst_tt = dest_thread->process->GetTranslationTable();
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

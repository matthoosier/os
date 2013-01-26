#include <string.h>

#include <muos/error.h>
#include <muos/message.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/minmax.hpp>
#include <kernel/mmu.hpp>
#include <kernel/process.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/thread.hpp>

/**
 * Synchronizes modification to channel/connection pairs during
 * message passing.
 */
static Spinlock_t gLock = SPINLOCK_INIT;

SyncSlabAllocator<Channel> Channel::sSlab;
SyncSlabAllocator<Connection> Connection::sSlab;
SyncSlabAllocator<Message> Message::sSlab;

static bool FindChunkOffset (IoVector const & iov,
                             size_t skip_bytes_count,
                             size_t & found_chunk_idx,
                             size_t & found_chunk_skip_bytes);

static ssize_t TransferPayloadV (
        Thread *         source_thread,
        IoVector const & source_iov,
        size_t           source_skip_bytes,
        Thread *         dest_thread,
        IoVector const & dest_iov,
        size_t           dest_skip_bytes
        );

static ssize_t TransferPayload (
        Thread *        source_thread,
        const void *    source_buf,
        size_t          source_len,
        Thread *        dest_thread,
        void *          dest_buf,
        size_t          dest_len
        );

Channel::Channel ()
    : mDisposed(false)
{
}

Channel::~Channel ()
{
    SpinlockLock(&gLock);

    assert(this->mBlockedConnections.Empty());
    assert(this->mNotBlockedConnections.Empty());
    assert(this->mReceiveBlockedMessages.Empty());

    SpinlockUnlock(&gLock);
}

void Channel::Dispose ()
{
    if (mDisposed) {
        return;
    }

    SpinlockLock(&gLock);

    mDisposed = true;

    // Immediately drop filesystem name of the channel
    name_record.Reset();

    // Dispose all our connections that are send-blocked
    while (!mBlockedConnections.Empty()) {
        RefPtr<Connection> connection = mBlockedConnections.PopFirst();

        SpinlockUnlock(&gLock);
        connection->Dispose();
        SpinlockLock(&gLock);

        assert(connection->link.Unlinked());
    }

    // Dispose all our connections that are not send-blocked
    while (!mNotBlockedConnections.Empty()) {
        RefPtr<Connection> connection = mNotBlockedConnections.PopFirst();

        SpinlockUnlock(&gLock);
        connection->Dispose();
        SpinlockLock(&gLock);

        assert(connection->link.Unlinked());
    }

    // Unqueue any receive-blocked messages
    while (!mReceiveBlockedMessages.Empty()) {
        RefPtr<Message> message = mReceiveBlockedMessages.PopFirst();

        if (message->mSender && message->mSender->GetState() != Thread::STATE_FINISHED) {
            assert(message->mReceiver);

            message->mSendData.sync.msgv = NULL;
            message->mSendData.sync.msgv_count = 0;
            message->mSendData.sync.replyv = NULL;
            message->mSendData.sync.replyv_count = 0;

            SpinlockUnlock(&gLock);
            message->mReceiverSemaphore.Up();
            SpinlockLock(&gLock);
        }
    }

    // By disposing all the connected channels above, we've
    // ensured that there are no remaining receive-blocked
    // messages.
    assert(mReceiveBlockedMessages.Empty());

    SpinlockUnlock(&gLock);
}

Connection::Connection (RefPtr<Channel> server)
    : channel(server)
    , mDisposed(false)
{
    SpinlockLock(&gLock);

    server->mNotBlockedConnections.Append(SelfRef());

    SpinlockUnlock(&gLock);
}

Connection::~Connection ()
{
}

void Connection::Dispose ()
{
    if (mDisposed) {
        return;
    }

    SpinlockLock(&gLock);

    assert(channel);

    if (this->mSendBlockedMessages.Empty()) {
        // Channel flushes out all send-blocked messages upon its
        // own disposal, so there's no need to check whether the
        // channel is disposed.
        this->channel->mNotBlockedConnections.Remove(SelfRef());
    }
    else {
        // Channel disconnects all non-send-blocked connections upon
        // its own disposal, so there's no need to check whether
        // the channel is disposed.
        this->channel->mBlockedConnections.Remove(SelfRef());

        while (!this->mSendBlockedMessages.Empty()) {
            RefPtr<Message> message = this->mSendBlockedMessages.PopFirst();

            if (message->mSender && message->mSender->GetState() != Thread::STATE_FINISHED) {
                // Unblock sender; it'll deallocate the message when it returns
                // out of SendMessage()
                message->Reply(ERROR_NO_SYS, &IoBuffer::GetEmpty(), 1);
            } else {
                // Async message; no reply needed. Just directly deallocate.
                message.Reset();
            }
        }
    }

    channel.Reset();

    assert(this->mSendBlockedMessages.Empty());

    SpinlockUnlock(&gLock);

    mDisposed = true;
}

Message::Message ()
    : mSender ()
    , mSenderSemaphore (0)
    , mReceiver ()
    , mReceiverSemaphore (0)
    , mResult ()
    , mDisposed (false)
{
    memset(&mSendData, 0, sizeof(mSendData));
    memset(&mReceiveData, 0, sizeof(mReceiveData));
}

Message::~Message ()
{
}

void Message::Dispose ()
{
    if (mDisposed) {
        return;
    }

    mConnection.Reset();
    mDisposed = true;
}

ssize_t Connection::SendMessageAsync (int8_t type, uintptr_t value)
{
    return SendMessageAsyncInternal(type, value, false);
}

ssize_t Connection::SendMessageAsyncDuringException (int8_t type,
                                                     uintptr_t value)
{
    return SendMessageAsyncInternal(type, value, true);
}

ssize_t Connection::SendMessageAsyncInternal (int8_t type,
                                              uintptr_t value,
                                              bool isDuringException)
{
    RefPtr<Message> message;

    SpinlockLock(&gLock);

    if (this->mDisposed || this->channel->mDisposed) {
        SpinlockUnlock(&gLock);
        return -ERROR_INVALID;
    }

    if (this->channel->mReceiveBlockedMessages.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message.Reset(new Message());
        } catch (std::bad_alloc) {
            SpinlockUnlock(&gLock);
            return -ERROR_NO_MEM;
        }

        message->mConnection = SelfRef();
        message->mSender = NULL;
        message->mType = Message::TYPE_ASYNC;
        message->mSendData.async.type = type;
        message->mSendData.async.value = value;

        message->mReceiver = NULL;

        /* Enqueue message for delivery whenever receiver asks for it */
        if (this->mSendBlockedMessages.Empty()) {
            RefPtr<Connection> self = SelfRef();
            this->channel->mNotBlockedConnections.Remove(self);
            this->channel->mBlockedConnections.Append(self);
        }
        this->mSendBlockedMessages.Append(message);
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->mReceiveBlockedMessages.PopFirst();

        assert(message->mReceiver);

        message->mConnection = SelfRef();
        message->mSender = NULL;
        message->mType = Message::TYPE_ASYNC;
        message->mSendData.async.type = type;
        message->mSendData.async.value = value;
    }

    SpinlockUnlock(&gLock);

    /* Allow receiver to wake up */
    if (isDuringException) {
        message->mReceiverSemaphore.UpDuringException();
    } else {
        message->mReceiverSemaphore.Up();
    }

    return ERROR_OK;
}

ssize_t Connection::SendMessage (
        IoBuffer const msgv[],
        size_t         msgv_count,
        IoBuffer const replyv[],
        size_t         replyv_count
        )
{
    RefPtr<Message> message;
    ssize_t result;

    SpinlockLock(&gLock);

    if (this->mDisposed || this->channel->mDisposed) {
        SpinlockUnlock(&gLock);
        return -ERROR_INVALID;
    }

    if (this->channel->mReceiveBlockedMessages.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message.Reset(new Message());
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mConnection = SelfRef();
        message->mSender = THREAD_CURRENT();
        message->mType = Message::TYPE_SYNC;
        message->mSendData.sync.msgv = msgv;
        message->mSendData.sync.msgv_count = msgv_count;
        message->mSendData.sync.replyv = replyv;
        message->mSendData.sync.replyv_count = replyv_count;

        message->mReceiver = NULL;

        /* Enqueue as blocked on the channel */
        if (this->mSendBlockedMessages.Empty()) {
            RefPtr<Connection> self = SelfRef();
            this->channel->mNotBlockedConnections.Remove(self);
            this->channel->mBlockedConnections.Append(self);
        }
        this->mSendBlockedMessages.Append(message);
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->mReceiveBlockedMessages.PopFirst();

        assert(message->mReceiver);

        message->mConnection = SelfRef();
        message->mSender = THREAD_CURRENT();
        message->mType = Message::TYPE_SYNC;
        message->mSendData.sync.msgv = msgv;
        message->mSendData.sync.msgv_count = msgv_count;
        message->mSendData.sync.replyv = replyv;
        message->mSendData.sync.replyv_count = replyv_count;

        /* Temporarily gift our priority to the message-handling thread */
        message->mReceiver->SetEffectivePriority(THREAD_CURRENT()->effective_priority);
    }

    SpinlockUnlock(&gLock);

    message->mReceiverSemaphore.Up();
    message->mSenderSemaphore.Down(Thread::STATE_REPLY);

    /*
    By the time that the receiver wakes us back up, the reply payload
    has already been copied to replybuf. All we have to do is to chain
    the return value along.
    */

    result = message->mResult;
    message.Reset();

    return result;
} /* Connection::SendMessage() */

void Channel::SetNameRecord (NameRecord * name_record)
{
    this->name_record = name_record;
}

ssize_t Channel::ReceiveMessage (
        RefPtr<Message> & context,
        IoBuffer const msgv[],
        size_t msgv_count
        )
{
    RefPtr<Message> message;
    ssize_t         num_copied;

    SpinlockLock(&gLock);

    if (this->mBlockedConnections.Empty()) {
        /* No message is waiting in the channel at the moment */
        try {
            message.Reset(new Message());
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mReceiver = THREAD_CURRENT();
        message->mReceiveData.msgv = msgv;
        message->mReceiveData.msgv_count = msgv_count;

        message->mSender = NULL;

        /* Enqueue as blocked on the channel */
        this->mReceiveBlockedMessages.Append(message);
    }
    else {
        /* Some message is waiting in the channel already */
        RefPtr<Connection> client = this->mBlockedConnections.First();
        assert(!client->mSendBlockedMessages.Empty());
        message = client->mSendBlockedMessages.PopFirst();

        /* If there are no more messages, mark conn as not-blocked */
        if (client->mSendBlockedMessages.Empty()) {
            this->mBlockedConnections.Remove(client);
            this->mNotBlockedConnections.Append(client);
        }

        message->mReceiver = THREAD_CURRENT();
        message->mReceiveData.msgv = msgv;
        message->mReceiveData.msgv_count = msgv_count;
    }

    SpinlockUnlock(&gLock);

    message->mReceiverSemaphore.Down(Thread::STATE_RECEIVE);

    if (message->mType == Message::TYPE_SYNC) {

        // Give receiver a reference to the message
        context = message;

        num_copied = TransferPayloadV(
                *message->mSender,
                IoVector(message->mSendData.sync.msgv,
                         message->mSendData.sync.msgv_count),
                0,
                *message->mReceiver,
                IoVector(message->mReceiveData.msgv,
                         message->mReceiveData.msgv_count),
                0
                );
    }
    else if (message->mType == Message::TYPE_ASYNC) {

        IoBuffer payload_chunk(&message->mSendData.async, sizeof(message->mSendData.async));

        /*
        Slight hack. Just need to nominate some thread (the current one
        will do) whose pagetable can be used for the upcoming TransferPayloadV()
        call.
        */
        Thread * sender_pagetable_thread = THREAD_CURRENT();

        num_copied = TransferPayloadV(
                sender_pagetable_thread,
                IoVector(&payload_chunk, 1),
                0,
                *message->mReceiver,
                IoVector(message->mReceiveData.msgv,
                         message->mReceiveData.msgv_count),
                0
                );

        // There will be no reply, so free the Message struct now
        message.Reset();

        // ... and make sure that the receiver doesn't accidentally get
        // a reference to it
        context.Reset();
    }
    else {
        assert(false);
    }

    return num_copied;
} /* Channel::ReceiveMessage() */

size_t Message::GetLength ()
{
    if (mType == TYPE_SYNC) {
        return IoVector(mSendData.sync.msgv,
                        mSendData.sync.msgv_count).Length();
    } else if (mType == TYPE_ASYNC) {
        return sizeof(mSendData.async);
    } else {
        assert(false);
        return 0;
    }
}

ssize_t Message::Read (size_t src_offset,
                       IoBuffer const destv[],
                       size_t destv_count)
{
    if (mType == TYPE_SYNC) {

        IoVector srcVector(mSendData.sync.msgv,
                           mSendData.sync.msgv_count);

        IoVector dstVector(destv, destv_count);

        if (src_offset >= srcVector.Length() || dstVector.Length() < 0) {
            return 0;
        }

        return TransferPayloadV(*mSender, srcVector, src_offset,
                                THREAD_CURRENT(), dstVector, 0);
    } else if (mType == TYPE_ASYNC) {
        assert(mType == Message::TYPE_ASYNC);

        if (src_offset > sizeof(mSendData.async)) {
            return 0;
        }

        IoBuffer chunk((uint8_t *)&mSendData.async + src_offset,
                    sizeof(mSendData.async) - src_offset);

        return TransferPayloadV(
                THREAD_CURRENT(),   // Dummy sender; won't matter
                IoVector(&chunk, 1),
                0,
                THREAD_CURRENT(),
                IoVector(destv, destv_count),
                0);
    } else {
        assert(false);
        return 0;
    }
}

ssize_t Message::Reply (
        unsigned int status,
        IoBuffer const replyv[],
        size_t replyv_count
        )
{
    size_t result;

    assert(*mReceiver == THREAD_CURRENT());

    if (mDisposed) {
        result = -ERROR_INVALID;
    }
    else {
        mReceiveData.replyv = replyv;
        mReceiveData.replyv_count = replyv_count;

        /*
        Releasing this thread to run again might make the receiver's reply
        buffer invalid, so do the transfer before returning control. The sender
        will wake up with the reply message already copied into his address
        space's replybuf.
        */
        if (status == ERROR_OK) {
            result = TransferPayloadV (
                    *mReceiver,
                    IoVector(mReceiveData.replyv,
                             mReceiveData.replyv_count),
                    0,
                    *mSender,
                    IoVector(mSendData.sync.replyv,
                             mSendData.sync.replyv_count),
                    0
                    );
            mResult = result;
        } else {
            mResult = -status;
        }

        result = status == ERROR_OK ? mResult : ERROR_OK;

        /* Sender will get to run again whenever a scheduling decision happens */
        mSenderSemaphore.Up();
    }

    /* Abandon any temporary priority boost we had now that the sender is unblocked */
    THREAD_CURRENT()->SetEffectivePriority(THREAD_CURRENT()->assigned_priority);

    /* Sender frees the message after fetching the return value from it */
    return result;
} /* Message::Reply() */

Thread * Message::GetSender ()
{
    return *mSender;
}

Thread * Message::GetReceiver ()
{
    return *mReceiver;
}

__attribute__((used))
static bool FindChunkOffset (IoVector const & iov,
                             size_t skip_bytes_count,
                             size_t & found_chunk_idx,
                             size_t & found_chunk_skip_bytes)
{
    IoBuffer const * bufs = iov.GetBuffers();
    size_t skipped_bytes = 0;

    for (size_t idx = 0; idx < iov.GetCount(); ++idx)
    {
        if (skip_bytes_count < skipped_bytes + bufs[idx].mLength) {
            found_chunk_idx = idx;
            found_chunk_skip_bytes = skip_bytes_count - skipped_bytes;
            return true;
        }

        skipped_bytes += bufs[idx].mLength;
    }

    return false;
}

static ssize_t TransferPayloadV (
        Thread *         source_thread,
        IoVector const & source_iov,
        size_t           source_skip_bytes,
        Thread *         dest_thread,
        IoVector const & dest_iov,
        size_t           dest_skip_bytes
        )
{
    size_t remaining = MIN(source_iov.Length() - source_skip_bytes,
                           dest_iov.Length() - dest_skip_bytes);
    size_t transferred = 0;

    while (remaining > 0) {
        size_t src_chunk_idx;
        size_t src_chunk_skip;

        size_t dst_chunk_idx;
        size_t dst_chunk_skip;

        bool found_src = FindChunkOffset(source_iov,
                                         transferred + source_skip_bytes,
                                         src_chunk_idx,
                                         src_chunk_skip);

        bool found_dst = FindChunkOffset(dest_iov,
                                         transferred + dest_skip_bytes,
                                         dst_chunk_idx,
                                         dst_chunk_skip);

        if (found_src && found_dst) {

            IoBuffer const * src_chunk = &source_iov.GetBuffers()[src_chunk_idx];
            IoBuffer const * dst_chunk = &dest_iov.GetBuffers()[dst_chunk_idx];

            size_t n = MIN(MIN(remaining,
                               src_chunk->mLength - src_chunk_skip),
                           MIN(remaining,
                               dst_chunk->mLength - dst_chunk_skip));

            size_t actual = TransferPayload(source_thread,
                                            src_chunk->mData + src_chunk_skip,
                                            n,
                                            dest_thread,
                                            dst_chunk->mData + dst_chunk_skip,
                                            n);

            remaining -= actual;
            transferred += actual;

            if (actual < n) {
                break;
            }
        }
        else {
            break;
        }
    }

    return transferred;
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

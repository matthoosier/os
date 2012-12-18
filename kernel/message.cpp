#include <string.h>

#include <sys/error.h>

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
{
}

Channel::~Channel ()
{
    SpinlockLock(&gLock);

    assert(this->waiting_clients.Empty());
    assert(this->unwaiting_clients.Empty());
    assert(this->receive_blocked_head.Empty());
    assert(this->link.Unlinked());

    SpinlockUnlock(&gLock);
}

Connection::Connection (Channel * server)
    : channel(server)
{
    SpinlockLock(&gLock);

    server->unwaiting_clients.Append(this);

    SpinlockUnlock(&gLock);
}

Connection::~Connection ()
{
    SpinlockLock(&gLock);

    if (this->send_blocked_head.Empty()) {
        if (this->channel) {
            this->channel->unwaiting_clients.Remove(this);
        }
    }
    else {
        if (this->channel) {
            this->channel->waiting_clients.Remove(this);
        }

        while (!this->send_blocked_head.Empty()) {
            Message * message = this->send_blocked_head.PopFirst();

            if (message->mSender) {
                // Unblock sender; it'll deallocate the message when it returns
                // out of SendMessage()
                message->Reply(ERROR_NO_SYS, &IoBuffer::GetEmpty(), 1);
            } else {
                // Async message; no reply needed. Just directly deallocate.
                delete message;
            }
        }
    }

    assert(this->send_blocked_head.Empty());

    SpinlockUnlock(&gLock);
}

Message::Message ()
    : mConnection ()
    , mSender (NULL)
    , mSenderSemaphore (0)
    , mReceiver (NULL)
    , mReceiverSemaphore (0)
    , mResult ()
{
    memset(&mSendData, 0, sizeof(mSendData));
    memset(&mReceiveData, 0, sizeof(mReceiveData));
}

Message::~Message ()
{
}

void Message::Disarm ()
{
    mSenderSemaphore.Disarm();
    mReceiverSemaphore.Disarm();
}

ssize_t Connection::SendMessageAsync (uintptr_t payload)
{
    Message * message;

    SpinlockLock(&gLock);

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            SpinlockUnlock(&gLock);
            return -ERROR_NO_MEM;
        }

        message->mConnection = this;
        message->mSender = NULL;
        message->mSendData.async.payload = payload;

        message->mReceiver = NULL;

        /* Enqueue message for delivery whenever receiver asks for it */
        if (this->send_blocked_head.Empty()) {
            this->channel->unwaiting_clients.Remove(this);
            this->channel->waiting_clients.Append(this);
        }
        this->send_blocked_head.Append(message);

        SpinlockUnlock(&gLock);
        message->mReceiverSemaphore.UpDuringException();
        SpinlockLock(&gLock);
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->mReceiver != NULL);

        message->mConnection = this;
        message->mSender = NULL;
        message->mSendData.async.payload = payload;

        SpinlockUnlock(&gLock);

        /* Allow receiver to wake up */
        message->mReceiverSemaphore.UpDuringException();

        SpinlockLock(&gLock);
    }

    SpinlockUnlock(&gLock);

    return ERROR_OK;
}

ssize_t Connection::SendMessage (
        IoBuffer const msgv[],
        size_t         msgv_count,
        IoBuffer const replyv[],
        size_t         replyv_count
        )
{
    Message * message;
    ssize_t result;

    SpinlockLock(&gLock);

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mConnection = this;
        message->mSender = THREAD_CURRENT();
        message->mSendData.sync.msgv = msgv;
        message->mSendData.sync.msgv_count = msgv_count;
        message->mSendData.sync.replyv = replyv;
        message->mSendData.sync.replyv_count = replyv_count;

        message->mReceiver = NULL;

        /* Enqueue as blocked on the channel */
        if (this->send_blocked_head.Empty()) {
            this->channel->unwaiting_clients.Remove(this);
            this->channel->waiting_clients.Append(this);
        }
        this->send_blocked_head.Append(message);
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->mReceiver != NULL);

        message->mConnection = this;
        message->mSender = THREAD_CURRENT();
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
    delete message;

    return result;
} /* Connection::SendMessage() */

void Channel::SetNameRecord (NameRecord * name_record)
{
    this->name_record = name_record;
}

ssize_t Channel::ReceiveMessage (
        Message  ** context,
        IoBuffer const msgv[],
        size_t msgv_count
        )
{
    Message       * message;
    ssize_t         num_copied;

    SpinlockLock(&gLock);

    if (this->waiting_clients.Empty()) {
        /* No message is waiting in the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mReceiver = THREAD_CURRENT();
        message->mReceiveData.msgv = msgv;
        message->mReceiveData.msgv_count = msgv_count;

        message->mSender = NULL;
        message->mConnection = NULL;

        /* Enqueue as blocked on the channel */
        this->receive_blocked_head.Append(message);
    }
    else {
        /* Some message is waiting in the channel already */
        Connection * client = this->waiting_clients.First();
        assert(!client->send_blocked_head.Empty());
        message = client->send_blocked_head.PopFirst();

        if (client->send_blocked_head.Empty()) {
            this->waiting_clients.Remove(client);
            this->unwaiting_clients.Append(client);
        }

        message->mReceiver = THREAD_CURRENT();
        message->mReceiveData.msgv = msgv;
        message->mReceiveData.msgv_count = msgv_count;
    }

    SpinlockUnlock(&gLock);

    message->mReceiverSemaphore.Down(Thread::STATE_RECEIVE);

    if (message->mSender != NULL) {
        /* Synchronous message */
        *context = message;

        num_copied = TransferPayloadV(
                message->mSender,
                IoVector(message->mSendData.sync.msgv,
                         message->mSendData.sync.msgv_count),
                0,
                message->mReceiver,
                IoVector(message->mReceiveData.msgv,
                         message->mReceiveData.msgv_count),
                0
                );
    }
    else {
        /* Asynchronous message */
        *context = NULL;

        IoBuffer payload_chunk(message->mSendData.async.payload);

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
                message->mReceiver,
                IoVector(message->mReceiveData.msgv,
                         message->mReceiveData.msgv_count),
                0
                );

        /* There will be no reply, so free the Message struct now */
        delete message;
    }

    return num_copied;
} /* Channel::ReceiveMessage() */

size_t Message::GetLength ()
{
    if (mSender != NULL) {
        return IoVector(mSendData.sync.msgv,
                        mSendData.sync.msgv_count).Length();
    } else {
        return sizeof(mSendData.async.payload);
    }
}

ssize_t Message::Read (size_t src_offset,
                       IoBuffer const destv[],
                       size_t destv_count)
{
    if (mSender != NULL) {

        IoVector srcVector(mSendData.sync.msgv,
                           mSendData.sync.msgv_count);

        IoVector dstVector(destv, destv_count);

        if (src_offset >= srcVector.Length() || dstVector.Length() < 0) {
            return 0;
        }

        return TransferPayloadV(mSender, srcVector, src_offset,
                                THREAD_CURRENT(), dstVector, 0);
    } else {

        if (src_offset > sizeof(mSendData.async.payload)) {
            return 0;
        }

        IoBuffer chunk((uint8_t *)&mSendData.async.payload + src_offset,
                    sizeof(mSendData.async.payload) - src_offset);

        return TransferPayloadV(
                THREAD_CURRENT(),   // Dummy sender; won't matter
                IoVector(&chunk, 1),
                0,
                THREAD_CURRENT(),
                IoVector(destv, destv_count),
                0);
    }
}

ssize_t Message::Reply (
        unsigned int status,
        IoBuffer const replyv[],
        size_t replyv_count
        )
{
    size_t result;

    /*
    Make sure that client process hasn't been torn down already. If it has,
    just forget about the reply and deallocate the message object immediately.

    We can determine whether the client process is dead by inspecting whether
    the weak-pointer 'connection' has been nulled out. That happens
    automatically when the object deconstructs itself.
    */
    if (!mConnection) {
        delete this;
        return -ERROR_INVALID;
    }

    assert(mReceiver == THREAD_CURRENT());

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
                mReceiver,
                IoVector(mReceiveData.replyv,
                         mReceiveData.replyv_count),
                0,
                mSender,
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

    /* Abandon any temporary priority boost we had now that the sender is unblocked */
    THREAD_CURRENT()->SetEffectivePriority(THREAD_CURRENT()->assigned_priority);

    /* Sender frees the message after fetching the return value from it */
    return result;
} /* Message::Reply() */

Thread * Message::GetSender ()
{
    return mSender;
}

Thread * Message::GetReceiver ()
{
    return mReceiver;
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

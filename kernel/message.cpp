#include <string.h>

#include <sys/error.h>

#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/mmu.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/thread.hpp>

SyncSlabAllocator<Channel> Channel::sSlab;
SyncSlabAllocator<Connection> Connection::sSlab;
SyncSlabAllocator<Message> Message::sSlab;

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
    assert(this->waiting_clients.Empty());
    assert(this->unwaiting_clients.Empty());
    assert(this->receive_blocked_head.Empty());
    assert(this->link.Unlinked());
}

Connection::Connection (Channel * server)
    : channel(server)
{
    server->unwaiting_clients.Append(this);
}

Connection::~Connection ()
{
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
                message->Reply(ERROR_NO_SYS, NULL, 0);
            } else {
                // Async message; no reply needed. Just directly deallocate.
                delete message;
            }
        }
    }

    assert(this->send_blocked_head.Empty());
}

Message::Message ()
    : mConnection ()
    , mSender (NULL)
    , mReceiver (NULL)
    , mResult ()
{
    memset(&mSendData, 0, sizeof(mSendData));
    memset(&mReceiveData, 0, sizeof(mReceiveData));
}

Message::~Message ()
{
}

ssize_t Connection::SendMessageAsync (uintptr_t payload)
{
    Message * message;

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
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
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->mReceiver != NULL);

        message->mConnection = this;
        message->mSender = NULL;
        message->mSendData.async.payload = payload;

        /* Allow receiver to wake up */
        Thread::BeginTransactionDuringException();
        Thread::MakeReady(message->mReceiver);
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

    if (this->channel->receive_blocked_head.Empty()) {
        /* No receiver thread is waiting on the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mConnection = this;
        message->mSender = THREAD_CURRENT();
        message->mSendData.sync.sender_msgbuf = msgbuf;
        message->mSendData.sync.sender_msgbuf_len = msgbuf_len;
        message->mSendData.sync.sender_replybuf = replybuf;
        message->mSendData.sync.sender_replybuf_len = replybuf_len;

        message->mReceiver = NULL;

        /* Enqueue as blocked on the channel */
        if (this->send_blocked_head.Empty()) {
            this->channel->unwaiting_clients.Remove(this);
            this->channel->waiting_clients.Append(this);
        }
        this->send_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_SEND);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }
    else {
        /* Receiver thread is ready to go */
        message = this->channel->receive_blocked_head.PopFirst();

        assert(message->mReceiver != NULL);

        message->mConnection = this;
        message->mSender = THREAD_CURRENT();
        message->mSendData.sync.sender_msgbuf = msgbuf;
        message->mSendData.sync.sender_msgbuf_len = msgbuf_len;
        message->mSendData.sync.sender_replybuf = replybuf;
        message->mSendData.sync.sender_replybuf_len = replybuf_len;

        /* Temporarily gift our priority to the message-handling thread */
        message->mReceiver->SetEffectivePriority(THREAD_CURRENT()->effective_priority);

        /* Now allow handler to run */
        Thread::BeginTransaction();
        Thread::MakeReady(message->mReceiver);
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_REPLY);
        Thread::RunNextThread();
        Thread::EndTransaction();
    }

    /*
    By the time that the receiver wakes us back up, the reply payload
    has already been copied to replybuf. All we have to do is to chain
    the return value along.
    */

    result = message->mResult;
    delete message;

    return result;
} /* Connection::SendMessage() */

ssize_t Channel::ReceiveMessage (
        Message  ** context,
        void      * msgbuf,
        size_t      msgbuf_len
        )
{
    Message       * message;
    ssize_t         num_copied;

    if (this->waiting_clients.Empty()) {
        /* No message is waiting in the channel at the moment */
        try {
            message = new Message();
        } catch (std::bad_alloc) {
            return -ERROR_NO_MEM;
        }

        message->mReceiver = THREAD_CURRENT();
        message->mReceiveData.receiver_msgbuf = msgbuf;
        message->mReceiveData.receiver_msgbuf_len = msgbuf_len;

        message->mSender = NULL;
        message->mConnection = NULL;

        /* Enqueue as blocked on the channel */
        this->receive_blocked_head.Append(message);

        Thread::BeginTransaction();
        Thread::MakeUnready(THREAD_CURRENT(), Thread::STATE_RECEIVE);
        Thread::RunNextThread();
        Thread::EndTransaction();
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
        message->mReceiveData.receiver_msgbuf = msgbuf;
        message->mReceiveData.receiver_msgbuf_len = msgbuf_len;
    }

    if (message->mSender != NULL) {
        /* Synchronous message */
        *context = message;

        num_copied = TransferPayload(
                message->mSender,
                message->mSendData.sync.sender_msgbuf,
                message->mSendData.sync.sender_msgbuf_len,
                message->mReceiver,
                message->mReceiveData.receiver_msgbuf,
                message->mReceiveData.receiver_msgbuf_len
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
                &message->mSendData.async.payload,
                sizeof(message->mSendData.async.payload),
                message->mReceiver,
                message->mReceiveData.receiver_msgbuf,
                message->mReceiveData.receiver_msgbuf_len
                );

        /* There will be no reply, so free the Message struct now */
        delete message;
    }

    return num_copied;
} /* Channel::ReceiveMessage() */

size_t Message::GetLength ()
{
    if (mSender != NULL) {
        return mSendData.sync.sender_msgbuf_len;
    } else {
        return sizeof(mSendData.async.payload);
    }
}

ssize_t Message::Read (
        size_t src_offset,
        void * dest,
        size_t len
        )
{
    if (mSender != NULL) {

        if (src_offset > mSendData.sync.sender_msgbuf_len || len < 0) {
            return 0;
        }

        return TransferPayload(
                mSender,
                (uint8_t *)mSendData.sync.sender_msgbuf + src_offset,
                mSendData.sync.sender_msgbuf_len - src_offset,
                THREAD_CURRENT(),
                dest,
                len);
    } else {

        if (src_offset > sizeof(mSendData.async.payload) || len < 0) {
            return 0;
        }

        return TransferPayload(
                THREAD_CURRENT(),   // Dummy sender; won't matter
                (uint8_t *)&mSendData.async.payload + src_offset,
                sizeof(mSendData.async.payload) - src_offset,
                THREAD_CURRENT(),
                dest,
                len);
    }
}

ssize_t Message::Reply (
        unsigned int status,
        const void * replybuf,
        size_t replybuf_len
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

    mReceiveData.receiver_replybuf = replybuf;
    mReceiveData.receiver_replybuf_len = replybuf_len;

    /*
    Releasing this thread to run again might make the receiver's reply
    buffer invalid, so do the transfer before returning control. The sender
    will wake up with the reply message already copied into his address
    space's replybuf.
    */
    if (status == ERROR_OK) {
        result = TransferPayload (
                mReceiver,
                mReceiveData.receiver_replybuf,
                mReceiveData.receiver_replybuf_len,
                mSender,
                mSendData.sync.sender_replybuf,
                mSendData.sync.sender_replybuf_len
                );
        mResult = result;
    } else {
        mResult = -status;
    }

    result = status == ERROR_OK ? mResult : ERROR_OK;

    Thread::BeginTransaction();

    /* Sender will get to run again whenever a scheduling decision happens */
    Thread::MakeReady(mSender);

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
    return mSender;
}

Thread * Message::GetReceiver ()
{
    return mReceiver;
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

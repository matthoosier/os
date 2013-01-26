#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <new>

#include <muos/decls.h>
#include <muos/message.h>

#include <kernel/assert.h>
#include <kernel/io.hpp>
#include <kernel/list.hpp>
#include <kernel/nameserver.hpp>
#include <kernel/ref-list.hpp>
#include <kernel/semaphore.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>

BEGIN_DECLS

typedef int Channel_t;
typedef int Connection_t;
typedef int Message_t;

END_DECLS

class Thread;
class Process;

class Message;
class Channel;
class Connection;

/**
 * @class Message message.hpp kernel/message.hpp
 *
 * @brief   The fundamental unit of communication between processes
 *
 * Represents the sender, receiver, and parameters of a message
 *
 * If \a sender is non-NULL, then this message is a normal synchronous
 * communication that must be replied to by the receive in order to
 * unblock the sender. The message payload and buffer for holding the
 * reply are in \a send_data.sync.
 *
 * If \a sender is NULL, then this message is an asychronous message
 * that accepts to reply. In this case, the message payload is in
 * \a send_data.async.
 */
class Message : public RefCounted
{
public:
    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the receiver's virtual memory
     */
    struct ReceiverBufferInfo
    {
        IoBuffer const * msgv;
        size_t msgv_count;

        IoBuffer const * replyv;
        size_t replyv_count;
    };

    enum Type
    {
        TYPE_SYNC,
        TYPE_ASYNC,
    };

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the sender's virtual memory
     */
    union SenderBufferInfo
    {
        struct
        {
            IoBuffer const * msgv;
            size_t msgv_count;

            IoBuffer const * replyv;
            size_t replyv_count;
        }  sync;

        struct Pulse async;
    };

public:
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(Message));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    Message ();

    /**
     * @brief   Query sender of this message
     */
    Thread * GetSender ();

    /**
     * @brief   Query receiver of this message
     */
    Thread * GetReceiver ();

    /**
     * @brief   Transmit a reply to a client's message to complete the
     *          synchronous cycle of a message exchange
     *
     * @return  if zero or greater, the number of bytes written into \a replybuf.
     *          If less than zero, then an error happened and the specific
     *          \c Error_t value is found by negating the return value.
     */
    ssize_t Reply (unsigned int status,
                   IoBuffer const replyv[],
                   size_t replyv_count);


    inline ssize_t Reply (unsigned int status,
                          IoBuffer const & reply)
    {
        return Reply(status, &reply, 1);
    }

    inline ssize_t Reply (unsigned int status,
                          void * reply_buf,
                          size_t reply_buf_len)
    {
        return Reply(status, IoBuffer(reply_buf, reply_buf_len));
    }

    ssize_t Read (size_t src_offset,
                  IoBuffer const destv[],
                  size_t destv_count);

    inline ssize_t Read (size_t src_offset,
                         IoBuffer const & dest)
    {
        return Read(src_offset, &dest, 1);
    }

    inline ssize_t Read (size_t src_offset,
                         void * dest_buf,
                         size_t dest_buf_len)
    {
        return Read(src_offset, IoBuffer(dest_buf, dest_buf_len));
    }

    size_t GetLength ();

    /**
     * @brief   Clear out all back-pointers
     */
    void Dispose ();

protected:
    /**
     * Only RefPtr can free instances
     */
    ~Message ();

private:
    /**
     * @brief   Allocates memory for instances of Message
     */
    static SyncSlabAllocator<Message> sSlab;

    /**
     * @brief   The connection through which the sender sent this
     *          message
     */
    RefPtr<Connection> mConnection;

    /**
     * @brief   The process sending this message
     *
     * NULL if the message is asynchronous
     */
    WeakPtr<Thread> mSender;

    /**
     * @brief   Synchronizes the wait of the sender until
     *          the reply is sent.
     */
    Semaphore mSenderSemaphore;

    /**
     * @brief   The process to whom the message is being sent
     */
    WeakPtr<Thread> mReceiver;

    /**
     * @brief   Synchronizes the wait of the receiver until
     *          the message is sent.
     */
    Semaphore mReceiverSemaphore;

    Type mType;

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the sender's virtual memory
     *
     * See notes on the Message class overview to determine which
     * interpretation of the \c union is valid for this message
     * object.
     */
    SenderBufferInfo mSendData;

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the receiver's virtual memory
     */
    ReceiverBufferInfo mReceiveData;

    /**
     * @brief   Overall success status reported back to the sender
     */
    ssize_t mResult;

    /**
     * @brief   Intrusive link for inserting this message object
     *          into linked lists of messages
     */
    ListElement mQueueLink;

    /**
     * @brief   True if object is about to be deallocated
     */
    bool mDisposed;

    friend class Channel;
    friend class Connection;

    /**
     * For allowing RefPtr to invoke dtor
     */
    friend class RefPtr<Message>;
};

/**
 * @class Connection message.hpp kernel/message.hpp
 *
 * @brief   Client object on which MessageSend() is performed
 */
class Connection : public RefCounted
{
public:
    Connection (RefPtr<Channel> channel);

    void Dispose ();

    /**
     * @brief   Synchronously send a message
     *
     * @return  if zero or greater, then the number of bytes written to
     *          \a replybuf. If negative, then an error happened and
     *          the specific \c Error_t value is found by negating the
     *          return value.
     */
    ssize_t SendMessage (IoBuffer const msgv[],
                         size_t msgv_count,
                         IoBuffer const replyv[],
                         size_t replyv_count);

    inline ssize_t SendMessage (IoBuffer const & msg,
                                IoBuffer const & reply)
    {
        return SendMessage(&msg, 1, &reply, 1);
    }

    inline ssize_t SendMessage (void * msg_buf, size_t msg_buf_len,
                                void * reply_buf, size_t reply_buf_len)
    {
        return SendMessage(IoBuffer(msg_buf, msg_buf_len),
                           IoBuffer(reply_buf, reply_buf_len));
    }

    /**
     * @brief   Asynchronously send a bounded-size message
     *
     * @return  if zero, then successful. If less than zero, then an
     *          error happened and specific \c Error_t value is
     *          found by negating the return value.
     */
    ssize_t SendMessageAsync (int8_t type, uintptr_t value);

    /**
     * @brief   Asynchronously send a bounded-size message
     *
     * @return  if zero, then successful. If less than zero, then an
     *          error happened and the specific \c Error_t value is
     *          found by negating the return value.
     */
    ssize_t SendMessageAsyncDuringException (int8_t type,
                                             uintptr_t value);

    inline RefPtr<Connection> SelfRef ()
    {
        return RefPtr<Connection>(this);
    }

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(Connection));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

private:
    ssize_t SendMessageAsyncInternal (int8_t type,
                                      uintptr_t value,
                                      bool isDuringException);

public:
    /**
     * @brief   Intrusive link for inserting this connection object
     *          into linked lists of connections.
     */
    ListElement link;

protected:
    /**
     * Only RefPtr can free instances
     */
    ~Connection ();


private:
    /**
     * @brief   Allocates instances of Connection
     */
    static SyncSlabAllocator<Connection> sSlab;

    /**
     * @brief   All the senders who are queued waiting to send on
     *          this connection
     */
    RefList<Message, &Message::mQueueLink> mSendBlockedMessages;

    /**
     * @brief   The server object to which this client is connected
     */
    RefPtr<Channel> channel;

    /**
     * @brief   True if object is about to be deallocated
     */
    bool mDisposed;

    friend class Channel;

    /**
     * For allowing RefPtr to invoke dtor
     */
    friend class RefPtr<Connection>;
};

/**
 * @class Channel message.hpp kernel/message.hpp
 *
 * @brief   Server object on which MsgReceive() is performed
 */
class Channel : public RefCounted
{
public:
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(Channel));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    Channel ();

    void Dispose ();

    /**
     * @brief   Install (and take ownership of) a filesystem name
     *          by which this channel is accessible
     */
    void SetNameRecord (NameRecord * name_record);

    /**
     * @brief   Synchronously receive a message
     *
     * @return  if zero or greater, the number of bytes written into \a msgbuf.
     *          If less than zero, then an error happened and the specific
     *          \c Error_t value is found by negating the return value.
     */
    ssize_t ReceiveMessage (RefPtr<Message> & context,
                            IoBuffer const msgv[],
                            size_t msgv_count);

    inline ssize_t ReceiveMessage (RefPtr<Message> & context,
                                   IoBuffer const & msg)
    {
        return ReceiveMessage(context, &msg, 1);
    }

    ssize_t ReceiveMessage (RefPtr<Message> & context,
                            void * msg_buf,
                            size_t msg_buf_len)
    {
        return ReceiveMessage(context, IoBuffer(msg_buf, msg_buf_len));
    }

protected:
    /**
     * Only RefPtr can free instances
     */
    ~Channel ();

private:
    /**
     * @brief   Allocates instances of Channel
     */
    static SyncSlabAllocator<Channel> sSlab;

    /**
     * @brief   Filesystem name (if any) of this channel
     */
    ScopedPtr<NameRecord> name_record;

    /**
     * @brief   True if object is about to be deallocated
     */
    bool mDisposed;

    /**
     * @brief   All the receivers who are queued waiting to receive
     *          on this channel
     */
    RefList<Message, &Message::mQueueLink> mReceiveBlockedMessages;

    /**
     * @brief   Client connections to the channel, who have a message
     *          waiting
     */
    RefList<Connection, &Connection::link> mBlockedConnections;

    /**
     * @brief   Client connections to the channel, who do not have a
     *          message waiting
     */
    RefList<Connection, &Connection::link> mNotBlockedConnections;

public:

    friend class Connection;

    friend class Message;

    /**
     * For allowing RefPtr to invoke dtor
     */
    friend class RefPtr<Channel>;
};

#endif /* __MESSAGE_H__ */

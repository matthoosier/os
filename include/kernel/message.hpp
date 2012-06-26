#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <new>

#include <sys/decls.h>

#include <kernel/list.hpp>
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
class Message
{
public:
    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the receiver's virtual memory
     */
    struct ReceiverBufferInfo
    {
        void * receiver_msgbuf;
        size_t receiver_msgbuf_len;

        const void * receiver_replybuf;
        size_t receiver_replybuf_len;
    };

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the sender's virtual memory
     */
    union SenderBufferInfo
    {
        struct
        {
            const void * sender_msgbuf;
            size_t sender_msgbuf_len;

            void * sender_replybuf;
            size_t sender_replybuf_len;
        }  sync;

        struct
        {
            uintptr_t payload;
        } async;
    };

public:
    void * operator new (size_t) throw (std::bad_alloc);
    void operator delete (void *) throw ();

    Message ();
    ~Message ();

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
    ssize_t Reply (
            unsigned int status,
            const void * replybuf,
            size_t replybuf_len
            );

private:
    /**
     * @brief   The connection through which the sender sent this
     *          message
     */
    WeakPtr<Connection> connection;

    /**
     * @brief   The process sending this message
     *
     * NULL if the message is asynchronous
     */
    Thread * sender;

    /**
     * @brief   The process to whom the message is being sent
     */
    Thread * receiver;

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the sender's virtual memory
     *
     * See notes on the Message class overview to determine which
     * interpretation of the \c union is valid for this message
     * object.
     */
    SenderBufferInfo send_data;

    /**
     * @brief   All the metadata about the buffer addresses/sizes
     *          in the receiver's virtual memory
     */
    ReceiverBufferInfo receive_data;

    /**
     * @brief   Overall success status reported back to the sender
     */
    ssize_t result;

    /**
     * @brief   Intrusive link for inserting this message object
     *          into linked lists of messages
     */
    ListElement queue_link;

    friend class Channel;
    friend class Connection;
};

/**
 * @brief   Client object on which MessageSend() is performed
 */
class Connection : public WeakPointee
{
public:
    void * operator new (size_t) throw (std::bad_alloc);
    void operator delete (void *) throw ();

    Connection (Channel * channel);
    ~Connection ();

    /**
     * @brief   Synchronously send a message
     *
     * @return  if zero or greater, then the number of bytes written to
     *          \a replybuf. If negative, then an error happened and
     *          the specific \c Error_t value is found by negating the
     *          return value.
     */
    ssize_t SendMessage (
            const void    * msgbuf,
            size_t          msgbuf_len,
            void          * replybuf,
            size_t          replybuf_len
            );

    /**
     * @brief   Asynchronously send a bounded-size message
     *
     * @return  if zero, then successful. If less than zero, then an
     *          error happened and the specific \c Error_t value is
     *          found by negating the return value.
     */
    ssize_t SendMessageAsync (uintptr_t payload);

public:
    /**
     * @brief   Intrusive link for inserting this connection object
     *          into linked lists of connections.
     */
    ListElement link;

private:
    /**
     * @brief   All the senders who are queued waiting to send on
     *          this connection
     */
    List<Message, &Message::queue_link> send_blocked_head;

    /**
     * @brief   The server object to which this client is connected
     */
    WeakPtr<Channel> channel;

    friend class Channel;
};

/**
 * @brief   Server object on which MsgReceive() is performed
 */
class Channel : public WeakPointee
{
public:
    void * operator new (size_t) throw (std::bad_alloc);
    void operator delete (void *) throw ();

    Channel ();
    ~Channel ();

    /**
     * @brief   Synchronously receive a message
     *
     * @return  if zero or greater, the number of bytes written into \a msgbuf.
     *          If less than zero, then an error happened and the specific
     *          \c Error_t value is found by negating the return value.
     */
    ssize_t ReceiveMessage (
            Message  ** context,
            void      * msgbuf,
            size_t      msgbuf_len
            );

private:
    /**
     * @brief   All the receivers who are queued waiting to receive
     *          on this channel
     */
    List<Message, &Message::queue_link> receive_blocked_head;

    /**
     * @brief   Client connections to the channel, who have a message
     *          waiting
     */
    List<Connection, &Connection::link> waiting_clients;

    /**
     * @brief   Client connections to the channel, who do not have a
     *          message waiting
     */
    List<Connection, &Connection::link> unwaiting_clients;

public:
    /**
     * @brief   Intrusive link for inserting this channel object
     *          into linked lists of channels.
     */
    ListElement link;

    friend class Connection;
    friend class Message;
};

#endif /* __MESSAGE_H__ */

#include <sys/procmgr.h>
#include <sys/syscall.h>
#include <sys/message.h>
#include <sys/process.h>

#define N_ELEMENTS(_array)  \
    (                       \
    sizeof(_array) /        \
    sizeof(_array[0])       \
    )

int main () {
    struct iovec msgv[3];
    struct iovec replyv[3];
    /* Send message to echo server */
    char msg[] = "Artoo";
    char reply[sizeof(msg)];
    int echoCon = Connect(3, FIRST_CHANNEL_ID);

    /*
    Just for fun, fragment up the message to exercise the
    vectored message passing.
    */
    msgv[0].iov_base = msg;
    msgv[0].iov_len = 1;
    msgv[1].iov_base = msg + msgv[0].iov_len;
    msgv[1].iov_len = 1;
    msgv[2].iov_base = msg + msgv[0].iov_len + msgv[1].iov_len;
    msgv[2].iov_len = sizeof(msg) - msgv[0].iov_len - msgv[1].iov_len;

    replyv[0].iov_base = reply;
    replyv[0].iov_len = 2;
    replyv[1].iov_base = reply + replyv[0].iov_len;
    replyv[1].iov_len = 2;
    replyv[2].iov_base = reply + replyv[0].iov_len + replyv[1].iov_len;
    replyv[2].iov_len = sizeof(reply) - replyv[0].iov_len - replyv[1].iov_len;
    MessageSendV(echoCon, msgv, N_ELEMENTS(msgv), replyv, N_ELEMENTS(replyv));

    /* Terminate */
    return 0;
}

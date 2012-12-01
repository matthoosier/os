#include <sys/procmgr.h>
#include <sys/syscall.h>
#include <sys/message.h>
#include <sys/process.h>

int main () {
    /* Send message to echo server */
    const char send[] = "Artoo";
    char reply[sizeof(send)];
    int echoCon = Connect(2, FIRST_CHANNEL_ID);
    MessageSend(echoCon, send, sizeof(send), reply, sizeof(reply));

    /* Terminate */
    return 0;
}

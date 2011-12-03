#include <sys/procmgr.h>
#include <sys/syscall.h>
#include <sys/message.h>

int main () {
    /* Test out basic syscall mechanism */
    int result;
    result = syscall1(SYS_NUM_ECHO, 37);
    result = result;

    /* Send message to echo server */
    const char send[] = "Artoo";
    char reply[sizeof(send)];
    int echoCon = Connect(2, FIRST_CHANNEL_ID);
    MessageSend(echoCon, send, sizeof(send), reply, sizeof(reply));

    /* Terminate */
    struct ProcMgrMessage msg;
    msg.type = PROC_MGR_MESSAGE_EXIT;
    MessageSend(PROCMGR_CONNECTION_ID, &msg, sizeof(msg), &msg, sizeof(msg));

    return 0;
}

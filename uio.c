#include <sys/error.h>
#include <sys/io.h>
#include <sys/message.h>
#include <sys/procmgr.h>
#include <sys/spinlock.h>

struct IoNotificationSink ntfctn;

const struct IoNotificationSink * OnInterrupt ()
{
    return &ntfctn;
}

int main (int argc, char *argv[]) {

    int chid;

    chid = ChannelCreate();
    ntfctn.connection_id = Connect(SELF_PID, chid);

    void * zeroPtr = MapPhysical(0, 4096 * 4);
    zeroPtr = zeroPtr;

    InterruptHandler_t id = InterruptAttach(OnInterrupt, 4);

    for (;;) {
        int msgid;
        int num = MessageReceive(chid, &msgid, NULL, 0);

        if (msgid > 0) {
            MessageReply(msgid, ERROR_NO_SYS, NULL, 0);
        }
        else if (msgid < 0){
            MessageReply(msgid, ERROR_NO_SYS, NULL, 0);
        }
        else {
            // Pulse received
            id = id;
        }

        num = num;
    }

    InterruptDetach(id);

    return 0;
}

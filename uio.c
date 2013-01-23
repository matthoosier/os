#include <sys/error.h>
#include <sys/io.h>
#include <sys/message.h>
#include <sys/procmgr.h>

typedef union
{
    struct Pulse async;
} my_msg_type;

int main (int argc, char *argv[])
{

    int chid;
    int coid;

    my_msg_type msg;

    chid = ChannelCreate();
    coid = Connect(SELF_PID, chid);

    void * zeroPtr = MapPhysical(0, 4096 * 4);
    zeroPtr = zeroPtr;

    int handler_id = InterruptAttach(coid, 4, NULL);

    for (;;) {
        int msgid;
        int num = MessageReceive(chid, &msgid, &msg, sizeof(msg));

        if (msgid > 0) {
            MessageReply(msgid, ERROR_NO_SYS, NULL, 0);
        }
        else if (msgid < 0){
            MessageReply(msgid, ERROR_NO_SYS, NULL, 0);
        }
        else {
            // Pulse received
            switch (msg.async.type) {
                case PULSE_TYPE_INTERRUPT:
                    InterruptComplete(handler_id);
                    break;
                default:
                    *((char *)NULL) = '\0';
                    break;
            }
        }

        num = num;
    }

    InterruptDetach(handler_id);

    return 0;
}

#include <assert.h>
#include <stdbool.h>

#include <muos/message.h>
#include <muos/naming.h>
#include <muos/process.h>

typedef union
{
    struct Pulse                async;
    struct { char buf[64]; }    sync;

} msg_t;

int main (int argc, char * argv[])
{
    msg_t msg;
    int rcvid;
    int len;
    int client_pid;
    int channel = -1;
    int reap_coid;
    int reap_handler;

    channel = NameAttach("/dev/echo");
    reap_coid = Connect(SELF_PID, channel);

    client_pid = Spawn("echo-client");

    reap_handler = ChildWaitAttach(reap_coid, client_pid);
    ChildWaitArm(reap_handler, 1);

    while (channel >= 0) {

        len = MessageReceive(channel, &rcvid, &msg, sizeof(msg));

        if (rcvid == 0) {
            /* Pulse */
            switch (msg.async.type) {

                case PULSE_TYPE_CHILD_FINISH:
                    assert(msg.async.value == client_pid);
                    ChildWaitDetach(reap_handler);
                    reap_handler = -1;
                    Disconnect(reap_coid);
                    reap_coid = -1;
                    break;

                default:
                    assert(false);
                    break;
            }
        }
        else {
            MessageReply(rcvid, 0, msg.sync.buf, len);
        }
    }

    return 0;
}

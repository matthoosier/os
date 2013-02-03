#include <assert.h>

#include <muos/message.h>
#include <muos/process.h>

int main (int argc, char *argv[]) {

    int pid;

    int chid = ChannelCreate();
    int coid = Connect(SELF_PID, chid);
    int wait_id = ChildWaitAttach(coid, ANY_PID);

    pid = Spawn("echo");
    pid = Spawn("pl011");
    pid = Spawn("crasher");

    pid = pid;

    while (1) {

        /* Child wait is created with an initial count of 0 */
        ChildWaitArm(wait_id, 1);

        struct Pulse pulse;
        int msgid;
        size_t n = MessageReceive(chid, &msgid, &pulse, sizeof(pulse));

        assert(n == sizeof(struct Pulse));
        assert(msgid == 0);
    }

    ChildWaitDetach(wait_id);

    return 0;
}

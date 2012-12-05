#include <sys/message.h>
#include <sys/naming.h>
#include <sys/process.h>

int main (int argc, char * argv[])
{
    char buf[64];
    int rcvid;
    int len;
    int client_pid;
    int channel = -1;

    channel = NameAttach("/dev/echo");

    client_pid = Spawn("echo-client");

    while (channel >= 0) {
        len = MessageReceive(channel, &rcvid, buf, sizeof(buf));
        MessageReply(rcvid, 0, buf, len);
    }

    client_pid = client_pid;

    return 0;
}

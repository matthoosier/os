#include <sys/message.h>

int main (int argc, char * argv[])
{
    int channel;
    char buf[64];
    int rcvid;
    int len;

    channel = ChannelCreate();

    while (1) {
        len = MessageReceive(channel, &rcvid, buf, sizeof(buf));
        MessageReply(rcvid, 0, buf, len);
    }
}

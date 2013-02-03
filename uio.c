#include <stdio.h>
#include <string.h>

#include <muos/array.h>
#include <muos/error.h>
#include <muos/io.h>
#include <muos/message.h>
#include <muos/naming.h>
#include <muos/procmgr.h>

#include "uart.h"

typedef union
{
    struct Pulse async;
} my_msg_type;

int main (int argc, char *argv[])
{
    int chid;
    int coid;
    int uart_coid;

    my_msg_type msg;

    chid = ChannelCreate();
    coid = Connect(SELF_PID, chid);
    uart_coid = NameOpen("/dev/uart");

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
                {
                    static int count = 0;
                    static char buf[128];

                    struct iovec msg_parts[2];
                    struct iovec reply_parts[1];

                    UartMessage msg;
                    UartReply reply;

                    snprintf(buf, sizeof(buf), "Timer int #%d\n", count++);

                    InterruptComplete(handler_id);

                    msg.type = UART_MESSAGE_WRITE;
                    msg.payload.write.len = strlen(buf);

                    msg_parts[0].iov_base = &msg;
                    msg_parts[0].iov_len = offsetof(UartMessage, payload.write.buf);
                    msg_parts[1].iov_base = &buf[0];
                    msg_parts[1].iov_len = strlen(buf);

                    reply_parts[0].iov_base = &reply;
                    reply_parts[0].iov_len = sizeof(reply);

                    MessageSendV(uart_coid,
                                 msg_parts,
                                 N_ELEMENTS(msg_parts),
                                 reply_parts,
                                 0);
                    break;
                }
                default:
                {
                    *((char *)NULL) = '\0';
                    break;
                }
            }
        }

        num = num;
    }

    InterruptDetach(handler_id);

    return 0;
}

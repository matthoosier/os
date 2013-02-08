#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#include <muos/naming.h>
#include <muos/message.h>

#include "uart.h"

int main (int argc, char *argv[]) {

    int uart_coid = NameOpen("/dev/uart");

    while (true) {

        UartMessage msg;
        UartReply reply;

        struct iovec msg_parts[2];
        struct iovec reply_parts[2];

        char buf[1];
        int num;

        msg.type = UART_MESSAGE_READ;
        msg.payload.read.len = sizeof(buf);

        msg_parts[0].iov_base = &msg;
        msg_parts[0].iov_len = sizeof(msg);

        reply_parts[0].iov_base = &reply;
        reply_parts[0].iov_len = offsetof(UartReply, payload.read.buf);
        reply_parts[1].iov_base = &buf[0];
        reply_parts[1].iov_len = msg.payload.read.len;

        num = MessageSendV(uart_coid, msg_parts, 1, reply_parts, 2);

        if (num < offsetof(UartReply, payload.read.len) +
                  sizeof(reply.payload.read.len))
        {
            /*
            Didn't get a valid 'len' field filled in on the response
            */
            continue;
        }

        /* Flip the casing of the character */
        for (num = 0; num < reply.payload.read.len; ++num) {

            if (buf[num] >= 'A' && buf[num] <= 'Z') {
                buf[num] = 'a' + (buf[num] - 'A');
            }

            else if (buf[num] >= 'a' && buf[num] <= 'z') {
                buf[num] = 'A' + (buf[num] - 'a');
            }
        }

        msg.type = UART_MESSAGE_WRITE;
        msg.payload.write.len = reply.payload.read.len;

        msg_parts[0].iov_base = &msg;
        msg_parts[0].iov_len = offsetof(UartMessage, payload.write.buf);
        msg_parts[1].iov_base = &buf[0];
        msg_parts[1].iov_len = msg.payload.write.len;

        reply_parts[0].iov_base = &reply;
        reply_parts[0].iov_len = sizeof(reply);

        num = MessageSendV(uart_coid, msg_parts, 2, reply_parts, 1);
    }
}

#ifndef __UART_H__
#define __UART_H__

#include <stddef.h>

typedef enum
{
    UART_MESSAGE_READ,
    UART_MESSAGE_WRITE,
} UartMessageType;

typedef struct
{
    UartMessageType type;

    union
    {
        struct
        {
            size_t len;
        } read;

        struct
        {
            size_t len;
            char buf[0];
        } write;
    } payload;
} UartMessage;

typedef struct
{
    UartMessageType type;

    union
    {
        struct
        {
            size_t len;
        } write;

        struct
        {
            size_t len;
            char buf[0];
        } read;
    } payload;
} UartReply;

#endif

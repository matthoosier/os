#ifndef __INTERRUPT_HANDLER_H__
#define __INTERRUPT_HANDLER_H__

#include <stdbool.h>

#include <sys/decls.h>
#include <sys/io.h>

#include <kernel/list.h>
#include <kernel/message.h>
#include <kernel/process.h>

BEGIN_DECLS

/**
 * Signature of an IRQ handler function.
 */
typedef void (*IrqKernelHandlerFunc) ();

/**
 * Record attached to
 */
struct UserInterruptHandlerRecord
{
    struct
    {
        int             irq_number;
        Pid_t           pid;
        Connection_t    coid;
        uintptr_t       param;
    } handler_info;

    struct
    {
        bool            masked;
    } state_info;

    struct list_head link;
};

/**
 * Set up the stacks used for interrupt handling
 */
void InterruptsConfigure();

void InterruptAttachKernelHandler (unsigned int irq_number, IrqKernelHandlerFunc f);

void InterruptAttachUserHandler (
        struct UserInterruptHandlerRecord * handler
        );

int InterruptCompleteUserHandler (
        struct UserInterruptHandlerRecord * handler
        );

void InterruptDetachUserHandler (
        struct UserInterruptHandlerRecord * handler
        );

struct UserInterruptHandlerRecord * UserInterruptHandlerRecordAlloc ();
void UserInterruptHandlerRecordFree (struct UserInterruptHandlerRecord * record);

void InterruptHandler ();

void InterruptUnmaskIrq (int n);

void InterruptMaskIrq (int n);

END_DECLS

#endif /* __INTERRUPT_HANDLER_H__ */

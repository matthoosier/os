#ifndef __INTERRUPT_HANDLER_H__
#define __INTERRUPT_HANDLER_H__

#include <sys/decls.h>
#include <sys/io.h>

#include <kernel/list.h>

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
    InterruptHandlerFunc    func;
    int                     pid;

    struct list_head        link;
};

/**
 * Set up the stacks used for interrupt handling
 */
void InterruptsConfigure();

void InterruptAttachKernelHandler (int irq_number, IrqKernelHandlerFunc f);

void InterruptAttachUserHandler (
        int irq_number,
        struct UserInterruptHandlerRecord * handler
        );

struct UserInterruptHandlerRecord * UserInterruptHandlerRecordAlloc ();
void UserInterruptHandlerRecordFree (struct UserInterruptHandlerRecord * record);

void InterruptHandler ();

void InterruptUnmaskIrq ();

void InterruptMaskIrq ();

END_DECLS

#endif /* __INTERRUPT_HANDLER_H__ */

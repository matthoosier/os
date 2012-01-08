#ifndef __IO_H__
#define __IO_H__

#include <stddef.h>
#include <stdint.h>

#include <sys/decls.h>

BEGIN_DECLS

typedef int InterruptHandler_t;

/**
 * One-time call by a user program to establish a hook to be
 * invoked when a particular IRQ arrives.
 *
 * @connection_id
 *          the channel into which an async event should be
 *          injected upon @irq_number being raised.
 *
 * @param
 *          the payload that will be delivered into @connection_id
 *          as the content of the message indicating the interrupt
 *          has happened.
 *
 * @return  a token that's used in all future references to
 *          signaling a particular IRQ instance to be fully
 *          handled and/or remove the interrupt handler hook.
 *
 * If negative, the return value is the negated error code of
 * what went wrong while trying to register.
 */
InterruptHandler_t InterruptAttach (
        int connection_id,
        int irq_number,
        void * param
        );

/**
 * One-time call by a user program to terminate an interrupt-
 * handling hook.
 *
 * @id  the return value from InterruptAttach() indicating this
 *      unique IRQ-handling entity
 */
int InterruptDetach (
        InterruptHandler_t id
        );

/**
 * Called a user-program which has installed an interrupt-handling
 * hook, to indicate that one particular interrupt instance is done
 * being servied, and the system can allow interrupts to arrive from
 * the hardware again.
 *
 * Be sure to clear the hardware's interrupt source before calling
 * InterruptComplete(), or you will have an infinite sequence of
 * interrupts arrive.
 */
int InterruptComplete (
        InterruptHandler_t id
        );

void * MapPhysical (
        uintptr_t physaddr,
        size_t  len
        );

END_DECLS

#endif /* __IO_H__ */

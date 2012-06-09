#include <asm-offsets.h>
#include "arm-defs.inc"

    .section .text.vector

vector:
    /* Processor reset. Serviced in svc mode. */
    b reset_handler

    /* Undefined instruction. Serviced in und mode. */
    b undef_handler

    /* Software interrupt. Serviced in svc mode. */
    b swi_handler

    /* Instruction fetch memory abort. Serviced in abt mode. */
    b pabt_handler

    /* Data access memory abort. Serviced in abt mode. */
    b dabt_handler

    /* Reserved for future use. */
    b reserved_handler

    /* General-purpose interrupt. Serviced in irq mode. */
    b irq_handler

    /* Fast interrupt. Serviced in fiq mode. */
    b fiq_handler

reset_handler:
    b reset_handler

undef_handler:
    b undef_handler

swi_handler:
    /* User R15 (stored in LR_svc automatically by the SWI) */
    stmfd sp!, {lr}

    /* LR is now free, so fetch the address of the current TCB into it              */
    push {r0-r3,ip}
    mov r0, sp
    bl ThreadStructFromStackPointer
    mov lr, r0
    pop {r0-r3,ip}

    /* Push main user registers */
    stmfd sp, {r0 - r14}^
    nop
    sub sp, sp, #(15 * 4)

    /* Store R0-R12 all out to the TCB's user register-save area */
    add lr, lr, U_R0
    ldm sp, {r0 - r12}
    stm lr, {r0 - r12}
    sub lr, lr, U_R0

    /* Store R13-R15 all out to the TCB's user register-save area */
    add lr, lr, U_R13
    add sp, sp, #(13 * 4)
    ldm sp, {r0 - r2}
    stm lr, {r0 - r2}
    sub sp, sp, #(13 * 4)
    sub lr, lr, U_R13

    /* Store user-mode CPSR into TCB's user register-save area */
    mrs r8, spsr
    str r8, [lr, U_CPSR]

    /*
    Okay, all the user registers are saved into Thread::u_reg. It's
    safe to deallocate the stack space that temporarily held them.
    */
    add sp, sp, #(16 * 4)

    /* Re-enable regular interrupts (the SWI instruction disabled them) */
    mrs r8, cpsr
    bic r8, r8, #cpsr_i_bit
    msr cpsr, r8

    /* Do actual work here... */
    mov r0, lr
    bl do_syscall

swi_handler__exit$:
    /* Find address of TCB for current thread   */
    mov r0, sp
    bl ThreadStructFromStackPointer
    mov lr, r0

    /* Disable interrupts to keep context change atomic */
    mrs r8, cpsr
    orr r8, r8, #cpsr_i_bit
    msr cpsr, r8

    /* Make room on stack to reconstruct user registers from Thread::u_reg */
    sub sp, sp, #(16 * 4)

    /* Transfer user-mode CPSR out of TCB's user register-save area */
    ldr r8, [lr, U_CPSR]
    msr spsr, r8

    /* Restore main user registers (R0-R12) */
    add lr, lr, U_R0
    ldm lr, {r0 - r12}      /* TCB -> regs */
    stm sp, {r0 - r12}      /* regs -> stack */
    sub lr, lr, U_R0

    /* Restore main user registers (R13 - R15) */
    add lr, lr, U_R13
    add sp, sp, #(13 * 4)
    ldm lr, {r0 - r2}
    stm sp, {r0 - r2}
    sub sp, sp, #(13 * 4)
    sub lr, lr, U_R13

    /* Pop saved user R0-R14 from stack into live registers */
    ldmfd sp, {r0 - r14}^
    nop
    add sp, sp, #(15 * 4)

    /* Pop user PC into LR in preparation for special exception-return MOVS op */
    ldmfd sp!, {lr}

    /* Atomically transfer current LR into PC, and load SPSR into CPSR */
    movs pc, lr

/**
 * Point used to restart a user-space thread which was pre-empted by
 * an interrupt.
 */
swi_handler__restart_after_irq$:
    /* Drop the main scheduling lock        */
    bl ThreadEndTransaction

    /* Now just jump back to the main return sequence in the syscall handler */
    b swi_handler__exit$

pabt_handler:
    b pabt_handler

dabt_handler:
    b dabt_handler

reserved_handler:
    b reserved_handler

irq_handler:
    /* Interrupted execution's PC. On ARM IRQ's the PC is too far ahead by one word */
    sub lr, lr, #4
    stmfd sp!, {lr}

    /* Interrupted execution's processor-status register */
    mrs lr, spsr
    stmfd sp!, {lr}

    /* Main registers */
    stmfd sp!, {r0 - r12}

    bl InterruptHandler

    /*
    Ask whether something happened during the ISR handler that made the
    scheduler need to run again.
    */
    bl ThreadResetNeedResched       /* r0 := boolean result                 */

    /* If not, we're done */
    teq r0, #0
    beq irq_handler__normal_exit$

    /* Fetch interrupted task's kernel stack pointer */
    mrs r1, cpsr
    cps #svc
    mov r0, sp
    msr cpsr, r1

    /**
     * Deduce kernel thread structure location from the task's stack pointer.
     */

    mov r2, r0                      /* r2 := interrupted task's SP          */
    bl ThreadStructFromStackPointer /* r0 := 'struct Thread *'              */

    /**
     * Figure out execution mode processor was in before IRQ arrived.
     */
    ldr r1, [sp, #52]               /* r1 := spsr                           */
    bic r1, r1, #0xffffffe0         /* r1 := spsr & 0b11111                 */

    teq r1, #usr
    beq irq_handler__do_boot_user$

    teq r1, #svc
    beq irq_handler__do_boot_kernel$

    b irq_handler__normal_exit$

/**
 * Copy current user process's registers to its kernel stack
 * (where they normally get stored during a syscall).
 *
 * Requirements:
 *
 * r0: Interrupted task's TCB (struct Thread *)
 * r2: Interrupted task's SP
 * sp:  Bottom of IRQ stack's saved-register area
 */
irq_handler__do_boot_user$:

    /* Acquire scheduler lock */
    push {r0-r3}
    bl ThreadBeginTransactionEndingIrq
    pop {r0-r3}

    /* R15 */
    ldr r3, [sp, #(14 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, U_R15]         /* Store to TCB                         */

    /* R14 */
    stmfd sp, {r14}^            /* Load from banked register            */
    nop
    sub sp, sp, #4              /* (Stack is very temp. storage)        */
    ldmfd sp!, {r3}
    str r3, [r0, U_R14]         /* Store to TCB                         */

    /* R13 */
    stmfd sp, {r13}^            /* Load from banked register            */
    nop
    sub sp, sp, #4              /* (Stack is very temp. storage)        */
    ldmfd sp!, {r3}
    str r3, [r0, U_R13]         /* Store to TCB                         */

    /* R12 */
    ldr r3, [sp, #(12 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, U_R12]         /* Store to TCB                         */

    /* R0 - R11 */
    ldm sp, {r1 - r12}
    add r0, r0, U_R0            /* r0 += offsetof(Thread, u_reg[0])     */
    stm r0, {r1 - r12}
    sub r0, r0, U_R0            /* r0 -= offsetof(Thread, u_reg[0])     */

    /* SPSR */
    ldr r3, [sp, #(13 * 4)]     /* Load from staged SPSR on IRQ stack   */
    orr r3, r3, #cpsr_i_bit     /* Synthesized syscall should start out */
                                /* with interrupts disabled so that     */
                                /* it's protected from preemption until */
                                /* it finishes the handshake by calling */
                                /* ThreadEndTransaction().              */
                                /*                                      */
    str r3, [r0, U_CPSR]        /* Store to TCB                         */

    /**
     * Because the task we're ejecting from the CPU was running in user
     * mode, its kernel thread had no meaningful state (except a stack
     * pointer). This means that we are free to designate whatever entry
     * point we want for the thread when the scheduler next gets around
     * to resuming it, without losing any state.
     *
     * We'll strategically set the pre-empted task's kernel thread PC to
     * be the pointer during the syscall return sequence that unravels the
     * stored registers and returns back to the spot in user-space where
     * the task was interrupted.
     *
     * r0: still holds address of the interrupted task's kernel thread
     * r2: holds the value of interrupted task's SP_svc less the space
     *     required to hold the user-space saved registers we just stored
     *     on the task's kernel stack.
     */
    str r2, [r0, K_R13]         /* Outgoing task's kernel SP            */
    ldr r2, =swi_handler__restart_after_irq$
    str r2, [r0, K_R15]         /* Outgoing task's kernel PC            */

    b irq_handler__do_load_next_task$

/**
 * Copy current task's registers to kernel thread register-save area.
 *
 * Requirements:
 *
 * r2: Interrupted task's SP
 * sp:  Bottom of IRQ stack's saved-register area
 */
irq_handler__do_boot_kernel$:

    /* Acquire scheduler lock */
    push {r0-r3}
    bl ThreadBeginTransactionEndingIrq
    pop {r0-r3}

    /* SPSR */
    ldr r3, [sp, #(13 * 4)]     /* Load from staged SPSR on IRQ stack   */
    orr r3, r3, #cpsr_i_bit     /* Interrupted task should retain       */
                                /* preemption protection when it's      */
                                /* restored next time.                  */
    str r3, [r0, K_CPSR]        /* Store to kernel thread structure     */

    /* R15 */
    ldr r3, [sp, #(14 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, K_R15]         /* Store to kernel thread structure     */

    /* R14 */
    mrs r4, cpsr
    cps #svc
    mov r3, r14
    msr cpsr, r4                /* Load from banked register            */
    str r3, [r0, K_R14]         /* Store to kernel thread structure     */

    /* R13 */
    mrs r4, cpsr
    cps #svc
    mov r3, r13
    msr cpsr, r4                /* Load from banked register            */
    str r3, [r0, K_R13]         /* Store to kernel thread structure     */

    /* R12 */
    ldr r3, [sp, #(12 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, K_R12]         /* Store to kernel thread structure     */

    /* R0 - R11 */
    ldm sp, {r1 - r12}
    add r0, r0, K_R0            /* r0 += offsetof(Thread, k_reg[0])     */
    stm r0, {r1 - r12}
    sub r0, r0, K_R0            /* r0 -= offsetof(Thread, k_reg[0])     */

    /*
    Insert a tiny little restart shim into the interrupted kernel
    thread, that will cause it to drop the scheduler transaction
    lock upon its restart.

    ARM uses the full-descending stack model, so tactically we do this
    by first decrementing the stack pointer and then writing to the
    location addressed by it.
    */

    ldr r4, [r0, K_R15]         /* r4 := interrupted task's PC          */
    ldr r3, [r0, K_R13]         /* r3 := interrupted task's SP          */

    sub r3, r3, #4              /* Decrement interrupted task's stack   */
    str r4, [r3]                /* Stash PC in stack                    */
    str r3, [r0, K_R13]         /* Write tweaked SP back to saved regs  */

    ldr r5, =restart_interrupted_kernel_thread$
    str r5, [r0, K_R15]         /* Hijack the saved PC of outgoing task */

/**
 * Requirements:
 *
 * r0: holds address of outgoing task's kernel 'struct Thread'
 */
irq_handler__do_load_next_task$:

    /**
     * Mark the booted thread as ready to run
     */
    push {r0}
    bl ThreadMakeReady              /* r0 still holds 'struct Thread *' */
    pop {r0}

    /**
     * Find out pagetable used by outgoing thread
     */
    bl ThreadGetProcess             /* r0 := 'struct Process *'         */
    teq r0, #0                      /* Is r0 NULL?                      */
    blne ProcessGetTranslationTable /* r0 := 'struct TranslationTable *'*/
    mov r8, r0                      /* r8 := 'struct TranslationTable *'*/


    /**
     * Fetch next thread to run
     */
    bl ThreadDequeueReady       /* r0 := 'struct Thread *'          */

    /**
     * Enforce that the dequeue call found a thread -- at worst, it
     * should find the one we just booted.
     */
    push {r0}
    teq r0, #0                  /* Is r0 == NULL?                   */
    moveq r0, #0                /* If so, r0 := FALSE               */
    movne r0, #1                /* If not, r0 := TRUE               */
    bl assert                   /* assert(r0)                       */
    pop {r0}

    /**
     * Copy next task's registers out of kernel-thread register save area
     * and into IRQ stack's staging area. Normal exit path from IRQ handler
     * will restore them to the actual machine registers.
     *
     * Requirements:
     *
     * r0:  Base address of incoming's 'struct Thread'
     * r8:  Base address of outgoing's 'struct TranslationTable'
     * sp:  Bottom of IRQ stack's saved-register area
     */

    /* SPSR */
    ldr r3, [r0, K_CPSR]        /* Load from kernel thread structure    */
    str r3, [sp, #(13 * 4)]     /* Store as staged SPSR on IRQ stack    */

    /* R15 */
    ldr r3, [r0, K_R15]         /* Load from kernel thread structure    */
    str r3, [sp, #(14 * 4)]     /* Store on IRQ stack                   */

    /* R14 */
    ldr r3, [r0, K_R14]         /* Load from kernel thread structure    */
    mrs r4, cpsr
    cps #svc
    mov r14, r3                 /* Store in banked register             */
    msr cpsr, r4

    /* R13 */
    ldr r3, [r0, K_R13]         /* Load from kernel thread structure    */
    mrs r4, cpsr
    cps #svc
    mov r13, r3                 /* Store in banked register             */
    msr cpsr, r4

    /* R12 */
    ldr r3, [r0, K_R12]         /* Load from kernel thread structure    */
    str r3, [sp, #(12 * 4)]     /* Store on IRQ stack                   */

    /* R11 */
    ldr r3, [r0, K_R11]         /* Load from kernel thread structure    */
    str r3, [sp, #(11 * 4)]     /* Store on IRQ stack                   */

    /* R10 */
    ldr r3, [r0, K_R10]         /* Load from kernel thread structure    */
    str r3, [sp, #(10 * 4)]     /* Store on IRQ stack                   */

    /* R9 */
    ldr r3, [r0, K_R9]          /* Load from kernel thread structure    */
    str r3, [sp, #(9 * 4)]      /* Store on IRQ stack                   */

    /* R8 */
    ldr r3, [r0, K_R8]          /* Load from kernel thread structure    */
    str r3, [sp, #(8 * 4)]      /* Store on IRQ stack                   */

    /* R7 */
    ldr r3, [r0, K_R7]          /* Load from kernel thread structure    */
    str r3, [sp, #(7 * 4)]      /* Store on IRQ stack                   */

    /* R6 */
    ldr r3, [r0, K_R6]          /* Load from kernel thread structure    */
    str r3, [sp, #(6 * 4)]      /* Store on IRQ stack                   */

    /* R5 */
    ldr r3, [r0, K_R5]          /* Load from kernel thread structure    */
    str r3, [sp, #(5 * 4)]      /* Store on IRQ stack                   */

    /* R4 */
    ldr r3, [r0, K_R4]          /* Load from kernel thread structure    */
    str r3, [sp, #(4 * 4)]      /* Store on IRQ stack                   */

    /* R3 */
    ldr r3, [r0, K_R3]          /* Load from kernel thread structure    */
    str r3, [sp, #(3 * 4)]      /* Store on IRQ stack                   */

    /* R2 */
    ldr r3, [r0, K_R2]          /* Load from kernel thread structure    */
    str r3, [sp, #(2 * 4)]      /* Store on IRQ stack                   */

    /* R1 */
    ldr r3, [r0, K_R1]          /* Load from kernel thread structure    */
    str r3, [sp, #(1 * 4)]      /* Store on IRQ stack                   */

    /* R0 */
    ldr r3, [r0, K_R0]          /* Load from kernel thread structure    */
    str r3, [sp, #(0 * 4)]      /* Store on IRQ stack                   */

    /**
     * Find out pagetable used by outgoing thread
     */
    bl ThreadGetProcess             /* r0 := 'struct Process *'         */
    teq r0, #0                      /* Is r0 NULL?                      */
    blne ProcessGetTranslationTable /* r0 := 'struct TranslationTable *'*/
    mov r9, r0                      /* r9 := 'struct TranslationTable *'*/

    /**
     * Pagetable swap
     */
    teq r8, r9                      /* Are outgoing and incoming        */
                                    /* pagetables the same?             */

    beq irq_handler__normal_exit$   /* If so, we're done.               */

    mov r0, r9                      /* r0 := address of incoming        */
                                    /* thread's pagetable               */

    bl TranslationTableSetUser      /* Install new pagetable...         */
    b irq_handler__normal_exit$     /* Done.                            */

/**
 * Normal unraveling point. Assumes that the only thing on the
 * IRQ stack at this point are the registers of the task to be
 * resumed.
 */
irq_handler__normal_exit$:

    /* Main registers */
    ldmfd sp!, {r0 - r12}

    /* Interrupted execution's processor-status register */
    ldmfd sp!, {lr}
    msr spsr, lr

    /* Interrupted execution's PC */
    ldmfd sp!, {lr}

    /* Atomically transfer saved PC to live register and move SPSR into CPSR */
    subs pc, lr, #0

fiq_handler:
    b fiq_handler

/**
 * Shim used to restart a kernel task that got booted by an interrupt
 * handler.
 *
 * The scheduler expects that the switched-to thread always drops
 * the "scheduler transaction" lock as the first thing it does, so
 * we have to patch up the execution of the interrupted kernel thread
 * to do this.
 *
 * The IRQ handler stashed the real PC of the interrupted task as the
 * most recently pushed word on the stack.
 */
restart_interrupted_kernel_thread$:
    push {r0-r3, r4, r12, lr}

    /*
    The CPSR at this moment looks (except for the interrupt-enable bits)
    exactly like it should from the interrupted thread's perspective.
    But we're going to be doing some junk to the condition-code flags in
    it when we perform the upcoming spinlock release.

    So we have to save a copy of it.
    */
    mrs r4, cpsr
    bic r4, r4, #cpsr_i_bit
    bic r4, r4, #cpsr_f_bit

    bl ThreadEndTransactionFromRestart
    msr spsr, r4
    pop {r0-r3, r4, r12, lr}

    /*
    Finish the shim by consuming the extra word encoding the PC and
    jumping to it.
    */
    ldmfd sp!, {pc}^

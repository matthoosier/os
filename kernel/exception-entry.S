#include <asm-offsets.h>

#define ASSERTS_ENABLED 1
#define DEBUG_MESSAGES  0

#define IRQ_PC_RUNAHEAD #4
#define ABT_PC_RUNAHEAD #4

    .section .text

    .global reset_handler
    .global undef_handler
    .global swi_handler
    .global pabt_handler
    .global dabt_handler
    .global reserved_handler
    .global irq_handler
    .global fiq_handler

/**
 * Inputs:
 *
 * Modifies:
 *   sp: the register-save area is pushed onto the stack
 *
 * Corrupts:
 *   All registers except pc and sp (modified as described)
 */
.macro exception_save_regs_to_stack
    /* Empty slot to preserve 8-byte stack alignment */
    sub sp, sp, #4

    /* Carve hole for CPSR */
    sub sp, sp, #4

    /* PC */
    stmfd sp!, {lr}

    /* Carve hole for R14 and R13 */
    sub sp, sp, #8

    /* R0 - R12 */
    stmfd sp!, {r0-r12}

    /* Fill in CPSR */
    mrs r2, spsr
    str r2, [sp, #(4 * 16)]

    /* Determine what mode to get banked R13/R14 from */
    mvn r0, ARM_PSR_MODE_MASK
    bic r1, r2, r0

    teq r1, ARM_PSR_MODE_USR_BITS
    beq 0f

    teq r1, ARM_PSR_MODE_SVC_BITS
    beq 1f

    mov r0, FALSE
    bl assert

0:
    /* Get user mode banked R13/R14 */
    stmfd sp, {r13, r14}^
    nop
    sub sp, sp, #8
    ldmfd sp!, {r0, r1}
    str r0, [sp, #(4 * 13)]
    str r1, [sp, #(4 * 14)]
    b 2f

1:
    /* Get supervisor mode banked R13/R14 */
    mrs r0, cpsr
    cps ARM_PSR_MODE_SVC_BITS
    mov r1, r13
    mov r2, r14
    msr cpsr, r0
    str r1, [sp, #(4 * 13)]
    str r2, [sp, #(4 * 14)]
    b 2f

2:
.endm

/**
 * Inputs:
 *   r0: address of task's kernel Thread object
 *   sp: point at base of exception stack's register-save area
 *
 * Modifies:
 *   sp: the register-save area is popped off the stack
 *
 * Corrupts:
 *   All registers except pc and sp (modified as described)
 */
.macro exception_load_regs_from_stack
    /* Examine saved PSR to determine target processor mode */
    ldr r0, [sp, #(4 * 16)]     /* Load saved CPSR                  */
    mvn r1, ARM_PSR_MODE_MASK   /* Find ~ARM_PSR_MODE_MASK          */
    bic r2, r0, r1              /* r2 := CPSR & ARM_PSR_MODE_MASK   */

    teq r2, ARM_PSR_MODE_USR_BITS
    beq 0f

    teq r2, ARM_PSR_MODE_SVC_BITS
    beq 1f

    mov r0, FALSE
    bl assert

0:
    /* Load user mode banked registers */
    ldr r0, [sp, #(4 * 13)]
    ldr r1, [sp, #(4 * 14)]
    stmfd sp!, {r0, r1}
    ldmfd sp, {r13, r14}^
    nop
    add sp, sp, #8
    b 2f

1:
    /* Load supervisor mode banked registers */
    ldr r1, [sp, #(4 * 13)]
    ldr r2, [sp, #(4 * 14)]
    mrs r0, cpsr
    cps ARM_PSR_MODE_SVC_BITS
    mov r13, r1
    mov r14, r2
    msr cpsr, r0
    b 2f

2:
    /* CPSR */
    ldr r0, [sp, #(4 * 16)]
    msr spsr, r0

    /* R0 - R12 */
    ldmfd sp!, {r0-r12}

    /* R13 and R14 (already loaded into live registers) */
    add sp, sp, #8

    /* PC */
    ldmfd sp!, {lr}

    /* CPSR (already loaded into live register) */
    add sp, sp, #4

    /* Empty slot used to pad out stack to 8-byte alignment */
    add sp, sp, #4
.endm

/**
 * Inputs:
 *   r0: address of task's kernel Thread object
 *   sp: point at base of exception stack's register-save area
 *
 * Corrupts:
 *   All registers except r0, sp, lr, pc
 */
.macro exception_store_user_saveregs_from_stack
    /* R0 - R11 out to PCB */
    ldm sp, {r1 - r12}
    add r0, r0, U_R0
    stm r0, {r1 - r12}
    sub r0, r0, U_R0

    /* R12 - R15 out to PCB */
    add sp, sp, #(4 * 12)
    ldm sp, {r1 - r4}
    sub sp, sp, #(4 * 12)
    add r0, r0, U_R12
    stm r0, {r1 - r4}
    sub r0, r0, U_R12

    /* CPSR out to PCB */
    ldr r3, [sp, #(4 * 16)]
    str r3, [r0, U_CPSR]
.endm

.macro exception_store_kernel_saveregs_from_stack
    /* R0 - R11 out to TCB */
    ldm sp, {r1 - r12}          /* Load from IRQ stack                  */
    add r0, r0, K_R0            /* Store to kernel thread structure     */
    stm r0, {r1 - r12}
    sub r0, r0, K_R0

    /* R12 - R15 out to TCB */
    add sp, sp, #(4 * 12)       /* Load from IRQ stack                  */
    ldm sp, {r1 - r4}
    sub sp, sp, #(4 * 12)
    add r0, r0, K_R12           /* Store to kernel thread structure     */
    stm r0, {r1 - r4}
    sub r0, r0, K_R12

    /* CPSR out to TCB */
    ldr r3, [sp, #(4 * 16)]
    str r3, [r0, K_CPSR]
.endm

/**
 * Inputs:
 *   r0: address of task's kernel Thread object
 *   sp: point at base of exception stack's register-save area
 *
 * Corrupts:
 *   All registers except r0, sp, lr, pc
 */
.macro exception_load_stack_from_kernel_saveregs
    /* R0 - R6 in from TCB */
    add r0, r0, K_R0
    ldm r0, {r1 - r7}
    sub r0, r0, K_R0
    stm sp, {r1 - r7}

    /* R7 - 13 in from TCB */
    add r0, r0, K_R7
    ldm r0, {r1 - r7}
    sub r0, r0, K_R7
    add sp, sp, #(4 * 7)
    stm sp, {r1 - r7}
    sub sp, sp, #(4 * 7)

    /* R14 - R15 in from TCB */
    add r0, r0, K_R14
    ldm r0, {r1 - r2}
    sub r0, r0, K_R14
    add sp, sp, #(4 * 14)
    stm sp, {r1 - r2}
    sub sp, sp, #(4 * 14)

    /* CPSR in from TCB */
    ldr r3, [r0, K_CPSR]
    str r3, [sp, #(4 * 16)]
.endm

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
    bic r8, r8, ARM_PSR_I_VALUE
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
    orr r8, r8, ARM_PSR_I_VALUE
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
    /* For now, treat prefetch abort same as data abort */
    b dabt_handler

dabt_handler:
    /*
    On data aborts, the saved PC value is 2 words ahead of the
    instruction that faulted. To restart on the immediately
    following instruction, subtract 4.
    */
    sub lr, lr, ABT_PC_RUNAHEAD

    push {r0-r3,r12,lr}

    /* Only user-mode code should be generating exceptions */
    mrs r3, spsr
    mvn r2, ARM_PSR_MODE_MASK
    bic r3, r3, r2
    teq r3, ARM_PSR_MODE_USR_BITS
    movne r0, FALSE
    moveq r0, TRUE
    bl assert

    /* Restore corrupted registers */
    pop {r0-r3,r12,lr}

    /* Store off live registers to exception stack */
    exception_save_regs_to_stack

    /* Figure out what task owns this process */
    mrs r1, cpsr
    cps ARM_PSR_MODE_SVC_BITS
    mov r0, sp
    msr cpsr, r1
    bl ThreadStructFromStackPointer     /* r0 := address of aborted task    */

    /* Update saved user registers with latest copy */
    exception_store_user_saveregs_from_stack

    /* Make the busted process request its own termination                  */
    ldr r1, =dabt_handler__restart_for_termination$
    str r1, [r0, K_R15]

    /* Don't want interrupts to be disabled in the synthesized syscall      */
    ldr r2, [r0, K_CPSR]
    bic r2, ARM_PSR_I_VALUE
    str r2, [r0, K_CPSR]

    /* Load the process's kernel task registers, to synthesize a syscall    */
    exception_load_stack_from_kernel_saveregs

    /* Transfer stack registers to live on CPU */
    exception_load_regs_from_stack

    /* Return right back to the calling code, adjusting saved PC for run-ahead */
    movs pc, lr

dabt_handler__restart_for_termination$:
    bl ScheduleSelfAbort
    mov r0, FALSE
    bl assert

reserved_handler:
    b reserved_handler

irq_handler:
    /* Interrupted execution's PC. On ARM IRQ's the PC is too far ahead by one word */
    sub lr, lr, IRQ_PC_RUNAHEAD

    /* Spill all registers the AAPCS allows to be corrupted by function calls       */
    stmfd sp!, {r0-r3,ip,lr}

#if ASSERTS_ENABLED
    /* assert(sched_spinlock.lockval == SPINLOCK_LOCKVAL_UNLOCKED); */
    ldr r0, =sched_spinlock
    ldr r1, [r0, Spinlock_t__lockval]
    mov r2, SPINLOCK_LOCKVAL_UNLOCKED
    teq r1, r2
    moveq r0, TRUE
    movne r0, FALSE
    bl assert
#endif

    /* Handle the interrupt */
    bl InterruptHandler

    /*
    Ask whether something happened during the ISR handler that made the
    scheduler need to run again.
    */
    bl ThreadResetNeedResched       /* r0 := boolean result         */

    /* If not, we're done */
    teq r0, FALSE
    ldmnefd sp!, {r0-r3,ip,lr}      /* Undo fast-path register save */
    bne irq_handler__need_switch$

/**
 * Fast-path exit to interrupted task
 */
irq_handler__fast_exit$:
    ldmfd sp!, {r0-r3,ip,lr}    /* Restore corrupted registers      */
    movs pc, lr                 /* CPU mode transition              */
    mov r0, FALSE               /* Should never get here            */
    bl assert

/**
 * The IRQ-specific handler determined that some kind of context
 * switch needs to happen, so we can't use the fast-path return.
 *
 * So, restore the corrupted AAPCS registers, and then do a full
 * shuffle of the interrupted task's registers out to either the
 * PCB or TCB.
 */
irq_handler__need_switch$:
    exception_save_regs_to_stack

    /* Fetch interrupted task's kernel stack pointer */
    mrs r1, cpsr
    cps ARM_PSR_MODE_SVC_BITS
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
    ldr r1, [sp, #(4 * 16)]         /* r1 := spsr                           */
    mvn r3, ARM_PSR_MODE_MASK       /* r3 := ~ARM_PSR_MODE_MASK             */
    bic r1, r1, r3                  /* r1 := spsr & ARM_PSR_MODE_MASK       */

    teq r1, ARM_PSR_MODE_USR_BITS
    beq irq_handler__do_boot_user$

    teq r1, ARM_PSR_MODE_SVC_BITS
    beq irq_handler__do_boot_kernel$

    mov r0, FALSE
    bl assert

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
    push {r0-r3,ip,lr}
    bl ThreadBeginTransactionEndingException
    pop {r0-r3,ip,lr}

    /* Copy the registers from the stack to the PCB */
    exception_store_user_saveregs_from_stack

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
     * We'll also set the kernel task to be resumed with interrupts
     * disabled, as expected by the resume-after-IRQ entrypoint we're
     * setting in its PC.
     *
     * r0: still holds address of the interrupted task's kernel thread
     */

    /* Re-fetch interrupted tasks's SP_svc */
    mrs r1, cpsr
    cps ARM_PSR_MODE_SVC_BITS
    mov r2, sp
    msr cpsr, r1

    str r2, [r0, K_R13]         /* Outgoing task's kernel SP            */
    ldr r2, =swi_handler__restart_after_irq$
    str r2, [r0, K_R15]         /* Outgoing task's kernel PC            */

    /*
    Patch up the process's kernel thread saved CPSR to retain protection
    against preemption when it next hits the CPU from SwitchTo().
    */
    ldr r3, [r0, K_CPSR]
    orr r3, r3, ARM_PSR_I_VALUE
    str r3, [r0, K_CPSR]

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

#if DEBUG_MESSAGES
    push {r0-r3}
    ldr r0, =.__kernel_interrupted_msg

    ldr r1, [sp, #((15 + 4) * 4)]       /* r1 := interrupted PC     */
    ldr r2, [sp, #((13 + 4) * 4)]       /* r2 := interrupted SP     */

    bl printk
    pop {r0-r3}
#endif

#if ASSERTS_ENABLED
    /* assert(sched_spinlock.lockval == SPINLOCK_LOCKVAL_LOCKED); */
    push {r0-r3}
    ldr r0, =sched_spinlock
    ldr r1, [r0, Spinlock_t__lockval]
    mov r2, SPINLOCK_LOCKVAL_UNLOCKED
    teq r1, r2
    moveq r0, TRUE
    movne r0, FALSE
    bl assert
    pop {r0-r3}
#endif

    /* Acquire scheduler lock */
    push {r0-r3}
    bl ThreadBeginTransactionEndingException
    pop {r0-r3}

    /* Copy the registers from the stack to the TCB */
    exception_store_kernel_saveregs_from_stack

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

    ldr r3, [r0, K_CPSR]
    orr r3, r3, ARM_PSR_I_VALUE /* Interrupted task should retain       */
                                /* preemption protection when it's      */
                                /* restored next time.                  */
    str r3, [r0, K_CPSR]        /* Store to kernel thread structure     */

/**
 * Requirements:
 *
 * r0: holds address of outgoing task's kernel 'struct Thread'
 */
irq_handler__do_load_next_task$:

    /**
     * Mark the booted thread as ready to run
     */
    push {r0,r1}
    bl ThreadMakeReady              /* r0 still holds 'struct Thread *' */
    pop {r0,r1}

    /**
     * Fetch next thread to run
     */
    bl ThreadDequeueReady       /* r0 := 'struct Thread *'          */

    /**
     * Enforce that the dequeue call found a thread -- at worst, it
     * should find the one we just booted.
     */
    push {r0,r1}
    teq r0, NULL                /* Is r0 == NULL?                   */
    moveq r0, FALSE             /* If so, r0 := FALSE               */
    movne r0, TRUE              /* If not, r0 := TRUE               */
    bl assert                   /* assert(r0)                       */
    pop {r0,r1}

    /**
     * Copy next task's registers out of kernel-thread register save area
     * and into IRQ stack's staging area. Normal exit path from IRQ handler
     * will restore them to the actual machine registers.
     *
     * Requirements:
     *
     * r0:  Base address of incoming's 'struct Thread'
     * sp:  Bottom of IRQ stack's saved-register area
     */

    /* Load incoming task's registers from TCB to stack */
    exception_load_stack_from_kernel_saveregs

    /**
     * Find out pagetable used by incoming thread
     */
    bl ThreadGetProcess             /* r0 := 'struct Process *'         */
    teq r0, NULL                    /* Is r0 NULL?                      */
    blne ProcessGetTranslationTable /* r0 := 'struct TranslationTable *'*/

    /**
     * Install new pagetable
     */
    bl TranslationTableSetUser      /* Install new pagetable...         */

irq_handler__enforce_lock$:

#if ASSERTS_ENABLED
    /* assert(sched_spinlock.lockval == SPINLOCK_LOCKVAL_LOCKED); */
    ldr r0, =sched_spinlock
    ldr r1, [r0, Spinlock_t__lockval]
    mov r2, SPINLOCK_LOCKVAL_LOCKED
    teq r1, r2
    moveq r0, TRUE
    movne r0, FALSE
    bl assert
#endif

/**
 * Normal unraveling point. Assumes that the only thing on the
 * IRQ stack at this point are the registers of the task to be
 * resumed.
 */
irq_handler__unravel_into_next_task$:

    exception_load_regs_from_stack

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
    push {r0-r5, ip, lr}

#if ASSERTS_ENABLED
    /* assert(sched_spinlock.lockval == SPINLOCK_LOCKVAL_LOCKED); */
    ldr r0, =sched_spinlock
    ldr r1, [r0, Spinlock_t__lockval]
    mov r2, SPINLOCK_LOCKVAL_LOCKED
    teq r1, r2
    moveq r0, TRUE
    movne r0, FALSE
    bl assert
#endif

    /*
    The CPSR at this moment looks (except for the interrupt-enable bits)
    exactly like it should from the interrupted thread's perspective.
    But we're going to be doing some junk to the condition-code flags in
    it when we perform the upcoming spinlock release.

    So we have to save a copy of it. Use r4 so that it survives
    the procedure call.
    */
    mrs r4, cpsr
    bic r4, r4, ARM_PSR_I_VALUE

    bl ThreadEndTransactionFromRestart
    msr spsr, r4
    pop {r0-r5, ip, lr}

    /*
    Finish the shim by consuming the extra word encoding the PC and
    jumping to it.
    */
    ldmfd sp!, {pc}^


/**
 * String data used by printk() calls above.
 */
    .section .rodata
.__kernel_interrupted_msg:
    .ascii "Interrupted Kernel Thread\n"
    .ascii "\tPC was 0x%08x\n"
    .asciz "\tSP was 0x%08x\n"

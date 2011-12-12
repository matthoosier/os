    .include "arm-defs.inc"
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
    /* User PC (stored in the SVC mode LR automatically by the SWI) */
    stmfd sp!, {lr}

    /* Main user registers (caret selects user registers instead of current mode's )*/
    stmfd sp, {r0 - r14}^
    nop
    sub sp, sp, #60

    /* Fetch user-mode CPSR into a register and push it too */
    mrs r8, spsr
    stmfd sp!, {r8}

    /* Re-enable regular interrupts (the SWI instruction disabled them) */
    mrs r8, cpsr
    bic r8, r8, #cpsr_i_bit
    msr cpsr, r8

    /* Do actual work here... */
    add r0, sp, #4
    bl do_syscall

    /* Restore user-mode CPSR from stack (using an intermediate register) */
    ldmfd sp!, {r8}
    msr spsr, r8

    /* Main user registers (caret selects user registers instead of current mode's ) */
    ldmfd sp, {r0 - r14}^
    nop
    add sp, sp, #60

    /* Put user PC into LR in preparation for special exception-return MOVS op */
    ldmfd sp!, {lr}

    /* Atomically transfer current LR into PC, and load SPSR into CPSR */
    movs pc, lr

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
    ldr r1, [sp, #52]       /* r1 := spsr           */
    bic r1, r1, #0xffffffe0 /* r1 := spsr & 0b11111 */

    teq r1, #usr
    beq irq_do_user_preempted$

    teq r1, #svc
    beq irq_do_kernel_preempted$

    b irq_normal_exit$

irq_do_user_preempted$:
    /* Synthesize a system call for current task, load new task's registers */
    b irq_normal_exit$

irq_do_kernel_preempted$:
    /**
     * Copy current task's registers to kernel thread register-save area.
     */

    /* SPSR */
    ldr r3, [sp, #(13 * 4)]     /* Load from staged SPSR on IRQ stack   */
    str r3, [r0, #(16 * 4)]     /* Store to kernel thread structure     */

    /* R15 */
    ldr r3, [sp, #(14 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, #(15 * 4)]     /* Store to kernel thread structure     */

    /* R14 */
    mrs r4, cpsr
    cps #svc
    mov r3, r14
    msr cpsr, r4                /* Load from banked register            */
    str r3, [r0, #(14 * 4)]     /* Store to kernel thread structure     */

    /* R13 */
    mrs r4, cpsr
    cps #svc
    mov r3, r13
    msr cpsr, r4                /* Load from banked register            */
    str r3, [r0, #(13 * 4)]     /* Store to kernel thread structure     */

    /* R12 */
    ldr r3, [sp, #(12 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, #(12 * 4)]     /* Store to kernel thread structure     */

    /* R11 */
    ldr r3, [sp, #(11 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, #(11 * 4)]     /* Store to kernel thread structure     */

    /* R10 */
    ldr r3, [sp, #(10 * 4)]     /* Load from IRQ stack                  */
    str r3, [r0, #(10 * 4)]     /* Store to kernel thread structure     */

    /* R9 */
    ldr r3, [sp, #(9 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(9 * 4)]      /* Store to kernel thread structure     */

    /* R8 */
    ldr r3, [sp, #(8 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(8 * 4)]      /* Store to kernel thread structure     */

    /* R7 */
    ldr r3, [sp, #(7 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(7 * 4)]      /* Store to kernel thread structure     */

    /* R6 */
    ldr r3, [sp, #(6 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(6 * 4)]      /* Store to kernel thread structure     */

    /* R5 */
    ldr r3, [sp, #(5 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(5 * 4)]      /* Store to kernel thread structure     */

    /* R4 */
    ldr r3, [sp, #(4 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(4 * 4)]      /* Store to kernel thread structure     */

    /* R3 */
    ldr r3, [sp, #(3 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(3 * 4)]      /* Store to kernel thread structure     */

    /* R2 */
    ldr r3, [sp, #(2 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(2 * 4)]      /* Store to kernel thread structure     */

    /* R1 */
    ldr r3, [sp, #(1 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(1 * 4)]      /* Store to kernel thread structure     */

    /* R0 */
    ldr r3, [sp, #(0 * 4)]      /* Load from IRQ stack                  */
    str r3, [r0, #(0 * 4)]      /* Store to kernel thread structure     */


    /**
     * Mark the booted thread as ready to run
     */
    push {r0}
    bl ThreadAddReady           /* r0 still holds 'struct Thread *' */
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
     * Copy next task's registers out of kernel save area and into
     * IRQ stack's staging area. Normal exit path from IRQ handler
     * will restore them to the actual machine registers.
     */

    /* SPSR */
    ldr r3, [r0, #(16 * 4)]     /* Load from kernel thread structure    */
    str r3, [sp, #(13 * 4)]     /* Store as staged SPSR on IRQ stack    */

    /* R15 */
    ldr r3, [r0, #(15 * 4)]     /* Load from kernel thread structure    */
    str r3, [sp, #(14 * 4)]     /* Store on IRQ stack                   */

    /* R14 */
    ldr r3, [r0, #(14 * 4)]     /* Load from kernel thread structure    */
    mrs r4, cpsr
    cps #svc
    mov r14, r3                 /* Store in banked register             */
    msr cpsr, r4

    /* R13 */
    ldr r3, [r0, #(13 * 4)]     /* Load from kernel thread structure    */
    mrs r4, cpsr
    cps #svc
    mov r13, r3                 /* Store in banked register             */
    msr cpsr, r4

    /* R12 */
    ldr r3, [r0, #(12 * 4)]     /* Load from kernel thread structure    */
    str r3, [sp, #(12 * 4)]     /* Store on IRQ stack                   */

    /* R11 */
    ldr r3, [r0, #(11 * 4)]     /* Load from kernel thread structure    */
    str r3, [sp, #(11 * 4)]     /* Store on IRQ stack                   */

    /* R10 */
    ldr r3, [r0, #(10 * 4)]     /* Load from kernel thread structure    */
    str r3, [sp, #(10 * 4)]     /* Store on IRQ stack                   */

    /* R9 */
    ldr r3, [r0, #(9 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(9 * 4)]      /* Store on IRQ stack                   */

    /* R8 */
    ldr r3, [r0, #(8 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(8 * 4)]      /* Store on IRQ stack                   */

    /* R7 */
    ldr r3, [r0, #(7 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(7 * 4)]      /* Store on IRQ stack                   */

    /* R6 */
    ldr r3, [r0, #(6 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(6 * 4)]      /* Store on IRQ stack                   */

    /* R5 */
    ldr r3, [r0, #(5 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(5 * 4)]      /* Store on IRQ stack                   */

    /* R4 */
    ldr r3, [r0, #(4 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(4 * 4)]      /* Store on IRQ stack                   */

    /* R3 */
    ldr r3, [r0, #(3 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(3 * 4)]      /* Store on IRQ stack                   */

    /* R2 */
    ldr r3, [r0, #(2 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(2 * 4)]      /* Store on IRQ stack                   */

    /* R1 */
    ldr r3, [r0, #(1 * 4)]      /* Load from kernel thread structure    */
    str r3, [sp, #(1 * 4)]      /* Store on IRQ stack                   */

    /* R0 */
    ldr r3, [r0, #(0 * 4)]      /* Load from kernel thread structure    */
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

    beq irq_normal_exit$            /* If so, we're done.               */

    mov r0, r9                      /* r0 := address of incoming        */
                                    /* thread's pagetable               */

    bl MmuSetUserTranslationTable   /* Install new pagetable...         */
    bl MmuFlushTlb                  /* ... and flush TLB                */
    b irq_normal_exit$              /* Done.                            */

irq_normal_exit$:

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


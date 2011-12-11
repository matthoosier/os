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

    /* Deduce kernel thread structure location from the task's stack pointer */
    bl ThreadStructFromStackPointer

    /* Figure out execution mode processor was in before IRQ arrived        */
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
    /* Kernel isn't preemptible for now */
    b irq_normal_exit$

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


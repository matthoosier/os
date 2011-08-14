    .include "arm-defs.inc"
    .section "vectors"

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

    .text
    .align 4
    .global _start

_start:
    /*
    ARM stacks grow down, so set the initial stack pointer to the "high"
    end of the chunk of memory designated for our initialization stack.
    */
    cps #svc
    ldr sp, =init_stack_ceiling

    bl init

0:
    b 0b

reset_handler:
    b reset_handler

undef_handler:
    b undef_handler

swi_handler:
    b swi_handler

pabt_handler:
    b pabt_handler

dabt_handler:
    b dabt_handler

reserved_handler:
    b reserved_handler

irq_handler:
    b irq_handler

fiq_handler:
    b fiq_handler

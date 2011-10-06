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
    /* Echo the arg0 back as result */
    mov r0, r1
    movs pc, lr

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

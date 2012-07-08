    .section .text.vector

/**
 * This symbol must be located at virtual memory address 0xffff0000
 * exactly in order to satisfy the ARM architecture's expectations.
 *
 * An MMU mapping is used to make it appear there (in addition to
 * wherever in the normal virtual memory 'text' section range that
 * it naturally appears as part of the kernel executable).
 */
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

    .section "vectors"

/* Processor reset */
reset:
    b reset_handler

/* Illegal instruction */
undef: 
    b undef_handler

/* Software interrupt */
swi:
    b swi_handler

/* Instruction stream prefetch abort */
pabt:
    b pabt_handler

/* Data abort */
dabt:
    b dabt_handler

/* Reserved for future use */
reserved:
    b reserved_handler

/* General-purpose interrupt */
irq:
    b irq_handler

/* Fast interrupt */
fiq:
    b fiq_handler

    .text
    .align 4
    .global _start

_start:
    /*
    ARM stacks grow down, so set the initial stack pointer to the "high"
    end of the chunk of memory designated for our initialization stack.
    */
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

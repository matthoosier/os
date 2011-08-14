    .include "arm-defs.inc"
    .align 4
    .global _start_high

_start_high:
    /*
    ARM stacks grow down, so set the initial stack pointer to the "high"
    end of the chunk of memory designated for our initialization stack.
    */
    cps #svc
    ldr r0, =init_stack_ceiling
    ldr sp, [r0, #0]

    bl init

0:
    b 0b

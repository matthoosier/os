    .include "arm-defs.inc"
    .align 4
    .global _start_high

_start_high:
    /*
    Continue using the same stack, but adjust the stack pointer to refer
    to it from high-memory addresses.
    */
    cps #svc
    ldr r0, =__KernelStart
    add sp, sp, r0

    bl Init

0:
    b 0b

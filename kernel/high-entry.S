#include <asm-offsets.h>

    .align 4
    .global _start_high

_start_high:
    /*
    Continue using the same stack, but adjust the stack pointer to refer
    to it from high-memory addresses.
    */
    cps ARM_PSR_MODE_SVC_BITS
    ldr r0, =__KernelStart
    add sp, sp, r0

    bl Init

0:
    b 0b

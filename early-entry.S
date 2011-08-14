    .include "arm-defs.inc"

    .align 4
    .global _start

_start:
    ldr r0, =realmode_stack_ceiling
    ldr sp, [r0, #0]

    /*
    This function installs a memory map that leaves both the early init
    code (in low addresses) and the regular kernel code (in high addresses)
    valid.

    We'll use that configuration as a crutch until we're in full kernel
    mode and can just unmap the low addresses.
    */
    bl early_setup_dual_memory_map

    /*
    Only do this jump once a memory map is installed that maps
    the kernel code up by +3GB

    Note that this call must not return. Setting up regular protected
    mode will result in the VM unmapping this early init code, so there
    would be no valid instruction at the next address anyway.

    We use a BX instruction here because the branch is 3GB--far beyond
    the maximum allowed PC-relative target for label encoded directly
    in the 'B' opcode.
    */
    ldr r0, =_start_high
    bx r0

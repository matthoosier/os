#include "arm-defs.inc"

    .align 4
    .global _start

_start:
    /*
    ARM stacks grow down, so set the initial stack pointer to the "high"
    end of the chunk of memory designated for our initialization stack.

    There are some gymnastics here, because the C data structure providing
    the storage of the stack is linked expecting to be accessed from
    "high memory" kernel addresses (>= 0xc0000000).

    So we have to apply corrections both to the address of the pointer
    variable (uint8_t * init_stack_ceiling) that records the address of
    the top end of the stack, and of the payload of that memory too.

    r0: VMA of main kernel
    r1: low-memory evaluation of &init_stack_ceiling
    sp: low-memory evaluation of *(&init_stack_ceiling)
    */
    ldr r0, =__KernelStart
    ldr r1, =init_stack_ceiling
    sub r1, r1, r0
    ldr sp, [r1, #0]
    sub sp, sp, r0

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

    .section .text.early.vector

early_vector:
    /* Processor reset. Serviced in svc mode. */
    b early_reset_handler

    /* Undefined instruction. Serviced in und mode. */
    b early_undef_handler

    /* Software interrupt. Serviced in svc mode. */
    b early_swi_handler

    /* Instruction fetch memory abort. Serviced in abt mode. */
    b early_pabt_handler

    /* Data access memory abort. Serviced in abt mode. */
    b early_dabt_handler

    /* Reserved for future use. */
    b early_reserved_handler

    /* General-purpose interrupt. Serviced in irq mode. */
    b early_irq_handler

    /* Fast interrupt. Serviced in fiq mode. */
    b early_fiq_handler

early_reset_handler:
    b early_reset_handler

early_undef_handler:
    b early_undef_handler

early_swi_handler:
    b early_swi_handler

early_pabt_handler:
    b early_pabt_handler

early_dabt_handler:
    b early_dabt_handler

early_reserved_handler:
    b early_reserved_handler

early_irq_handler:
    b early_irq_handler

early_fiq_handler:
    b early_fiq_handler


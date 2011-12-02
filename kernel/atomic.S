    .arch armv6
    .text
    .arm
    .align 4

    .global __sync_bool_compare_and_swap_4

/**
 * bool __sync_compare_and_swap_4(
 *         uint32_t * ptr,
 *         uint32_t oldval,
 *         uint32_t newval
 *         );
 * Inputs:
 *   r0: Pointer to 4-byte word
 *   r1: 4-byte oldval
 *   r2: 4-byte newval
 *
 * Outputs:
 *   r0: 0 if failed, nonzero if succeeded
 *
 */
__sync_bool_compare_and_swap_4:
    /**
     * Local variables:
     *   v1: old
     *   v2: result
     */
    stmfd sp!, {v1, v2}
    ldrex v1, [r0]
    mov v2, #1
    teq v1, r1
    strexeq v2, r2, [r0]
    teq v2, #0
    moveq r0, #1
    movne r0, #0
    ldmfd sp!, {v1, v2}
    bx lr

    .arch armv6
    .text
    .arm
    .align 4

    .global __sync_bool_compare_and_swap_4
    .global __sync_add_and_fetch_4
    .global __sync_sub_and_fetch_4

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
     *   r3: old
     *   r4: result
     */
    stmfd sp!, {r3, r4}
    ldrex r3, [r0]
    mov r4, #1
    teq r3, r1
    strexeq r4, r2, [r0]
    teq r4, #0
    moveq r0, #1
    movne r0, #0
    ldmfd sp!, {r3, r4}
    bx lr

/**
 * bool __sync_sub_and_fetch_4(
 *         uint32_t * ptr,
 *         uint32_t   amount
 *         );
 *
 * Inputs:
 *     r0: Pointer to 4-byte word
 *     r1: Amount to subtract
 *
 * Outputs:
 *     r0: Result of subtraction
 */
__sync_sub_and_fetch_4:
    /**
     * Local variables:
     *  r2: original value
     *  r3: post-subtraction value
     *  r4: result code of atomic store attempt
     */
    stmfd sp!, {r2, r3, r4}

0:
    ldrex r2, [r0]          /* r2 := *ptr                   */
    sub r3, r2, r1          /* r3 := *ptr - amount          */
    strex r4, r3, [r0]      /* r4 := 0 if exchange worked   */

    teq r4, #0              /* Did exchange work?           */
    bne 0b                  /* If not, try again            */

    mov r0, r3              /* r0 := return value           */

    ldmfd sp!, {r2, r3, r4}
    bx lr

/**
 * bool __sync_add_and_fetch_4(
 *         uint32_t * ptr,
 *         uint32_t   amount
 *         );
 */
__sync_add_and_fetch_4:
    neg r1, r1              /* amount := -amount            */
    b __sync_sub_and_fetch_4

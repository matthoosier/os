/*
 * From http://www.bearcave.com/software/divide.htm
 */
static void unsigned_divide (
        unsigned int dividend,
        unsigned int divisor,
        unsigned int *quotient,
        unsigned int *remainder
        )
{
    unsigned int t;
    unsigned int num_bits;
    unsigned int q;
    unsigned int bit;
    unsigned int d;
    int i;

    *remainder = 0;
    *quotient = 0;

    if (divisor == 0) {
        return;
    }

    if (divisor > dividend) {
        *remainder = dividend;
        return;
    }

    if (divisor == dividend) {
        *quotient = 1;
        return;
    }

    num_bits = 32;

    while (*remainder < divisor) {
        bit = (dividend & 0x80000000) >> 31;
        *remainder = (*remainder << 1) | bit;
        d = dividend;
        dividend = dividend << 1;
        num_bits--;
    }

    /* The loop, above, always goes one iteration too far.
       To avoid inserting an "if" statement inside the loop
       the last iteration is simply reversed. */
    dividend = d;
    *remainder = *remainder >> 1;
    num_bits++;

    for (i = 0; i < num_bits; i++) {
        bit = (dividend & 0x80000000) >> 31;
        *remainder = (*remainder << 1) | bit;
        t = *remainder - divisor;
        q = !((t & 0x80000000) >> 31);
        dividend = dividend << 1;
        *quotient = (*quotient << 1) | q;
        if (q) {
            *remainder = t;
        }
    }
}

static inline unsigned int ABS (int x)
{
    return x < 0 ? -x : x;
}

/*
 * From http://www.bearcave.com/software/divide.htm
 */
void signed_divide (
        int dividend,
        int divisor,
        int *quotient,
        int *remainder
        )
{
    unsigned int dend;
    unsigned int dor;
    unsigned int q;
    unsigned int r;

    dend = ABS(dividend);
    dor = ABS(divisor);

    unsigned_divide(dend, dor, &q, &r);

    /* the sign of the remainder is the same as the sign of the dividend
       and the quotient is negated if the signs of the operands are
       opposite */
    *quotient = q;
    if (dividend < 0) {
        *remainder = -r;
        if (divisor > 0) {
            *quotient = -q;
        }
    }
    else {
        *remainder = r;
        if (divisor < 0) {
            *quotient = -q;
        }
    }
}

#if defined(__ARM_EABI__) && (__ARM_EABI__ != 0)
    unsigned __aeabi_uidiv(
            unsigned numerator,
            unsigned denominator
            )
    {
        unsigned int quotient;
        unsigned int remainder;

        unsigned_divide(numerator, denominator, &quotient, &remainder);

        return quotient;
    }

    typedef struct { unsigned quot; unsigned rem; } uidiv_return;

    __attribute__((naked))
    void /* __value_in_regs uidiv_return */
    __aeabi_uidivmod (
            unsigned numerator,
            unsigned denominator
            )
    {
        asm volatile(
            "stmfd sp!, {lr}    \n\t"
            "sub sp, sp, #8     \n\t" /* make space for uidiv_return */
            "add r2, sp, #0     \n\t" /* r2 := &quotient    */
            "add r3, sp, #4     \n\t" /* r3 := &remainder   */
            "bl unsigned_divide \n\t"
            "ldr r0, [sp, #0]   \n\t" /* uidiv_return.quot  */
            "ldr r1, [sp, #4]   \n\t" /* uidiv_return.rem   */
            "add sp, sp, #8     \n\t" /* reclaim space of uidiv_return */
            "ldmfd sp!, {lr}    \n\t"
            "bx lr              \n\t"
        );
    }
#endif

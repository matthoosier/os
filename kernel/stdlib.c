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

#include <string.h>

void * memset (void *b, int c, size_t len)
{
    char * ptr;
    char * bound = ((char *)b) + len;

    for (ptr = (char *)b; ptr < bound; ptr++) {
        *ptr = (char)c;
    }

    return b;
}

void *
memcpy (void *__restrict s1, const void *__restrict s2, size_t n)
{
    char * src_ptr = (char *)s2;
    char * dst_ptr = (char *)s1;
    char * src_bound = ((char *)s2) + n;

    for (; src_ptr < src_bound; src_ptr++, dst_ptr++) {
        *dst_ptr = *src_ptr;
    }

    return s1;
}

int strcmp (const char *s1, const char *s2)
{
    for (;*s1 != '\0' && *s2 != '\0'; s1++, s2++) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
    }

    if (*s1 == '\0' && *s2 == '\0') {
        /* Both strings same length. */
        return 0;
    }
    else if (*s1 == '\0') {
        /* s1 is shorter, and therefore "less" */
        return -*s2;
    }
    else {
        /* s1 is longer, and therefore "greater" */
        return +*s1;
    }
}

char * strcpy (char *s1, const char *s2)
{
    char *orig_s1;

    for (orig_s1 = s1; *s2 != '\0'; s1++, s2++) {
        *s1 = *s2;
    }

    *s1 = '\0';
    return orig_s1;
}

char * strncpy (char *s1, const char *s2, size_t n)
{
    char *orig_s1 = s1;
    char *last_s1 = s1 + n;

    for (; (s1 <= last_s1) && (*s2 != '\0'); s1++, s2++) {
        *s1 = *s2;
    }

    for (;s1 <= last_s1; s1++) {
        *s1 = '\0';
    }

    return orig_s1;
}

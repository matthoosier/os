#include "arch.h"
#include "bits.h"

#include <stdint.h>

int arch_get_version (void)
{
    uint32_t cp15_r1;

    /*
    p15: coprocessor number
    0: opcode 1; generally 0b000
    %0: destination ARM register
    c0: primary coprocessor register name
    c0: further specification of the coprocessor register name.
        "c0" is a placeholder here and is ignored for cp15 c1
    */
    asm volatile(
        "mrc p15, 0, %0, c0, c0"
        : "=r"(cp15_r1)
        :
        :
    );

    if (!testbit(cp15_r1, 19)) {
        uint32_t bits_12_15 = (cp15_r1 >> 12) & 0b1111;

        if (bits_12_15 == 0x0) {
            return 3;
        }
        else if (bits_12_15 == 0x7) {
            return 4;
        }
        else if (bits_12_15 > 0x7) {
            uint32_t bits_16_19 = (cp15_r1 >> 16) & 0b1111;

            switch (bits_16_19) {
                case 0x1:
                    /* v4 */
                    return 4;

                case 0x2:
                    /* v4t */
                    return 4;

                case 0x3:
                    /* v5 */
                    return 5;

                case 0x4:
                    /* v5t */
                    return 5;

                case 0x5:
                    /* v5te */
                    return 5;

                case 0x6:
                    /* v5tej */
                    return 5;

                case 0x7:
                    /* v6 */
                    return 6;

                case 0xf:
                    /* ARM-extension */
                    return -1;

                default:
                    /* Shouldn't get here */
                    return -1;
            }
        }
        else {
            /* Shouldn't reach here. */
            return -1;
        }
    }
    else {
        /* Shouldn't reach here. */
        return -1;
    }
}

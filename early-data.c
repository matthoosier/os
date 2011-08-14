#include <stdint.h>

#include "array.h"

/* Do NOT put in BSS */
uint8_t realmode_stack[1024 * 2]
    __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)))
    = { 0 };

uint8_t * realmode_stack_ceiling = &realmode_stack[N_ELEMENTS(realmode_stack)];

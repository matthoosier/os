#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

#include <stdbool.h>
#include <stdint.h>

#include <sys/arch.h>
#include <sys/bits.h>
#include <sys/compiler.h>
#include <sys/decls.h>

BEGIN_DECLS

/**
 * Stores interrupt enable/disable state of a CPU core.
 */
typedef struct
{
    int cpsr_interrupt_flags;
} IrqSave_t;

/* Should be passable by-value as a simple word */
COMPILER_ASSERT(sizeof(IrqSave_t) <= sizeof(int));

__attribute__ ((optimize(2)))
static inline bool InterruptsDisabled ()
{
    uint32_t cpsr;
    asm volatile (
        "mrs %[cpsr], cpsr"
        : [cpsr] "=r" (cpsr)
        :
        : "memory"
    );

    return (cpsr & SETBIT(ARM_PSR_I_BIT)) != 0;
}

__attribute__((optimize(2)))
static inline IrqSave_t InterruptsDisable ()
{
    uint32_t    cpsr;
    IrqSave_t   prev_state;

    asm volatile (
        "mrs %[cpsr], cpsr              \n\t"
        "mov %[prev_cpsr], %[cpsr]      \n\t"
        "orr %[cpsr], %[cpsr], %[bits]  \n\t"
        "msr cpsr, %[cpsr]              \n\t"
        : [cpsr] "=r" (cpsr)
        , [prev_cpsr] "=r" (prev_state.cpsr_interrupt_flags)
        : [bits] "i" (SETBIT(ARM_PSR_I_BIT))
        : "cc"
    );

    prev_state.cpsr_interrupt_flags &= SETBIT(ARM_PSR_I_BIT);

    return prev_state;
}

__attribute__((optimize(2)))
static inline IrqSave_t InterruptsEnabledState ()
{
    IrqSave_t state;
    state.cpsr_interrupt_flags = 0;
    return state;
}

__attribute__((optimize(2)))
static inline IrqSave_t InterruptsEnable ()
{
    uint32_t cpsr;
    IrqSave_t prev_state;

    asm volatile (
        "mrs %[cpsr], cpsr              \n\t"
        "mov %[prev_cpsr], %[cpsr]      \n\t"
        "bic %[cpsr], %[cpsr], %[bits]  \n\t"
        "msr cpsr, %[cpsr]              \n\t"
        : [cpsr] "=r" (cpsr)
        , [prev_cpsr] "=r" (prev_state.cpsr_interrupt_flags)
        : [bits] "i" SETBIT(ARM_PSR_I_BIT)
        : "cc"
    );

    prev_state.cpsr_interrupt_flags &= SETBIT(ARM_PSR_I_BIT);

    return prev_state;
}

__attribute__((optimize(2)))
static inline void InterruptsRestore (IrqSave_t saved_state)
{
    uint32_t    cpsr;

    asm volatile (
        "mrs %[cpsr], cpsr"
        : [cpsr] "=r" (cpsr)
    );

    cpsr &= ~SETBIT(ARM_PSR_I_BIT);
    cpsr |= saved_state.cpsr_interrupt_flags;

    asm volatile (
        "msr cpsr, %[cpsr]"
        :
        : [cpsr] "r" (cpsr)
        : "cc"
    );
}

END_DECLS

#endif /* __INTERRUPTS_H__ */

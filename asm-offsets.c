#include <stddef.h>
#include <stdbool.h>

#include <kernel/thread.hpp>

#define DEFINE(_id, _val)               \
    asm volatile (                      \
        "\n#define " #_id " %0\n" \
        :                               \
        : "i" (_val)                    \
    )

#define OFFSET(_type, _member)          \
    DEFINE(_type ## __ ## _member, offsetof(_type, _member))

__attribute__((used))
void AsmOffsetsMain ()
{
    OFFSET(Spinlock_t, lockval);
    OFFSET(Spinlock_t, irq_saved_state);
    OFFSET(IrqSave_t, cpsr_interrupt_flags);
    OFFSET(Thread, k_reg);

    DEFINE(K_R0,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  0]));
    DEFINE(K_R1,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  1]));
    DEFINE(K_R2,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  2]));
    DEFINE(K_R3,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  3]));
    DEFINE(K_R4,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  4]));
    DEFINE(K_R5,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  5]));
    DEFINE(K_R6,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  6]));
    DEFINE(K_R7,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  7]));
    DEFINE(K_R8,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  8]));
    DEFINE(K_R9,   offsetof(Thread, k_reg[REGISTER_INDEX_R0 +  9]));
    DEFINE(K_R10,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 10]));
    DEFINE(K_R11,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 11]));
    DEFINE(K_R12,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 12]));
    DEFINE(K_R13,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 13]));
    DEFINE(K_R14,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 14]));
    DEFINE(K_R15,  offsetof(Thread, k_reg[REGISTER_INDEX_R0 + 15]));
    DEFINE(K_CPSR, offsetof(Thread, k_reg[REGISTER_INDEX_PSR]));

    DEFINE(U_R0,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  0]));
    DEFINE(U_R1,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  1]));
    DEFINE(U_R2,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  2]));
    DEFINE(U_R3,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  3]));
    DEFINE(U_R4,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  4]));
    DEFINE(U_R5,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  5]));
    DEFINE(U_R6,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  6]));
    DEFINE(U_R7,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  7]));
    DEFINE(U_R8,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  8]));
    DEFINE(U_R9,   offsetof(Thread, u_reg[REGISTER_INDEX_R0 +  9]));
    DEFINE(U_R10,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 10]));
    DEFINE(U_R11,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 11]));
    DEFINE(U_R12,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 12]));
    DEFINE(U_R13,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 13]));
    DEFINE(U_R14,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 14]));
    DEFINE(U_R15,  offsetof(Thread, u_reg[REGISTER_INDEX_R0 + 15]));
    DEFINE(U_CPSR, offsetof(Thread, u_reg[REGISTER_INDEX_PSR]));

    DEFINE(SPINLOCK_LOCKVAL_LOCKED, SPINLOCK_LOCKVAL_LOCKED);
    DEFINE(SPINLOCK_LOCKVAL_UNLOCKED, SPINLOCK_LOCKVAL_UNLOCKED);

    DEFINE(ARM_PSR_I_BIT, ARM_PSR_I_BIT);
    DEFINE(ARM_PSR_I_VALUE, ARM_PSR_I_VALUE);
    DEFINE(ARM_PSR_MODE_MASK, ARM_PSR_MODE_MASK);
    DEFINE(ARM_PSR_MODE_SVC_BITS, ARM_PSR_MODE_SVC_BITS);
    DEFINE(ARM_PSR_MODE_USR_BITS, ARM_PSR_MODE_USR_BITS);
    DEFINE(ARM_PSR_MODE_IRQ_BITS, ARM_PSR_MODE_IRQ_BITS);

    DEFINE(FALSE, false);
    DEFINE(TRUE, true);
    DEFINE(NULL, NULL);
}

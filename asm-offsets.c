#include <stddef.h>

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
    OFFSET(Thread, registers);

    DEFINE(K_R0,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  0]));
    DEFINE(K_R1,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  1]));
    DEFINE(K_R2,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  2]));
    DEFINE(K_R3,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  3]));
    DEFINE(K_R4,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  4]));
    DEFINE(K_R5,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  5]));
    DEFINE(K_R6,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  6]));
    DEFINE(K_R7,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  7]));
    DEFINE(K_R8,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  8]));
    DEFINE(K_R9,   offsetof(Thread, registers[REGISTER_INDEX_R0 +  9]));
    DEFINE(K_R10,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 10]));
    DEFINE(K_R11,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 11]));
    DEFINE(K_R12,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 12]));
    DEFINE(K_R13,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 13]));
    DEFINE(K_R14,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 14]));
    DEFINE(K_R15,  offsetof(Thread, registers[REGISTER_INDEX_R0 + 15]));
    DEFINE(K_CPSR, offsetof(Thread, registers[REGISTER_INDEX_PSR]));

    DEFINE(SPINLOCK_LOCKVAL_LOCKED, SPINLOCK_LOCKVAL_LOCKED);
    DEFINE(SPINLOCK_LOCKVAL_UNLOCKED, SPINLOCK_LOCKVAL_UNLOCKED);
}

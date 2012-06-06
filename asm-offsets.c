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

    DEFINE(SPINLOCK_LOCKVAL_LOCKED, SPINLOCK_LOCKVAL_LOCKED);
    DEFINE(SPINLOCK_LOCKVAL_UNLOCKED, SPINLOCK_LOCKVAL_UNLOCKED);
}

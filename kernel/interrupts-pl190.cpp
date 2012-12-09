#include <stdint.h>

#include <kernel/interrupts.hpp>
#include <kernel/mmu.hpp>

/**
 * \brief   Device-specific ARM PL190 implementation of abstract
 *          interrupt-controller driver model.
 */
class Pl190 : public InterruptController
{
public:
    Pl190();

    virtual void Init ();
    virtual void MaskIrq (int n);
    virtual void UnmaskIrq (int n);
    virtual unsigned int GetNumSupportedIrqs ();
    virtual int GetRaisedIrqNum ();

private:
    volatile uint32_t * IrqStatus;
    volatile uint32_t * IntEnable;
    volatile uint32_t * IntEnClear;
};

Pl190::Pl190 ()
{
    // Hook into modular core
    Interrupts::RegisterController(this);
}

void Pl190::Init ()
{
    enum
    {
        PL190_BASE_PHYS = 0x10140000,
        PL190_BASE_VIRT = 0xfff10000,
    };

    bool mapped = TranslationTable::GetKernel()->MapPage(
            PL190_BASE_VIRT,
            PL190_BASE_PHYS,
            PROT_KERNEL
            );
    assert(mapped);

    uint8_t * base = (uint8_t *)PL190_BASE_VIRT;

    this->IrqStatus     = (uint32_t *)(base + 0x000);
    this->IntEnable     = (uint32_t *)(base + 0x010);
    this->IntEnClear    = (uint32_t *)(base + 0x014);
}

void Pl190::MaskIrq (int n)
{
    *this->IntEnClear = 1u << n;
}

void Pl190::UnmaskIrq (int n)
{
    *this->IntEnable = 1u << n;
}

unsigned int Pl190::GetNumSupportedIrqs ()
{
    return 32;
}

int Pl190::GetRaisedIrqNum ()
{
    uint32_t irqs = *this->IrqStatus;

    int which = __builtin_ffs(irqs) - 1;
    assert(which >= 0);

    return which;
}

// Constructor execution will register this driver instance with the core
static Pl190 instance;

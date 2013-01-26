#ifndef __INTERRUPTS_HPP__
#define __INTERRUPTS_HPP__

#include <kernel/list.hpp>

/**
 * \brief   Base class that a specific interrupt controller implementation should
 *          subclass.
 *
 * Concrete implementations should be hooked into the system by declaring
 * a single global-variable instance of the implementation class, and the constructor
 * of this class should make a call to Interrupts#RegisterController().
 *
 * \code
 * class SomeInterruptController : public InterruptController
 * {
 * public:
 *     SomeInterruptController () {
 *         Interrupts::RegisterController(this);
 *     }
 *
 *     virtual void Init () {
 *         // Establish MMU mappings for memory-mapped IO register access...
 *
 *         // Device-specific register initialization...
 *     }
 * };
 *
 * // Constructor's execution will hook this driver into the interrupt handling
 * // core.
 * SomeInterruptController instance;
 * \endcode
 *
 * \class InterruptController interrupts.hpp kernel/interrupts.hpp
 */
class InterruptController
{
public:
    virtual ~InterruptController () {};

    virtual void Init () = 0;
    virtual void MaskIrq (int n) = 0;
    virtual void UnmaskIrq (int n) = 0;
    virtual unsigned int GetNumSupportedIrqs () = 0;
    virtual int GetRaisedIrqNum () = 0;

public:
    ListElement mLink;
};

/**
 * \brief   Core API for implementing device-independent interrupt
 *          controller logic.
 *
 * \class Interrupts interrupts.hpp kernel/interrupts.hpp
 */
class Interrupts
{
public:
    /**
     * \brief   Register an interrupt controller driver implementation
     *          into the handler core.
     */
    static void RegisterController (InterruptController * controller);
};

#endif /* __INTERRUPTS_HPP__ */

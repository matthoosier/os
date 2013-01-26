#ifndef __INTERRUPT_HANDLER_H__
#define __INTERRUPT_HANDLER_H__

#include <new>
#include <stdbool.h>

#include <muos/decls.h>

#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/message.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>

BEGIN_DECLS

/**
 * Signature of an IRQ handler function.
 */
typedef void (*IrqKernelHandlerFunc) ();

/**
 * \brief   State information about what connection an async message
 *          should be sent on when a particular IRQ is raised.
 *
 * \class UserInterruptHandler interrupt-handler.hpp kernel/interrupt-handler.hpp
 */
class UserInterruptHandler : public RefCounted
{
public:
    class HandlerInfo
    {
    public:
        int                 mIrqNumber;
        RefPtr<Connection>  mConnection;
        uintptr_t           mPulsePayload;
    };

    class StateInfo
    {
    public:
        bool mMasked;
    };

public:
    UserInterruptHandler ();

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(UserInterruptHandler));
        void * ret = sSlab.AllocateWithThrow();
        return ret;
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    void Dispose ();

protected:
    virtual ~UserInterruptHandler ();

public:
    HandlerInfo mHandlerInfo;

    StateInfo mStateInfo;

    ListElement mLink;

private:
    static SyncSlabAllocator<UserInterruptHandler> sSlab;

    bool mDisposed;

    /**
     * Only RefPtr is allowed to deallocate instances
     */
    friend class RefPtr<UserInterruptHandler>;
};

/**
 * Set up the stacks used for interrupt handling
 */
void InterruptsConfigure();

void InterruptAttachKernelHandler (unsigned int irq_number, IrqKernelHandlerFunc f);

void InterruptAttachUserHandler (
        RefPtr<UserInterruptHandler> handler
        );

int InterruptCompleteUserHandler (
        RefPtr<UserInterruptHandler> handler
        );

void InterruptDetachUserHandler (
        RefPtr<UserInterruptHandler> handler
        );

void InterruptHandler ();

void InterruptUnmaskIrq (int n);

void InterruptMaskIrq (int n);

END_DECLS

#endif /* __INTERRUPT_HANDLER_H__ */

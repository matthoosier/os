#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/procmgr.hpp>

static ProcMgrOperationFunc handler_funcs[PROC_MGR_MESSAGE_COUNT] = { NULL };

void ProcMgrRegisterMessageHandler (
        enum ProcMgrMessageType type,
        ProcMgrOperationFunc func
        )
{
    assert(handler_funcs[type] == NULL);

    handler_funcs[type] = func;
}

ProcMgrOperationFunc ProcMgrGetMessageHandler (enum ProcMgrMessageType type)
{
    if (type >= 0 && type < N_ELEMENTS(handler_funcs)) {
        return handler_funcs[type];
    }
    else {
        return NULL;
    }
}

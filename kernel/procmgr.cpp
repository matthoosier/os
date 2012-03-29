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
    return handler_funcs[type];
}

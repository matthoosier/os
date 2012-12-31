#include <kernel/kmalloc.h>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>

static void HandleSpawn (RefPtr<Message> message)
{
    size_t path_len;
    char * path;
    ProcMgrReply reply;
    int ret;
    size_t n;
    Process * p;

    message->Read(offsetof(struct ProcMgrMessage, payload.spawn.path_len),
                  &path_len,
                  sizeof(path_len));

    path = (char *)kmalloc(path_len);

    if (!path) {
        ret = ERROR_NO_MEM;
        goto cleanup;
    }

    n = message->Read(offsetof(struct ProcMgrMessage, payload.spawn.path),
                      path,
                      path_len);

    if (n < path_len) {
        ret = ERROR_INVALID;
        goto cleanup;
    }

    p = Process::Create(path);

    if (!p) {
        ret = ERROR_INVALID;
        goto cleanup;
    }

    reply.payload.spawn.pid = p->GetId();
    ret = ERROR_OK;

cleanup:

    if (path) {
        kfree(path, path_len);
    }

    message->Reply(ret, &reply, sizeof(reply));
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_SPAWN, HandleSpawn);

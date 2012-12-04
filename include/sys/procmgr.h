#ifndef __SYS_PROCMGR_H__
#define __SYS_PROCMGR_H__

#include <stdint.h>

#include <sys/decls.h>
#include <sys/io.h>
#include <sys/message.h>

#define PROCMGR_CONNECTION_ID FIRST_CONNECTION_ID

BEGIN_DECLS

#define PROC_MGR_MSG_LEN(payload_name)                              \
    (                                                               \
    offsetof(struct ProcMgrMessage, payload.payload_name)           \
    + sizeof(((struct ProcMgrMessage *)NULL)->payload.payload_name) \
    )

typedef enum
{
    PROC_MGR_MESSAGE_EXIT = 0,
    PROC_MGR_MESSAGE_SIGNAL,
    PROC_MGR_MESSAGE_GETPID,
    PROC_MGR_MESSAGE_SPAWN,
    PROC_MGR_MESSAGE_INTERRUPT_ATTACH,
    PROC_MGR_MESSAGE_INTERRUPT_DETACH,
    PROC_MGR_MESSAGE_INTERRUPT_COMPLETE,
    PROC_MGR_MESSAGE_MAP_PHYS,

    /**
     * Not a message. Just a count.
     */
    PROC_MGR_MESSAGE_COUNT,
} ProcMgrMessageType;

struct ProcMgrMessage
{
    ProcMgrMessageType type;

    union {

        struct {
        } dummy;

        struct {
        } exit;

        struct {
            int signalee_pid;
        } signal;

        struct {
        } getpid;

        struct {
            size_t path_len;
            char path[0];
        } spawn;

        struct {
            int connection_id;
            int irq_number;
            void * param;
        } interrupt_attach;

        struct {
            InterruptHandler_t handler;
        } interrupt_detach;

        struct {
            InterruptHandler_t handler;
        } interrupt_complete;

        struct {
            uintptr_t physaddr;
            size_t len;
        } map_phys;

    } payload;
};

struct ProcMgrReply
{
    union {

        struct {
        } dummy;

        struct {
        } exit;

        struct {
        } signal;

        struct {
            int pid;
        } getpid;

        struct {
            int pid;
        } spawn;

        struct {
            InterruptHandler_t handler;
        } interrupt_attach;

        struct {
        } interrupt_detach;

        struct {
            uintptr_t vmaddr;
        } map_phys;

    } payload;
};

END_DECLS

#endif /* __SYS_PROCMGR_H__ */

#ifndef __MUOS_PROCMGR_H__
#define __MUOS_PROCMGR_H__

#include <stdint.h>

#include <muos/decls.h>
#include <muos/message.h>

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
    PROC_MGR_MESSAGE_NAME_ATTACH,
    PROC_MGR_MESSAGE_NAME_OPEN,
    PROC_MGR_MESSAGE_CHILD_WAIT_ATTACH,
    PROC_MGR_MESSAGE_CHILD_WAIT_DETACH,
    PROC_MGR_MESSAGE_CHILD_WAIT_ARM,
    PROC_MGR_MESSAGE_SBRK,

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
            int handler_id;
        } interrupt_detach;

        struct {
            int handler_id;
        } interrupt_complete;

        struct {
            uintptr_t physaddr;
            size_t len;
        } map_phys;

        struct {
            size_t path_len;
            char path[0];
        } name_attach;

        struct {
            size_t path_len;
            char path[0];
        } name_open;

        struct {
            int connection_id;
            int child_pid;
        } child_wait_attach;

        struct {
            int handler_id;
        } child_wait_detach;

        struct {
            int handler_id;
            unsigned int count;
        } child_wait_arm;

        struct {
            intptr_t increment;
        } sbrk;

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
            int handler_id;
        } interrupt_attach;

        struct {
        } interrupt_detach;

        struct {
            uintptr_t vmaddr;
        } map_phys;

        struct {
            int channel_id;
        } name_attach;

        struct {
            int connection_id;
        } name_open;

        struct {
            int handler_id;
        } child_wait_attach;

        struct {
        } child_wait_detach;

        struct {
        } child_wait_arm;

        struct {
            intptr_t previous;
        } sbrk;

    } payload;
};

END_DECLS

#endif /* __MUOS_PROCMGR_H__ */

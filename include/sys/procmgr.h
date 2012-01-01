#ifndef __SYS_PROCMGR_H__
#define __SYS_PROCMGR_H__

#include <stdint.h>

#include <sys/decls.h>
#include <sys/io.h>
#include <sys/message.h>

#define PROCMGR_CONNECTION_ID FIRST_CONNECTION_ID

BEGIN_DECLS

enum ProcMgrMessageType
{
    PROC_MGR_MESSAGE_EXIT = 0,
    PROC_MGR_MESSAGE_GETPID,
    PROC_MGR_MESSAGE_INTERRUPT_ATTACH,
    PROC_MGR_MESSAGE_INTERRUPT_DETACH,
    PROC_MGR_MESSAGE_MAP_PHYS,

    /**
     * Not a message. Just a count.
     */
    PROC_MGR_MESSAGE_COUNT,
};

struct ProcMgrMessage
{
    enum ProcMgrMessageType type;

    union {

        struct {
        } dummy;

        struct {
        } exit;

        struct {
        } getpid;

        struct {
            InterruptHandlerFunc func;
            int irq_number;
        } interrupt_attach;

        struct {
            InterruptHandler_t handler;
        } interrupt_detach;

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
            int pid;
        } getpid;

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

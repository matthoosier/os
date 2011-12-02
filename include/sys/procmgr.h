#ifndef __PROCMGR_H__
#define __PROCMGR_H__

#include <sys/decls.h>

BEGIN_DECLS

enum ProcMgrMessageType
{
    PROC_MGR_MESSAGE_DUMMY,
    PROC_MGR_MESSAGE_EXIT,
};

struct ProcMgrMessage
{
    enum ProcMgrMessageType type;

    union {

        struct {
        } dummy;

        struct {
        } exit;

    } payload;
};

struct ProcMgrReply
{
    union {

        struct {
        } dummy;

        struct {
        } exit;

    } payload;
};

END_DECLS

#endif /* __PROCMGR_H__ */

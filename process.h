#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "decls.h"
#include "list.h"
#include "message.h"
#include "mmu.h"
#include "tree-map.h"
#include "vm.h"

BEGIN_DECLS

typedef int Pid_t;

struct Segment
{
    VmAddr_t            base;

    /**
     * There will be (@length + PAGE_SIZE - 1) % PAGE_SIZE pages held in @pages_head.
     */
    size_t              length;

    struct list_head    pages_head;
    struct list_head    link;
};

struct Process
{
    struct TranslationTable   * pagetable;
    VmAddr_t                    entry;
    struct list_head            segments_head;
    struct Thread             * thread;
    Pid_t                       pid;

    struct TreeMap            * chid_to_channel_map;
    struct list_head            channels_head;

    Channel_t                   next_chid;
};

struct Process * ProcessStartManager ();

struct Process * ProcessCreate (const char executableName[]);

struct Process * ProcessLookup (Pid_t pid);

END_DECLS

#endif /* __PROCESS_H__ */

#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <sys/decls.h>
#include <sys/error.h>
#include <sys/spinlock.h>

#include <kernel/list.h>
#include <kernel/message.h>
#include <kernel/message.h>
#include <kernel/mmu.h>
#include <kernel/tree-map.h>
#include <kernel/vm.h>

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
    Spinlock_t                  lock;

    struct TranslationTable   * pagetable;
    VmAddr_t                    entry;
    char                        comm[16];
    struct list_head            segments_head;
    struct Thread             * thread;
    Pid_t                       pid;

    struct TreeMap            * id_to_channel_map;
    struct list_head            channels_head;
    Channel_t                   next_chid;

    struct TreeMap            * id_to_connection_map;
    struct list_head            connections_head;
    Connection_t                next_coid;

    struct TreeMap            * id_to_message_map;
    int                         next_msgid;
};

/**
 * Calculates the value of the 'pagetable' field on a process object.
 * Implemented as a symbol to be usable from places where C structures'
 * field can't be accessed by name (e.g., assembly code).
 */
struct TranslationTable * ProcessGetTranslationTable (struct Process *);

/**
 * Called one time at startup to initialize the privileged kernel thread
 * that implements the responses to basic process-service message API.
 */
struct Process * ProcessStartManager ();

/**
 * Called any time after startup, to fetch the privileged kernel running
 * the ProcMgr.
 */
struct Process * ProcessGetManager ();

struct Process * ProcessCreate (const char executableName[]);

struct Process * ProcessLookup (Pid_t pid);

Channel_t ProcessRegisterChannel (
        struct Process * p,
        struct Channel * c
        );

int ProcessUnregisterChannel (
        struct Process * p,
        Channel_t id
        );

struct Channel * ProcessLookupChannel (
        struct Process * p,
        Channel_t id
        );

Connection_t ProcessRegisterConnection (
        struct Process * p,
        struct Connection * c
        );

int ProcessUnregisterConnection (
        struct Process * p,
        Connection_t id
        );

struct Connection * ProcessLookupConnection (
        struct Process * p,
        Connection_t id
        );

Message_t ProcessRegisterMessage (
        struct Process * p,
        struct Message * m
        );

int ProcessUnregisterMessage (
        struct Process * p,
        Message_t id
        );

struct Message * ProcessLookupMessage (
        struct Process * p,
        Message_t id
        );

END_DECLS

#endif /* __PROCESS_H__ */

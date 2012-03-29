#include <string.h>

#include <sys/arch.h>
#include <sys/elf.h>
#include <sys/error.h>
#include <sys/procmgr.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/ramfs.h>
#include <kernel/thread.hpp>
#include <kernel/timer.h>
#include <kernel/tree-map.h>

/** Handed off between spawner and spawnee threads */
struct process_creation_context
{
    struct Thread     * caller;
    struct Process    * created;
    const char        * executableName;
    bool                caller_should_release;
};

/** Allocates instances of 'struct Process' */
static struct ObjectCache   process_cache;

/** Allocates instances of 'struct Segment' */
static struct ObjectCache   segment_cache;

static Once_t               caches_init_control = ONCE_INIT;

/** Allocates monotonically increasing process identifiers */
static Pid_t get_next_pid (void);

/** Fetches Pid_t -> (struct Process *) mappings */
static struct Process * PidMapLookup (Pid_t key);
static struct Process * PidMapInsert (Pid_t key, struct Process * process);
static struct Process * PidMapRemove (Pid_t key);

static void init_caches (void * ignored)
{
    ObjectCacheInit(&process_cache, sizeof(struct Process));
    ObjectCacheInit(&segment_cache, sizeof(struct Segment));
}

static struct Segment * SegmentAlloc ()
{
    struct Segment * s;

    Once(&caches_init_control, init_caches, NULL);
    s = (struct Segment *)ObjectCacheAlloc(&segment_cache);

    if (s) {
        memset(s, 0, sizeof(*s));
        s->pages_head.DynamicInit();
        s->link.DynamicInit();
    }

    return s;
}

static void SegmentFree (
        struct Segment * segment,
        struct TranslationTable * table
        )
{
    typedef List<Page, &Page::list_link> list_t;

    VmAddr_t map_addr;

    map_addr = segment->base;

    for (list_t::Iterator i = segment->pages_head.Begin(); i; ++i) {

        struct Page * page = *i;

        bool unmapped = TranslationTableUnmapPage(
                table,
                map_addr
                );

        assert(unmapped);
        map_addr += PAGE_SIZE;

        segment->pages_head.Remove(page);
        VmPageFree(page);
    }

    ObjectCacheFree(&segment_cache, segment);
}

static struct Process * ProcessAlloc ()
{
    struct Process * p;

    Once(&caches_init_control, init_caches, NULL);
    p = (struct Process *)ObjectCacheAlloc(&process_cache);

    if (p) {
        memset(p, 0, sizeof(*p));

        SpinlockInit(&p->lock);

        p->id_to_channel_map    = TreeMapAlloc(TreeMapSignedIntCompareFunc);
        p->id_to_connection_map = TreeMapAlloc(TreeMapSignedIntCompareFunc);
        p->id_to_message_map    = TreeMapAlloc(TreeMapSignedIntCompareFunc);

        if (p->id_to_channel_map && p->id_to_connection_map && p->id_to_message_map) {
            p->segments_head.DynamicInit();
            p->channels_head.DynamicInit();
            p->connections_head.DynamicInit();
            p->next_chid = FIRST_CHANNEL_ID;
            p->next_coid = FIRST_CONNECTION_ID;
            p->next_msgid = 1;
        }
        else {
            if (p->id_to_channel_map)       TreeMapFree(p->id_to_channel_map);
            if (p->id_to_connection_map)    TreeMapFree(p->id_to_connection_map);
            if (p->id_to_message_map)       TreeMapFree(p->id_to_message_map);

            ObjectCacheFree(&process_cache, p);
            p = NULL;
        }
    }

    return p;
}

static void ProcessFree (struct Process * process)
{
    /* Free all channels owned by process */
    while (!process->channels_head.Empty()) {
        struct Channel * channel = process->channels_head.PopFirst();
        KChannelFree(channel);
    }

    /* Free all connections owned by process */
    while (!process->connections_head.Empty()) {
        struct Connection * connection = process->connections_head.PopFirst();
        KDisconnect(connection);
    }

    /* XXX: Free the message (if any) the process has sent but not yet been replied */

    /* XXX: Free all messages that the process has received but not yet responded to */

    TreeMapFree(process->id_to_channel_map);
    TreeMapFree(process->id_to_connection_map);
    TreeMapFree(process->id_to_message_map);
    
    /* Deallocate virtual memory of the process */
    while (!process->segments_head.Empty()) {
        struct Segment * s = process->segments_head.PopFirst();
        SegmentFree(s, process->pagetable);
    }

    /* Reclaim the memory of the process's pagetable */
    if (process->pagetable) {
        TranslationTableFree(process->pagetable);
        process->pagetable = NULL;
    }

    ObjectCacheFree(&process_cache, process);
}

struct Process * exec_into_current (
        const char executableName[]
        )
{
    struct TranslationTable *tt;
    struct Process * p;
    const struct ImageEntry * image;
    const Elf32_Ehdr * hdr;
    unsigned int i;
    Connection_t procmgr_coid;
    struct Connection * procmgr_con;
    struct Process * procmgr;
    struct Channel * procmgr_chan;

    image = RamFsGetImage(executableName);

    if (!image) {
        return NULL;
    }

    hdr = (const Elf32_Ehdr *)image->fileStart;

    if(hdr->e_ident[EI_MAG0] != ELFMAG0 || hdr->e_ident[EI_MAG1] != ELFMAG1 ||
       hdr->e_ident[EI_MAG2] != ELFMAG2 || hdr->e_ident[EI_MAG3] != ELFMAG3) {
        return NULL;
    }

    if ((hdr->e_entry == 0) || (hdr->e_type != ET_EXEC) ||
        (hdr->e_machine != EM_ARM) || (hdr->e_phoff == 0)) {
        return NULL;
    }

    if ((p = ProcessAlloc()) == NULL) {
        return NULL;
    }

    /* Record our name */
    strncpy(p->comm, executableName, sizeof(p->comm));

    /* Get pagetable for the new process */
    tt = p->pagetable = TranslationTableAlloc();

    if (!tt) {
        assert(false);
        goto free_process;
    }

    MmuSetUserTranslationTable(tt);

    p->entry = hdr->e_entry;

    for (i = 0; i < hdr->e_phnum; i++) {
        const Elf32_Phdr * phdr;

        phdr = (const Elf32_Phdr *)
                    (image->fileStart + hdr->e_phoff + i * hdr->e_phentsize);

        if (phdr->p_type == PT_LOAD) {

            struct Segment * segment;
            unsigned int num_pages;
            unsigned int j;

            segment = SegmentAlloc();

            if (!segment) {
                assert(false);
                goto free_process;
            }

            /* Handle segments that don't line up on page boundaries */
            segment->base = phdr->p_vaddr & PAGE_MASK;
            segment->length = phdr->p_memsz + (phdr->p_vaddr - segment->base);

            /* All the address space of the process must be in user memory range */
            assert(segment->base + segment->length <= KERNEL_MODE_OFFSET);

            num_pages = (segment->length + PAGE_SIZE - 1) / PAGE_SIZE;

            /* Map in all the memory that will hold the segment */
            for (j = 0; j < num_pages; j++) {
                struct Page * page = VmPageAlloc();
                bool mapped;

                mapped = TranslationTableMapPage(
                        tt,
                        segment->base + PAGE_SIZE * j,
                        V2P(page->base_address),
                        PROT_USER_READWRITE
                        );

                if (!mapped) {
                    assert(false);
                    SegmentFree(segment, tt);
                    segment = NULL;
                    goto free_process;
                }

                segment->pages_head.Append(page);
            }

            /* With VM configured, simple memcpy() to load the contents. */

            /* ... first the explicitly initialized part */
            memcpy(
                (void *)phdr->p_vaddr,
                image->fileStart + phdr->p_offset,
                phdr->p_filesz
                );

            /* ... now fill the zero-init part */
            if (phdr->p_filesz < phdr->p_memsz) {
                memset(
                    (void *)(phdr->p_vaddr + phdr->p_filesz),
                    0,
                    phdr->p_memsz - phdr->p_filesz
                    );
            }

            p->segments_head.Append(segment);
        }
    }

    /* Allocate, assign, and record Pid */
    p->pid = get_next_pid();
    PidMapInsert(p->pid, p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    /* Establish the connection to the Process Manager's single channel. */
    procmgr = ProcessLookup(PROCMGR_PID);
    assert(procmgr != NULL);
    procmgr_chan = ProcessLookupChannel(procmgr, FIRST_CHANNEL_ID);
    assert(procmgr_chan != NULL);

    procmgr_con = KConnect(procmgr_chan);

    if (!procmgr_chan) {
        goto free_process;
    }

    procmgr_coid = ProcessRegisterConnection(p, procmgr_con);
    
    if (procmgr_coid < 0) {
        /* Not enough resources to register the connection */
        KDisconnect(procmgr_con);
        goto free_process;
    }

    assert(procmgr_coid == PROCMGR_CONNECTION_ID);
    
    return p;

free_process:

    ProcessFree(p);
    p = NULL;

    return p;
}

void process_creation_thread (void * pProcessCreationContext)
{
    struct process_creation_context * context;
    struct Process * p;

    context  = (struct process_creation_context *)pProcessCreationContext;
    context->created = p = exec_into_current(context->executableName);

    /* Release the spawner now that we have the resulting Process object */
    context->caller_should_release = true;

    if (p) {

        /* Jump into the new process */
        uint32_t spsr;

        /* Configure the SPSR to be user-mode execution */
        asm volatile(
            "mrs %[spsr], spsr              \n\t"
            "mov %[spsr], %[usr_mode_bits]  \n\t"
            "msr spsr, %[spsr]              \n\t"
            : [spsr] "=r" (spsr)
            : [usr_mode_bits] "i" (ARM_USR_MODE_BITS)
        );

        /* Jump to user mode (SPSR becomes the user-mode CPSR */
        asm volatile(
            "mov lr, %[user_pc]     \n\t"
            "mov r0, #0             \n\t"
            "mov r1, #0             \n\t"
            "mov r2, #0             \n\t"
            "mov r3, #0             \n\t"
            "mov r4, #0             \n\t"
            "mov r5, #0             \n\t"
            "mov r6, #0             \n\t"
            "mov r7, #0             \n\t"
            "mov r8, #0             \n\t"
            "mov r9, #0             \n\t"
            "mov r10, #0            \n\t"
            "mov r11, #0            \n\t"
            "mov r12, #0            \n\t"
            "movs pc, lr            \n\t"
            :
            : [user_pc] "r" (p->entry)
        );
    }

    /*
    If control reaches here, it's because this thread has no more purpose.

    The resources for the process couldn't be loaded, and a return value has
    already been created back to the spawner.

    Just let this thread fall off the end of itself and be reclaimed.
    */
}

struct Process * ProcessCreate (const char executableName[])
{
    struct process_creation_context context;

    if (ProcessGetManager() == NULL) {
        /* Something bad happened. Process manager wasn't spawned. */
        return NULL;
    }

    context.caller = THREAD_CURRENT();
    context.created = NULL;
    context.executableName = executableName;
    context.caller_should_release = false;

    /* Resulting process object will be stored into context->created */
    ThreadCreate(process_creation_thread, &context);

    /* Forked thread will wake us back up when the process creation is done */
    while (!context.caller_should_release) {
        ThreadYieldWithRequeue();
    }

    return context.created;
}

static Pid_t get_next_pid ()
{
    static Pid_t counter = PROCMGR_PID + 1;

    return counter++;
}

static Spinlock_t pid_map_lock = SPINLOCK_INIT;

static void alloc_pid_map (void * ppPidMap)
{
    struct TreeMap ** ptrPtrPidMap = (struct TreeMap **)ppPidMap;

    *ptrPtrPidMap = (struct TreeMap *)TreeMapAlloc(TreeMapSignedIntCompareFunc);
    assert(*ptrPtrPidMap != NULL);
}

static struct TreeMap * get_pid_map ()
{
    static struct TreeMap * pid_map;
    static Once_t           pid_map_once = ONCE_INIT;

    Once(&pid_map_once, alloc_pid_map, &pid_map);
    return pid_map;
}

static struct Process * PidMapLookup (Pid_t key)
{
    struct Process * ret;

    SpinlockLock(&pid_map_lock);
    ret = (struct Process *)TreeMapLookup(get_pid_map(), (TreeMapKey_t)key);
    SpinlockUnlock(&pid_map_lock);

    return ret;
}

static struct Process * PidMapInsert (Pid_t key, struct Process * process)
{
    struct Process * ret;

    SpinlockLock(&pid_map_lock);
    ret = (struct Process *)TreeMapInsert(get_pid_map(), (TreeMapKey_t)key, process);
    SpinlockUnlock(&pid_map_lock);

    return ret;
}

static struct Process * PidMapRemove (Pid_t key)
{
    struct Process * ret;

    SpinlockLock(&pid_map_lock);
    ret = (struct Process *)TreeMapRemove(get_pid_map(), (TreeMapKey_t)key);
    SpinlockUnlock(&pid_map_lock);

    return ret;
}

struct Process * ProcessLookup (Pid_t pid)
{
    return PidMapLookup(pid);
}

static void process_manager_thread (void * pProcessCreationContext)
{
    struct process_creation_context * caller_context;
    struct Process * p;
    struct Channel * channel;

    struct ProcMgrMessage   buf;
    struct Message        * m;

    caller_context = (struct process_creation_context *)pProcessCreationContext;

    /* Allocate the singular channel on which the Process Manager listens for messages */
    channel = KChannelAlloc();

    if (!channel) {
        /* This is unrecoverably bad. */
        assert(false);
        caller_context->caller_should_release = true;
        caller_context->created = NULL;
        return;
    }

    caller_context->created = p = ProcessAlloc();

    /* Record our name */
    strncpy(p->comm, "procmgr", sizeof(p->comm));

    /* Allocate, assign, and record Pid */
    p->pid = PROCMGR_PID;
    PidMapInsert(p->pid, p);
    assert(PidMapLookup(p->pid) == p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    /* Map the channel to a well-known integer identifier */
    TreeMapInsert(p->id_to_channel_map, (TreeMapKey_t)p->next_chid, channel);
    assert(TreeMapLookup(p->id_to_channel_map, (TreeMapKey_t)p->next_chid) == channel);
    p->next_chid++;

    /* Start periodic timer to use for pre-emption */
    TimerStartPeriodic(1000);
  
    /* Release the spawner now that we have the resulting Process object */
    caller_context->caller_should_release = true;

    while (true) {
        ssize_t len = KMessageReceive(
                channel,
                &m,
                &buf,
                sizeof(buf)
                );

        if (len == sizeof(buf)) {

            ProcMgrOperationFunc handler = ProcMgrGetMessageHandler(buf.type);

            if (handler != NULL ) {
                handler(m, &buf);
            } else {
                KMessageReply(m, ERROR_NO_SYS, &buf, 0);
            }

        }
        else {
            /* Send back empty reply */
            KMessageReply(m, ERROR_NO_SYS, &buf, 0);
        }
    }
}

static struct Process * managerProcess = NULL;

struct Process * ProcessStartManager ()
{
    struct process_creation_context context;

    assert(managerProcess == NULL);

    context.caller = THREAD_CURRENT();
    context.created = NULL;
    context.executableName = NULL;
    context.caller_should_release = false;

    /* Resulting process object will be stored into context->created */
    ThreadCreate(process_manager_thread, &context);

    /* Forked thread will wake us back up when the process creation is done */
    while (!context.caller_should_release) {
        ThreadYieldWithRequeue();
    }

    managerProcess = context.created;

    return managerProcess;
}

struct Process * ProcessGetManager ()
{
    assert(managerProcess != NULL);
    return managerProcess;
}

Channel_t ProcessRegisterChannel (
        struct Process * p,
        struct Channel * c
        )
{
    Channel_t id = p->next_chid++;

    if (TreeMapLookup(p->id_to_channel_map, (TreeMapKey_t)id) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    TreeMapInsert(p->id_to_channel_map, (TreeMapKey_t)id, c);

    if (TreeMapLookup(p->id_to_channel_map, (TreeMapKey_t)id) != c) {
        return -ERROR_NO_MEM;
    }

    p->channels_head.Append(c);
    return id;
}

int ProcessUnregisterChannel (
        struct Process * p,
        Channel_t id
        )
{
    struct Channel * c = (struct Channel *)TreeMapRemove(p->id_to_channel_map, (TreeMapKey_t)id);

    if (!c) {
        return -ERROR_INVALID;
    }

    p->channels_head.Remove(c);

    return ERROR_OK;
}

struct Channel * ProcessLookupChannel (
        struct Process * p,
        Channel_t id
        )
{
    return (struct Channel *)TreeMapLookup(p->id_to_channel_map, (TreeMapKey_t)id);
}

Connection_t ProcessRegisterConnection (
        struct Process * p,
        struct Connection * c
        )
{
    Connection_t id = p->next_coid++;

    if (TreeMapLookup(p->id_to_connection_map, (TreeMapKey_t)id) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    TreeMapInsert(p->id_to_connection_map, (TreeMapKey_t)id, c);

    if (TreeMapLookup(p->id_to_connection_map, (TreeMapKey_t)id) != c) {
        return -ERROR_NO_MEM;
    }

    p->connections_head.Append(c);
    return id;
}

int ProcessUnregisterConnection (
        struct Process * p,
        Connection_t id
        )
{
    struct Connection * c = (struct Connection *)TreeMapRemove(p->id_to_connection_map, (TreeMapKey_t)id);

    if (!c) {
        return -ERROR_INVALID;
    }

    p->connections_head.Remove(c);
    return ERROR_OK;
}

struct Connection * ProcessLookupConnection (
        struct Process * p,
        Connection_t id
        )
{
    return (struct Connection *)TreeMapLookup(p->id_to_connection_map, (TreeMapKey_t)id);
}

Message_t ProcessRegisterMessage (
        struct Process * p,
        struct Message * m
        )
{
    int msgid = p->next_msgid++;

    if (TreeMapLookup(p->id_to_message_map, (TreeMapKey_t)msgid) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    TreeMapInsert(p->id_to_message_map, (TreeMapKey_t)msgid, m);

    if (TreeMapLookup(p->id_to_message_map, (TreeMapKey_t)msgid) != m) {
        return -ERROR_NO_MEM;
    }

    return msgid;
}

int ProcessUnregisterMessage (
        struct Process * p,
        Message_t id
        )
{
    struct Message * m = (struct Message *)TreeMapRemove(p->id_to_message_map, (TreeMapKey_t)id);

    if (!m) {
        return -ERROR_INVALID;
    }

    return ERROR_OK;
}

struct Message * ProcessLookupMessage (
        struct Process * p,
        Message_t id
        )
{
    return (struct Message *)TreeMapLookup(p->id_to_message_map, (TreeMapKey_t)id);
}

struct TranslationTable * ProcessGetTranslationTable (struct Process * process)
{
    return process->pagetable;
}

/**
 * Handler for PROC_MGR_MESSAGE_EXIT.
 */
static void HandleExit (
        struct Message * message,
        const struct ProcMgrMessage * buf
        )
{
    /* Syscalls are always invoked by processes */
    assert(message->sender->process != NULL);

    /* Get rid of mapping */
    assert(
        PidMapLookup(message->sender->process->pid)
        ==
        message->sender->process
        );

    PidMapRemove(message->sender->process->pid);

    /* Reclaim all userspace resources */
    ProcessFree(message->sender->process);
    message->sender->process = NULL;

    /* Force sender's kernel thread to appear done */
    message->sender->state = THREAD_STATE_FINISHED;

    /* Reap sender's kernel thread */
    ThreadJoin(message->sender);

    /* No MessageReply(), so manually free the Message */
    KMessageFree(message);
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_EXIT, HandleExit)

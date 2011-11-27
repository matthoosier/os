#include <string.h>

#include "arch.h"
#include "array.h"
#include "assert.h"
#include "elf.h"
#include "object-cache.h"
#include "once.h"
#include "process.h"
#include "ramfs.h"
#include "thread.h"
#include "tree-map.h"

#define PROCMGR_PID     0
#define PROCMGR_CHID    0

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
static Pid_t get_next_pid ();

/** Fetches Pid_t -> (struct Process *) mappings */
static struct TreeMap * pid_to_process_map ();

static void init_caches (void * ignored)
{
    ObjectCacheInit(&process_cache, sizeof(struct Process));
    ObjectCacheInit(&segment_cache, sizeof(struct Segment));
}

static struct Segment * alloc_segment ()
{
    struct Segment * s;

    Once(&caches_init_control, init_caches, NULL);
    s = ObjectCacheAlloc(&segment_cache);

    if (s) {
        s->base = 0;
        s->length = 0;
        INIT_LIST_HEAD(&s->pages_head);
        INIT_LIST_HEAD(&s->link);
    }

    return s;
}

static void free_segment (
        struct Segment * segment,
        struct TranslationTable * table
        )
{
    VmAddr_t map_addr;
    struct list_head * page_cursor;
    struct list_head * page_cursor_temp;

    map_addr = segment->base;

    list_for_each_safe (page_cursor, page_cursor_temp, &segment->pages_head) {

        struct Page * page = list_entry(page_cursor, struct Page, list_link);

        bool unmapped = TranslationTableUnmapPage(
                table,
                map_addr
                );

        assert(unmapped);
        map_addr += PAGE_SIZE;

        list_del_init(&page->list_link);
        VmPageFree(page);
    }

    ObjectCacheFree(&segment_cache, segment);
}

static struct Process * alloc_process ()
{
    struct Process * p;

    Once(&caches_init_control, init_caches, NULL);
    p = ObjectCacheAlloc(&process_cache);

    if (p) {
        p->chid_to_channel_map = TreeMapAlloc(TreeMapSignedIntCompareFunc);

        if (p->chid_to_channel_map) {
            p->entry = 0;
            INIT_LIST_HEAD(&p->segments_head);
            p->pagetable = NULL;
            INIT_LIST_HEAD(&p->channels_head);
            p->next_chid = PROCMGR_CHID + 1;
        }
    }

    return p;
}

static void free_process (struct Process * process)
{
    /* Free all channels owned by process */
    while (!list_empty(&process->channels_head)) {
        struct Channel * channel = list_first_entry(
                &process->channels_head,
                struct Channel,
                link
                );
        ChannelFree(channel);
    }

    /*
    Free tree mapping chid's to Channel objects. No need to free individual values
    stored in the tree first.
    */
    TreeMapFree(process->chid_to_channel_map);
    
    /* Deallocate virtual memory of the process */
    while (!list_empty(&process->segments_head)) {

        struct Segment * s = list_first_entry(
                &process->segments_head,
                struct Segment,
                link
                );

        list_del_init(&s->link);
        free_segment(s, process->pagetable);
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

    if ((p = alloc_process()) == NULL) {
        return NULL;
    }

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

            segment = alloc_segment();

            if (!segment) {
                assert(false);
                goto free_process;
            }

            segment->base = phdr->p_vaddr;
            segment->length = phdr->p_memsz;

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
                        PROT_READ | PROT_WRITE
                        );

                if (!mapped) {
                    assert(false);
                    free_segment(segment, tt);
                    segment = NULL;
                    goto free_process;
                }

                list_add_tail(&page->list_link, &segment->pages_head);
            }

            /* With VM configured, simple memcpy() to load the contents. */

            /* ... first the explicitly initialized part */
            memcpy(
                (void *)segment->base,
                image->fileStart + phdr->p_offset,
                phdr->p_filesz
                );

            /* ... now fill the zero-init part */
            if (phdr->p_filesz < phdr->p_memsz) {
                memset(
                    (void *)(segment->base + phdr->p_filesz),
                    0,
                    phdr->p_memsz - phdr->p_filesz
                    );
            }

            list_add_tail(&segment->link, &p->segments_head);
        }
    }

    /* Allocate, assign, and record Pid */
    p->pid = get_next_pid();
    TreeMapInsert(pid_to_process_map(), (TreeMapKey_t)p->pid, p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    return p;

free_process:

    free_process(p);
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
            ".include \"arm-defs.inc\"  \n\t"
            "mrs %[spsr], spsr          \n\t"
            "mov %[spsr], #usr          \n\t"
            "msr spsr, %[spsr]          \n\t"
            : [spsr] "=r" (spsr)
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

    context.caller = THREAD_CURRENT();
    context.created = NULL;
    context.executableName = executableName;
    context.caller_should_release = false;

    /* Resulting process object will be stored into context->created */
    ThreadCreate(process_creation_thread, &context);

    /* Forked thread will wake us back up when the process creation is done */
    while (!context.caller_should_release) {
        ThreadAddReady(THREAD_CURRENT());
        ThreadYieldNoRequeue();
    }

    return context.created;
}

static Pid_t get_next_pid ()
{
    static Pid_t counter = PROCMGR_PID + 1;

    return counter++;
}

static struct TreeMap * pid_to_process_map ()
{
    static struct TreeMap * map;
    static Once_t init_control = ONCE_INIT;

    void init (void * ignored)
    {
        map = TreeMapAlloc(TreeMapSignedIntCompareFunc);
    }

    Once(&init_control, init, NULL);
    return map;
}

struct Process * ProcessLookup (Pid_t pid)
{
    return TreeMapLookup(pid_to_process_map(), (TreeMapKey_t)pid);
}

static void process_manager_thread (void * pProcessCreationContext)
{
    struct process_creation_context * caller_context;
    struct Process * p;
    struct Channel * channel;

    caller_context = (struct process_creation_context *)pProcessCreationContext;

    /* Allocate the singular channel on which the Process Manager listens for messages */
    channel = ChannelAlloc();

    if (!channel) {
        /* This is unrecoverably bad. */
        assert(false);
        caller_context->caller_should_release = true;
        caller_context->created = NULL;
        return;
    }

    caller_context->created = p = alloc_process();

    /* Allocate, assign, and record Pid */
    p->pid = get_next_pid();
    TreeMapInsert(pid_to_process_map(), (TreeMapKey_t)p->pid, p);
    assert(TreeMapLookup(pid_to_process_map(), (TreeMapKey_t)p->pid) == p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    TreeMapInsert(p->chid_to_channel_map, (TreeMapKey_t)PROCMGR_CHID, channel);
    assert(TreeMapLookup(p->chid_to_channel_map, (TreeMapKey_t)PROCMGR_CHID) == channel);

    /* Release the spawner now that we have the resulting Process object */
    caller_context->caller_should_release = true;

    /* TODO: replace with message-receiving loop */
    while (true) {

        static char         buf[4096];
        struct Message    * context;

        while (true) {
            int len = MessageReceive(
                    channel,
                    &context,
                    buf,
                    N_ELEMENTS(buf)
                    );

            /* Send back empty reply */
            len = len;
            MessageReply(context, buf, 0);
        }
    }
}

struct Process * ProcessStartManager ()
{
    static Once_t once = ONCE_INIT;
    static struct Process * managerProcess;

    void startup_process_manager (void * ptrPtrProcess)
    {
        struct Process ** p;
        struct process_creation_context context;

        p = (struct Process **)ptrPtrProcess;

        context.caller = THREAD_CURRENT();
        context.created = NULL;
        context.executableName = NULL;
        context.caller_should_release = false;

        /* Resulting process object will be stored into context->created */
        ThreadCreate(process_manager_thread, &context);

        /* Forked thread will wake us back up when the process creation is done */
        while (!context.caller_should_release) {
            ThreadAddReady(THREAD_CURRENT());
            ThreadYieldNoRequeue();
        }

        *p = context.created;
    }

    Once(&once, startup_process_manager, &managerProcess);
    return managerProcess;
}

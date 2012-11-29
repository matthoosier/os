#include <string.h>

#include <sys/arch.h>
#include <sys/atomic.h>
#include <sys/elf.h>
#include <sys/error.h>
#include <sys/procmgr.h>

#include <kernel/array.h>
#include <kernel/assert.h>
#include <kernel/process.hpp>
#include <kernel/procmgr.hpp>
#include <kernel/ramfs.h>
#include <kernel/semaphore.hpp>
#include <kernel/thread.hpp>
#include <kernel/timer.hpp>
#include <kernel/tree-map.hpp>

/** Handed off between spawner and spawnee threads */
struct process_creation_context
{
    Thread      * caller;
    Process     * created;
    const char  * executableName;
    Semaphore   * baton;
};

/** Allocates monotonically increasing process identifiers */
static Pid_t get_next_pid (void);

/** Terminates a process and removes registrations related to it */
static void TerminateProcess (Process * p);

SyncSlabAllocator<Segment> Segment::sSlab;

Segment::Segment (Process & p)
    : base(0)
    , length(0)
    , owner(p)
{
}

Segment::~Segment ()
{
    typedef List<Page, &Page::list_link> list_t;

    VmAddr_t map_addr = this->base;
    TranslationTable * pagetable = owner.GetTranslationTable();

    for (list_t::Iterator i = this->pages_head.Begin(); i; ++i) {

        Page * page = *i;

        bool unmapped = pagetable->UnmapPage(map_addr);

        assert(unmapped);
        map_addr += PAGE_SIZE;

        this->pages_head.Remove(page);
        Page::Free(page);
    }
}

SyncSlabAllocator<Process> Process::sSlab;

Process::Process ()
    : pagetable(0)
    , entry(0)
    , thread(NULL)
    , next_chid(FIRST_CHANNEL_ID)
    , next_coid(FIRST_CONNECTION_ID)
    , next_msgid(1)
{
    SpinlockInit(&this->lock);

    this->id_to_channel_map    = new IdToChannelMap_t(IdToChannelMap_t::SignedIntCompareFunc);
    this->id_to_connection_map = new IdToConnectionMap_t(IdToConnectionMap_t::SignedIntCompareFunc);
    this->id_to_message_map    = new IdToMessageMap_t(IdToMessageMap_t::SignedIntCompareFunc);
}

static void ForeachMessage (
        RawTreeMap::Key_t key,
        RawTreeMap::Value_t value,
        void * ignored
        )
{
    Message * message = static_cast<Message *>(value);

    // Only synchronous messages end up registered in the id_to_message_map.
    //
    // So, each of these has a client send-blocked on it. Just reply back
    // with a failure code. The client will be responsible for deallocating
    // the message instance.
    message->Reply(ERROR_NO_SYS, NULL, 0);
}

static void ForeachConnection (
        RawTreeMap::Key_t key,
        RawTreeMap::Value_t value,
        void * ignored
        )
{
    Connection * connection = static_cast<Connection *>(value);
    delete connection;
}

Process::~Process ()
{
    /*
    Free all connections owned by process. Internally, the destructor for
    the connection object will free any messages that have been queued
    for sending but aren't yet received by a server.
    */
    this->id_to_connection_map->Foreach (ForeachConnection, NULL);

    /* Free all channels owned by process */
    while (!this->channels_head.Empty()) {
        Channel * channel = this->channels_head.PopFirst();
        delete channel;
    }

    /* Free all messages that the process has received but not yet responded to */
    this->id_to_message_map->Foreach (ForeachMessage, NULL);
    
    /* Deallocate virtual memory of the process */
    while (!this->segments_head.Empty()) {
        Segment * s = this->segments_head.PopFirst();
        delete s;
    }
}

Process * Process::execIntoCurrent (const char executableName[]) throw (std::bad_alloc)
{
    TranslationTable *tt;
    Process * p;
    const struct ImageEntry * image;
    const Elf32_Ehdr * hdr;
    unsigned int i;
    Connection_t procmgr_coid;
    Connection * procmgr_con;
    Process * procmgr;
    Channel * procmgr_chan;

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

    // May throw. If it does, this function'll just cascade unwind to the caller,
    // who's trapping the exception
    try {
        p = new Process();
    } catch (std::bad_alloc) {
        return NULL;
    }

    /* Record our name */
    strncpy(p->comm, executableName, sizeof(p->comm));

    /* Allocate, assign, and record Pid */
    p->pid = get_next_pid();
    Process::Register(p->pid, p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    // Get pagetable for the new process.
    try {
        p->pagetable = new TranslationTable();
    } catch (std::bad_alloc a) {
        assert(false);
        THREAD_CURRENT()->process = NULL;
        goto free_process;
    }

    tt = *p->pagetable;

    /*
    Make sure that pagetable installation is flushed out to memory before
    making any use of it. This will make sure that an inconveniently timed
    preemption won't see the Process object in a moment when the fields
    modified above haven't been spilled back yet.
    */
    AtomicCompilerMemoryBarrier();

    // Okay, now it's safe to use the translation table.
    TranslationTable::SetUser(tt);

    p->entry = hdr->e_entry;

    for (i = 0; i < hdr->e_phnum; i++) {
        const Elf32_Phdr * phdr;

        phdr = (const Elf32_Phdr *)
                    (image->fileStart + hdr->e_phoff + i * hdr->e_phentsize);

        if (phdr->p_type == PT_LOAD) {

            Segment * segment;
            unsigned int num_pages;
            unsigned int j;

            try {
                segment = new Segment(*p);
            } catch (std::bad_alloc) {
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
                Page * page = Page::Alloc();
                bool mapped;

                mapped = tt->MapPage(
                        segment->base + PAGE_SIZE * j,
                        V2P(page->base_address),
                        PROT_USER_READWRITE
                        );

                if (!mapped) {
                    assert(false);
                    delete segment;
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

    /* Establish the connection to the Process Manager's single channel. */
    procmgr = Lookup(PROCMGR_PID);
    assert(procmgr != NULL);
    procmgr_chan = procmgr->LookupChannel(FIRST_CHANNEL_ID);
    assert(procmgr_chan != NULL);

    try {
        procmgr_con = new Connection(procmgr_chan);
    } catch (std::bad_alloc) {
        goto free_process;
    }

    procmgr_coid = p->RegisterConnection(procmgr_con);
    
    if (procmgr_coid < 0) {
        /* Not enough resources to register the connection */
        delete procmgr_con;
        goto free_process;
    }

    assert(procmgr_coid == PROCMGR_CONNECTION_ID);
    
    return p;

free_process:

    delete p;
    return NULL;
}

void Process::UserProcessThreadBody (void * pProcessCreationContext)
{
    struct process_creation_context * context;
    Process * p;

    context  = (struct process_creation_context *)pProcessCreationContext;
    context->created = p = Process::execIntoCurrent(context->executableName);

    /* Release the spawner now that we have the resulting Process object */
    context->baton->Up();

    if (p) {

        /* Jump into the new process */
        uint32_t spsr;

        assert(!InterruptsDisabled());

        /*
        Need to make sure that no context switch comes along and
        trashes the value we're setting up in SPSR.
        */
        InterruptsDisable();

        /* Configure the SPSR to be user-mode execution */
        asm volatile(
            "mrs %[spsr], spsr              \n\t"
            "mov %[spsr], %[usr_mode_bits]  \n\t"
            "msr spsr, %[spsr]              \n\t"
            : [spsr] "=r" (spsr)
            : [usr_mode_bits] "i" (ARM_PSR_MODE_USR_BITS)
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

        /* Unreachable */
        assert(false);
    }

    /*
    If control reaches here, it's because this thread has no more purpose.

    The resources for the process couldn't be loaded, and a return value has
    already been created back to the spawner.

    Just let this thread fall off the end of itself and be reclaimed.
    */
}

Process * Process::Create (const char executableName[])
{
    struct process_creation_context context;
    Thread * t;

    Semaphore baton(0);

    if (GetManager() == NULL) {
        /* Something bad happened. Process manager wasn't spawned. */
        return NULL;
    }

    context.caller = THREAD_CURRENT();
    context.created = NULL;
    context.executableName = executableName;
    context.baton = &baton;

    /* Resulting process object will be stored into context->created */
    t = Thread::Create(UserProcessThreadBody, &context);

    /* Forked thread will wake us back up when the process creation is done */
    baton.Down();

    if (context.created != 0) {
        return context.created;
    } else {
        t->Join();
        return 0;
    }
}

static Pid_t get_next_pid ()
{
    static Pid_t counter = PROCMGR_PID + 1;

    return counter++;
}

Spinlock_t Process::sPidMapSpinlock = SPINLOCK_INIT;

Process::PidMap_t Process::sPidMap(
        Process::PidMap_t::SignedIntCompareFunc
        );

Process * Process::Register (Pid_t key, Process * process)
{
    Process * ret;

    SpinlockLock(&sPidMapSpinlock);
    ret = sPidMap.Insert(key, process);
    SpinlockUnlock(&sPidMapSpinlock);

    return ret;
}

Process * Process::Remove (Pid_t key)
{
    Process * ret;

    SpinlockLock(&sPidMapSpinlock);
    ret = sPidMap.Remove(key);
    SpinlockUnlock(&sPidMapSpinlock);

    return ret;
}

Process * Process::Lookup (Pid_t pid)
{
    Process * ret;

    SpinlockLock(&sPidMapSpinlock);
    ret = sPidMap.Lookup(pid);
    SpinlockUnlock(&sPidMapSpinlock);

    return ret;
}

Pid_t Process::GetId ()
{
    return this->pid;
}

Thread * Process::GetThread ()
{
    return this->thread;
}

void Process::ManagerThreadBody (void * pProcessCreationContext)
{
    struct process_creation_context * caller_context;
    Process * p;
    Channel * channel;

    struct ProcMgrMessage   buf;
    Message               * m;

    caller_context = (struct process_creation_context *)pProcessCreationContext;

    /* Allocate the singular channel on which the Process Manager listens for messages */
    try {
        channel = new Channel();
    } catch (std::bad_alloc) {
        /* This is unrecoverably bad. */
        assert(false);
        caller_context->created = NULL;
        caller_context->baton->Up();
        return;
    }

    try {
        caller_context->created = p = new Process();
    } catch (std::bad_alloc a) {
        assert(false);
    }

    /* Record our name */
    strncpy(p->comm, "procmgr", sizeof(p->comm));

    /* Allocate, assign, and record Pid */
    p->pid = PROCMGR_PID;
    Process::Register(p->pid, p);
    assert(Process::Lookup(p->pid) == p);

    /* Okay. Save reference to this process object into the current thread */
    p->thread = THREAD_CURRENT();
    THREAD_CURRENT()->process = p;

    /* Map the channel to a well-known integer identifier */
    p->id_to_channel_map->Insert(p->next_chid, channel);
    assert(p->id_to_channel_map->Lookup(p->next_chid) == channel);
    p->next_chid++;

    /* Start periodic timer to use for pre-emption */
    Timer::StartPeriodic(5);
  
    /* Release the spawner now that we have the resulting Process object */
    caller_context->baton->Up();

    while (true) {
        ssize_t hdr_len = offsetof(struct ProcMgrMessage, type) + sizeof(buf.type);
        ssize_t len = channel->ReceiveMessage(
                &m,
                &buf,
                hdr_len);

        if (len == hdr_len) {

            ProcMgrOperationFunc handler = ProcMgrGetMessageHandler(buf.type);

            if (handler != NULL ) {
                handler(m);
            } else {
                m->Reply(ERROR_NO_SYS, &buf, 0);
            }

        }
        else {
            /* Send back empty reply */
            m->Reply(ERROR_NO_SYS, &buf, 0);
        }
    }
}

static Process * managerProcess = NULL;

Process * Process::StartManager ()
{
    struct process_creation_context context;
    Semaphore baton(0);

    assert(managerProcess == NULL);

    context.caller = THREAD_CURRENT();
    context.created = NULL;
    context.executableName = NULL;
    context.baton = &baton;

    /* Resulting process object will be stored into context->created */
    Thread::Create(ManagerThreadBody, &context);

    /* Forked thread will wake us back up when the process creation is done */
    baton.Down();

    managerProcess = context.created;

    return managerProcess;
}

Process * Process::GetManager ()
{
    assert(managerProcess != NULL);
    return managerProcess;
}

Channel_t Process::RegisterChannel (Channel * c)
{
    Channel_t id = this->next_chid++;

    if (this->id_to_channel_map->Lookup(id) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    this->id_to_channel_map->Insert(id, c);

    if (this->id_to_channel_map->Lookup(id) != c) {
        return -ERROR_NO_MEM;
    }

    this->channels_head.Append(c);
    return id;
}

int Process::UnregisterChannel (Channel_t id)
{
    Channel * c = this->id_to_channel_map->Remove(id);

    if (!c) {
        return -ERROR_INVALID;
    }

    this->channels_head.Remove(c);

    return ERROR_OK;
}

Channel * Process::LookupChannel (Channel_t id)
{
    return this->id_to_channel_map->Lookup(id);
}

Connection_t Process::RegisterConnection (Connection * c)
{
    Connection_t id = this->next_coid++;

    if (this->id_to_connection_map->Lookup(id) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    this->id_to_connection_map->Insert(id, c);

    if (this->id_to_connection_map->Lookup(id) != c) {
        return -ERROR_NO_MEM;
    }

    return id;
}

int Process::UnregisterConnection (Connection_t id)
{
    Connection * c = this->id_to_connection_map->Remove(id);

    if (!c) {
        return -ERROR_INVALID;
    }

    return ERROR_OK;
}

Connection * Process::LookupConnection (Connection_t id)
{
    return this->id_to_connection_map->Lookup(id);
}

Message_t Process::RegisterMessage (Message * m)
{
    int msgid = this->next_msgid++;

    if (this->id_to_message_map->Lookup(msgid) != NULL) {
        assert(false);
        return -ERROR_INVALID;
    }

    this->id_to_message_map->Insert(msgid, m);

    if (this->id_to_message_map->Lookup(msgid) != m) {
        return -ERROR_NO_MEM;
    }

    return msgid;
}

int Process::UnregisterMessage (Message_t id)
{
    Message * m = this->id_to_message_map->Remove(id);

    if (!m) {
        return -ERROR_INVALID;
    }

    return ERROR_OK;
}

Message * Process::LookupMessage (Message_t id)
{
    return this->id_to_message_map->Lookup(id);
}

TranslationTable * Process::GetTranslationTable ()
{
    return *this->pagetable;
}

TranslationTable * ProcessGetTranslationTable (Process * p)
{
    return p->GetTranslationTable();
}

static void TerminateProcess (Process * process)
{
    Thread * t = process->GetThread();

    /* Get rid of mapping */
    assert(Process::Lookup(process->GetId()) == process);

    Process::Remove(process->GetId());

    /* Reclaim all userspace resources */
    delete process;
    process = NULL;

    t->process = NULL;

    /* Force sender's kernel thread to appear done */
    Thread::BeginTransaction();
    Thread::MakeUnready(t, Thread::STATE_FINISHED);
    Thread::EndTransaction();

    /* Reap sender's kernel thread */
    t->Join();
}

/**
 * Handler for PROC_MGR_MESSAGE_EXIT.
 */
static void HandleExitMessage (Message * message)
{
    Thread * sender = message->GetSender();

    /* Syscalls are always invoked by processes */
    assert(sender->process != NULL);

    TerminateProcess(sender->process);

    /* No MessageReply(), so manually free the Message */
    delete message;
}

/**
 * Handler for PROC_MGR_MESSAGE_SIGNAL
 */
static void HandleSignalMessage (Message * message)
{
    struct ProcMgrMessage buf;

    ssize_t msg_len = PROC_MGR_MSG_LEN(signal);
    ssize_t actual_len = message->Read(0, &buf, msg_len);

    if (actual_len != msg_len) {
        message->Reply(ERROR_INVALID, NULL, 0);
    }
    else {
        Thread * sender = message->GetSender();
        Process * senderProcess = sender->process;
        Process * signalee = Process::Lookup(buf.payload.signal.signalee_pid);

        if (signalee) {
            TerminateProcess(signalee);

            if (senderProcess == signalee) {
                // Nobody around to return message to
                delete message;
            } else {
                message->Reply(ERROR_OK, NULL, 0);
            }
        } else {
            message->Reply(ERROR_INVALID, NULL, 0);
        }
    }
}

PROC_MGR_OPERATION(PROC_MGR_MESSAGE_EXIT, HandleExitMessage)
PROC_MGR_OPERATION(PROC_MGR_MESSAGE_SIGNAL, HandleSignalMessage)

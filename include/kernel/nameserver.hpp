#ifndef __NAMESERVER_HPP__
#define __NAMESERVER_HPP__

#include <new>

#include <muos/spinlock.h>

#include <kernel/once.h>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>
#include <kernel/string.hpp>
#include <kernel/tree-map.hpp>

// This forward declaration breaks a cycle between NameRecord
// and Channel
class Channel;

// This forward declaration breaks a cycle between NameRecord
// and NameServer
class NameServer;

/**
 * \brief   Association data between a filesystem pathname
 *          and a message-receiving channel bound to it
 *
 * \class NameRecord nameserver.hpp kernel/nameserver.hpp
 */
class NameRecord
{
public:
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    /**
     * Automatically unregisters
     */
    virtual ~NameRecord ();

private:
    NameRecord (const char aFullPath[], RefPtr<Channel> aChannel);

private:
    static SyncSlabAllocator<NameRecord> sSlab;

    String mFullPath;

    RefPtr<Channel> mChannel;

    friend class NameServer;
};

/**
 * \brief   Registry of mappings between filesystem pathnames
 *          and message-receiving channels bound to the paths.
 *
 * \class NameServer nameserver.hpp kernel/nameserver.hpp
 */
class NameServer
{
public:
    static NameRecord *
    RegisterName (char const aFullPath[], RefPtr<Channel> aChannel);

    /**
     * Unregisters the channel bound when aProvider was created
     * by a call to RegisterName().
     *
     * Does not free aProvider; the caller is responsible for that.
     */
    static void UnregisterName (NameRecord * aProvider);

    static RefPtr<Channel> LookupName (char const aFullPath[]);

private:
    void static OnceInit (void * ignored);

private:

    static TreeMap<char const *, NameRecord *> * sMap;
    static Spinlock_t sMapLock;
    static Once_t sOnceControl;
};

#endif

#include <string.h>

#include <kernel/message.hpp>
#include <kernel/nameserver.hpp>
#include <kernel/once.h>

SyncSlabAllocator<NameRecord> NameRecord::sSlab;

NameRecord::NameRecord (const char aFullPath[],
                        RefPtr<Channel> aChannel)
    : mFullPath(aFullPath)
    , mChannel(aChannel)
{
}

NameRecord::~NameRecord()
{
    NameServer::UnregisterName(this);
}

TreeMap<char const *, NameRecord *> * NameServer::sMap;
Spinlock_t NameServer::sMapLock;

Once_t NameServer::sOnceControl = ONCE_INIT;

static int CompareStrings (RawTreeMap::Key_t k1, RawTreeMap::Key_t k2)
{
    char const * s1 = (char const *)k1;
    char const * s2 = (char const *)k2;

    return strcmp(s1, s2);
}

void NameServer::OnceInit (void * ignored)
{
    SpinlockInit(&sMapLock);
    sMap = new TreeMap<char const *, NameRecord *>(&CompareStrings);
}

NameRecord *
NameServer::RegisterName (char const aFullPath[],
                          RefPtr<Channel> aChannel)
{
    NameRecord * ret;

    Once(&sOnceControl, &NameServer::OnceInit, NULL);

    try {
        ret = new NameRecord(aFullPath, aChannel);
    } catch (std::bad_alloc) {
        ret = NULL;
    }

    if (ret) {
        SpinlockLock(&sMapLock);

        if (sMap->Lookup(aFullPath) == NULL) {

            sMap->Insert(ret->mFullPath.c_str(), ret);

            if (sMap->Lookup(ret->mFullPath.c_str()) != ret) {
                delete ret;
                ret = NULL;
            }
        }

        SpinlockUnlock(&sMapLock);
    }

    return ret;
}

void NameServer::UnregisterName (NameRecord * aProvider)
{
    Once(&sOnceControl, &NameServer::OnceInit, NULL);

    SpinlockLock(&sMapLock);
    sMap->Remove(aProvider->mFullPath.c_str());
    SpinlockUnlock(&sMapLock);
}

RefPtr<Channel> NameServer::LookupName (char const aFullPath[])
{
    NameRecord * record;

    Once(&sOnceControl, &NameServer::OnceInit, NULL);

    SpinlockLock(&sMapLock);
    record = sMap->Lookup(aFullPath);
    SpinlockUnlock(&sMapLock);

    if (record) {
        return record->mChannel;
    }
    else {
        return RefPtr<Channel>();
    }
}

#include <kernel/kmalloc.h>
#include <kernel/once.h>
#include <kernel/slaballocator.hpp>
#include <kernel/string.hpp>

#include <string.h>

SyncSlabAllocator<String> String::sSlab;

String::String (String const & aOther) throw (std::bad_alloc)
{
    mData = static_cast<char *>(kmalloc(aOther.mDataSize));

    if (!mData) {
        throw std::bad_alloc();
    }

    mDataSize = aOther.mDataSize;
    strcpy(mData, aOther.mData);
}

String::String (char const aChars[]) throw (std::bad_alloc)
{
    mDataSize = strlen(aChars) + 1;
    mData = static_cast<char *>(kmalloc(mDataSize));

    if (!mData) {
        throw std::bad_alloc();
    }

    strcpy(mData, aChars);
}

String::~String()
{
    kfree(mData, mDataSize);
}

bool String::operator== (String const & aOther) const
{
    if (&aOther == this) {
        return true;
    } else {
        return *this == aOther.mData;
    }
}

bool String::operator== (char const aChars[]) const
{
    return strcmp(mData, aChars) == 0;
}

char const * String::c_str () const
{
    return mData;
}

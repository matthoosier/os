#ifndef __KERNEL_STRING_HPP__
#define __KERNEL_STRING_HPP__

#include <new>

#include <kernel/slaballocator.hpp>

/**
 * \brief   Basic wrapper around a null-terminated C <tt>char</tt> string
 *
 * \class String string.hpp kernel/string.hpp
 */
class String
{
public:
    String (String const & aOther) throw (std::bad_alloc);
    String (char const aChars[]) throw (std::bad_alloc);
    ~String ();

    char const * c_str() const;

    void *  operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    bool operator== (char const aChars[]) const;
    bool operator== (String const & aOther) const;

private:
    static SyncSlabAllocator<String> sSlab;

    char * mData;       //!< Character data, including null terminator
    size_t mDataSize;   //!< Size of mData (in bytes), inclusive of null terminator
};

#endif

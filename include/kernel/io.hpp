#ifndef __IO_HPP__
#define __IO_HPP__

#include <new>

/**
 * \brief   Simple handle to a location and length in memory
 *
 * \class IoBuffer io.hpp kernel/io.hpp
 */
struct IoBuffer
{
public:
    inline explicit IoBuffer (void * aData, size_t aLength)
        : mData(static_cast<uint8_t *>(aData))
        , mLength(aLength)
    {
    }

    template <typename T>
        inline explicit IoBuffer (T & aData)
            : mData(reinterpret_cast<uint8_t *>(&aData))
            , mLength(sizeof(T))
        {
        }

    static inline IoBuffer const & GetEmpty()
    {
        static IoBuffer instance((void *)NULL, 0);
        return instance;
    }

    uint8_t * mData;
    size_t mLength;

private:
    /**
     * Heap allocation is not allowed
     */
    void * operator new (size_t) throw (std::bad_alloc);

    /**
     * Heap allocation is not allowed
     */
    void operator delete (void *) throw ();
};


/**
 * \brief   A sequence of IoBuffer's
 *
 * \class IoVector io.hpp kernel/io.hpp
 */
struct IoVector
{
public:
    inline IoVector (IoBuffer const aBuffers[], size_t aCount)
        : mBuffers(aBuffers)
        , mCount(aCount)
    {
    }

    inline IoVector (IoBuffer const & aBuffer)
        : mBuffers(&aBuffer)
        , mCount(1)
    {
    }

    inline size_t Length () const
    {
        size_t total = 0;

        for (unsigned int i = 0; i < mCount; ++i) {
            total += mBuffers[i].mLength;
        }

        return total;
    }

    inline IoBuffer const * GetBuffers () const
    {
        return mBuffers;
    }

    inline size_t GetCount () const
    {
        return mCount;
    }

private:
    /**
     * Heap allocation is not allowed
     */
    void * operator new (size_t) throw (std::bad_alloc);

    /**
     * Heap allocation is not allowed
     */
    void operator delete (void *) throw ();

private:
    IoBuffer const * mBuffers;
    size_t mCount;
};


#endif

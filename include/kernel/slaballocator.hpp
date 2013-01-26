#ifndef __SLABALLOCATOR_HPP__
#define __SLABALLOCATOR_HPP__

#include <new>

#include <kernel/assert.h>
#include <kernel/object-cache.hpp>

#include <sys/spinlock.h>

/**
 * Fully functional locking mixin class for use by SlabAllocator
 *
 * \class SlabLocker slaballocator.hpp kernel/slaballocator.hpp
 */
class SlabLocker
{
private:
    Spinlock_t mSpinlock;   //!< Non-sleeping synchronization primitive

public:
    /**
     * Initialize synchronization primtive
     */
    inline SlabLocker()
    {
        SpinlockInit(&mSpinlock);
    }

    /**
     * Acquire synchronization primitive
     */
    inline void Lock()
    {
        SpinlockLock(&mSpinlock);
    }

    /**
     * Release synchronization primitive
     */
    inline void Unlock()
    {
        SpinlockUnlock(&mSpinlock);
    }
};

/**
 * No-op locking mixin class for use by SlabAllocator
 *
 * \class SlabNullLocker slaballocator.hpp kernel/slaballocator.hpp
 */
class SlabNullLocker
{
public:
    /**
     * Does nothing
     */
    inline void Lock() {}

    /**
     * Does nothing
     */
    inline void Unlock() {}
};

/**
 * Jeff Bonwick-style object cache slab allocation system. All
 * the heavy lifting is implemented in ObjectCache.
 *
 * \tparam T    the type of object whose instances this
 *              SlabAllocator will allocate
 *
 * \tparam LockModel    either SlabNullLocker or SlabLocker
 *
 * \class SlabAllocator slaballocator.hpp kernel/slaballocator.hpp
 */
template <typename T, typename LockModel = SlabNullLocker>
    class SlabAllocator
    {
    public:
        SlabAllocator ()
        {
            ObjectCacheInit(&mObjectCache, sizeof(T));
        }

        ~SlabAllocator ()
        {
            assert(false);
        }

        /**
         * Allocate an instance of T
         *
         * \return  pointer to freshly allocated storage for a T
         *
         * \throw   std::bad_alloc if insufficient memory to fulfill
         *          request
         */
        T * AllocateWithThrow () throw (std::bad_alloc)
        {
            mLockModel.Lock();
            T * ret = static_cast<T *>(ObjectCacheAlloc(&mObjectCache));
            mLockModel.Unlock();

            if (!ret) {
                throw std::bad_alloc();
            }

            return ret;
        }

        /**
         * Allocate an instance of T
         *
         * \return  pointer to freshly allocated storage for a T,
         *          or NULL if insufficient memory to fulfill
         */
        T * Allocate ()
        {
            mLockModel.Lock();
            T * ret = static_cast<T *>(ObjectCacheAlloc(&mObjectCache));
            mLockModel.Unlock();

            return ret;
        }

        /**
         * Return memory of a T instance back to pool
         *
         * \param aInstanceOfT  object whose use is completely
         *                      finished
         */
        void Free (void * aInstanceOfT)
        {
            mLockModel.Lock();
            ObjectCacheFree(&mObjectCache, aInstanceOfT);
            mLockModel.Unlock();
        }

    private:
        ObjectCache mObjectCache;   //!< Underlying slab allocator
        LockModel   mLockModel;     //!< Locking model (see notes on LockModel)
    };

/**
 * Convenience subtype of SlabAllocator that adds automatic
 * to the main Allocate() and Free() methods. Suitable for
 * use as a global variable automatically initialized by
 * C++ startup code.
 *
 * \extends     SlabAllocator
 *
 * \tparam T    the type of object whose instances this
 *              SyncSlabAllocator will allocate
 *
 * \class SyncSlabAllocator slaballocator.hpp kernel/slaballocator.hpp
 */
template <typename T>
    class SyncSlabAllocator : public SlabAllocator<T, SlabLocker>
    {
    };

#endif

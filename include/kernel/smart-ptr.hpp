#ifndef __SMART_PTR_HPP__
#define __SMART_PTR_HPP__

#ifdef __KERNEL__
#   include <kernel/assert.h>
#else
#   include <assert.h>
#endif

/**
 * \brief   Smart pointer class for very simple automatic
 *          deallocation of objects.
 *
 * No transferrance of the pointee is possible.
 */
template <class T>
    class ScopedPtr
    {
    private:
        inline ScopedPtr (const ScopedPtr<T> & other)
        {
        }

        inline ScopedPtr<T> & operator = (const ScopedPtr<T> & other)
        {
            return *this;
        }

        inline void Release ()
        {
            if (mPointee)
            {
                delete mPointee;
                mPointee = 0;
            }
        }

        T * mPointee;

    public:
        inline ScopedPtr (T * pointee = 0)
            : mPointee(pointee)
        {
        }

        inline ~ScopedPtr ()
        {
            Release();
        }

        inline void Clear ()
        {
            Release();
        }

        inline void Reset (T * pointee)
        {
            assert(pointee != 0);

            if (pointee != mPointee) {
                Release();
                mPointee = pointee;
            }
        }

        inline ScopedPtr<T> & operator= (T * pointee)
        {
            Reset(pointee);
            return *this;
        }

        inline T * operator * ()
        {
            return mPointee;
        }

        inline T * operator -> ()
        {
            return mPointee;
        }

        inline operator bool ()
        {
            return mPointee != 0;
        }
    };

// Forward declaration to facilitate using this by reference in
// RefPtrBase
class RefCounted;

/**
 * \brief   Concrete non-template base class for RefPtr<T> that
 *          allows the destructor logic to be written without
 *          need for the template type argument of RefPtr.
 */
class RefPtrBase
{
protected:
    inline RefPtrBase ()
    {
    }

    virtual inline ~RefPtrBase ()
    {
    }

    // Prototype only because the full signature of RefCounted is
    // required to implement this method
    inline unsigned int IncrementReference (RefCounted & countee);

    // Prototype only because the full signature of RefCounted is
    // required to implement this method
    inline unsigned int DecrementReference (RefCounted & countee);
};

/**
 * \brief   Smart pointer class which supports numerous independent
 *          references to a pointee.
 *
 * Pointee will be automatically deallocated when the last RefPtr
 * reference to the pointee is dropped.
 */
template <class T>
    class RefPtr : public RefPtrBase
    {
    private:
        T * mPointee;

    protected:
        inline void Release ()
        {
            if (mPointee)
            {
                unsigned int newCount = DecrementReference(*mPointee);

                if (newCount < 1)
                {
                    delete mPointee;
                }

                mPointee = 0;
            }
        }

        inline void Acquire (T * pointee)
        {
            assert(pointee != 0);
            mPointee = pointee;
            IncrementReference(*mPointee);
        }

    public:
        inline RefPtr ()
            : mPointee(0)
        {
        }

        inline RefPtr (T * pointee)
            : mPointee(0)
        {
            Acquire(pointee);
        }

        inline RefPtr (RefPtr<T> & other)
            : mPointee(0)
        {
            Acquire(other.mPointee);
        }

        virtual inline ~RefPtr ()
        {
            Release();
        }

        inline void Reset ()
        {
            Release();
        }

        inline void Reset (T * pointee)
        {
            Release();
            Acquire(pointee);
        }

        inline void Reset (RefPtr<T> & other)
        {
            Release();
            Acquire(other.mPointee);
        }

        inline operator bool ()
        {
            return mPointee != 0;
        }

        inline T * operator * ()
        {
            return mPointee;
        }

        inline T * operator -> ()
        {
            return mPointee;
        }

        inline RefPtr<T> & operator = (RefPtr<T> & other)
        {
            if (&other == this)
            {
                return *this;
            }
            else
            {
                Release();
                Acquire(other.mPointee);
            }
        }
    };

/**
 * \brief   Base class to be inherited by any object which wishes
 *          to be a pointee of a RefPtr
 */
class RefCounted
{
friend class RefPtrBase;

private:
    unsigned int mRefCount;

protected:
    unsigned int Increment ()
    {
        return ++mRefCount;
    }

    unsigned int Decrement ()
    {
        return --mRefCount;
    }

public:
    inline RefCounted ()
        : mRefCount(0)
    {
    }

    inline ~RefCounted ()
    {
    }
};

inline unsigned int RefPtrBase::IncrementReference (RefCounted & countee)
{
    return countee.Increment();
}

inline unsigned int RefPtrBase::DecrementReference (RefCounted & countee)
{
    return countee.Decrement();
}

#endif /* __SMART_PTR_HPP__ */

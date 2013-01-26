#ifndef __SMART_PTR_HPP__
#define __SMART_PTR_HPP__

#ifdef __KERNEL__
#   include <kernel/assert.h>
#else
#   include <assert.h>
#endif

#include <kernel/list.hpp>

/**
 * \brief   Smart pointer class for very simple automatic
 *          deallocation of objects.
 *
 * No transferrance of the pointee is possible.
 *
 * \class ScopedPtr smart-ptr.hpp kernel/smart-ptr.hpp
 */
template <class T>
    class ScopedPtr
    {
    private:
        inline ScopedPtr (const ScopedPtr<T> & other)
        {
        }

        inline ScopedPtr<T> & operator= (const ScopedPtr<T> & other)
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

        inline void Reset ()
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

        inline T * operator* ()
        {
            return mPointee;
        }

        inline T * operator-> ()
        {
            return mPointee;
        }

        inline operator bool ()
        {
            return mPointee != 0;
        }
    };

class RefCounted;

/**
 * \brief   Concrete non-template base class for RefPtr<T> that
 *          allows the destructor logic to be written without
 *          need for the template type argument of RefPtr.
 *
 * \class RefPtrBase smart-ptr.hpp kernel/smart-ptr.hpp
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
 *
 * \class RefPtr smart-ptr.hpp kernel/smart-ptr.hpp
 */
template <class T>
    class RefPtr : public RefPtrBase
    {
    public:
        explicit inline RefPtr ()
            : mPointee(0)
        {
        }

        explicit inline RefPtr (T * pointee)
            : mPointee(0)
        {
            Acquire(pointee);
        }

        inline RefPtr (const RefPtr<T> & other)
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

        inline T * operator* ()
        {
            return mPointee;
        }

        inline T * operator-> ()
        {
            assert(mPointee != 0);
            return mPointee;
        }

        inline RefPtr<T> & operator= (RefPtr<T> const& other)
        {
            if (&other == this)
            {
                // Nothing to do
            }
            else if (other.mPointee != mPointee)
            {
                Release();
                Acquire(other.mPointee);
            }

            return *this;
        }

        inline RefPtr<T> & operator= (T * other)
        {
            if (other != mPointee)
            {
                Release();
                Acquire(other);
            }

            return *this;
        }

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

    private:
        T * mPointee;
    };

class WeakPointee;

/**
 * \brief   Concrete non-template base class for WeakPtr<T> that
 *          allows the destructor logic to be written without
 *          need for the template type argument of RefPtr.
 *
 * Basically, this class exists to promote type erasure and prevent
 * as much code duplication from template instantiations as possible.
 *
 * \class WeakPtrBase smart-ptr.hpp kernel/smart-ptr.hpp
 */
class WeakPtrBase
{
public:
    inline WeakPtrBase (WeakPointee * pointee);
    inline WeakPtrBase ();
    inline ~WeakPtrBase ();

protected:
    inline void Assign (WeakPointee * pointee);
    inline void Reset ();

protected:
    WeakPointee * mPointee;
    ListElement  mLink;

    friend class WeakPointee;
};

/**
 * \brief   Smart-pointer class whose pointee is automatically
 *          nulled out when the pointee is deallocated
 *
 * This class is particularly useful in situations where breaking
 * up a reference cycle is necessary.
 *
 * \class WeakPtr smart-ptr.hpp kernel/smart-ptr.hpp
 */
template <class T>
    class WeakPtr : public WeakPtrBase
    {
    public:
        inline WeakPtr ()
            : WeakPtrBase ()
        {
        }

        inline WeakPtr (T * pointee)
            : WeakPtrBase(pointee)
        {
        }

        inline ~WeakPtr ()
        {
            Reset();
        }

        inline WeakPtr<T> & operator= (T * pointee)
        {
            Reset();

            if (pointee) {
                Assign(pointee);
            }

            return *this;
        }

        inline operator bool ()
        {
            return mPointee != 0;
        }

        inline T * operator* ()
        {
            return static_cast<T *>(mPointee);
        }

        inline T * operator-> ()
        {
            return static_cast<T *>(mPointee);
        }
    };

/**
 * \brief   Base type to be inherited by objects that want to
 *          be referenceable by WeakPtr's.
 *
 * \class WeakPointee smart-ptr.hpp kernel/smart-ptr.hpp
 */
class WeakPointee
{
friend class WeakPtrBase;

public:
    typedef List<WeakPtrBase, &WeakPtrBase::mLink> WeakPtrBaseList;

private:
    WeakPtrBaseList mWeakRefs;

public:
    inline virtual ~WeakPointee ()
    {
        for (WeakPtrBaseList::Iter i = mWeakRefs.Begin(); i; ++i) {
            i->Reset();
        }
        assert(mWeakRefs.Empty());
    }
};

/**
 * \brief   Base class to be inherited by any object which wishes
 *          to be a pointee of a RefPtr
 *
 * \class RefCounted smart-ptr.hpp kernel/smart-ptr.hpp
 */
class RefCounted
{
public:
    inline RefCounted ()
        : mRefCount(0)
    {
    }

    virtual inline ~RefCounted ()
    {
    }

    /**
     * \brief   Normally for use by RefPtrBase, but can also be
     *          used to manually implement reference-counting.
     *
     * \return  The the number of references after the increment
     *          finishes.
     */
    unsigned int Ref ()
    {
        return ++mRefCount;
    }

    /**
     * \brief   Normally for use by RefPtrBase, but can also be
     *          used to manually implement reference-counting.
     *
     * \return  The the number of references after the decrement
     *          finishes.
     */
    unsigned int Unref ()
    {
        return --mRefCount;
    }

private:
    unsigned int mRefCount;
};

inline unsigned int RefPtrBase::IncrementReference (RefCounted & countee)
{
    return countee.Ref();
}

inline unsigned int RefPtrBase::DecrementReference (RefCounted & countee)
{
    return countee.Unref();
}

inline WeakPtrBase::WeakPtrBase ()
    : mPointee(0)
{
}

inline WeakPtrBase::WeakPtrBase (WeakPointee * pointee)
{
    Assign(pointee);
}

inline WeakPtrBase::~WeakPtrBase ()
{
    Reset();
}

inline void WeakPtrBase::Assign (WeakPointee * pointee)
{
    assert(pointee != 0);
    mPointee = pointee;
    mPointee->mWeakRefs.Append(this);
}

inline void WeakPtrBase::Reset ()
{
    if (mPointee)
    {
        List<WeakPtrBase, &WeakPtrBase::mLink>::Remove(this);
        mPointee = 0;
    }
}

#endif /* __SMART_PTR_HPP__ */

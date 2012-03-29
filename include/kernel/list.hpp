#ifndef __LIST_HPP__
#define __LIST_HPP__

#ifdef __KERNEL__
#   include <kernel/assert.h>
#else
#   include <assert.h>
#endif

#include <stdint.h>

/**
 * \brief   Datatype to be embedded into any object which wants to
 *          be insertable into an intrusive doubly-linked list.
 *
 * ListElement instances should live directly in the storage of
 * the containing object, and should be treated as an opaque
 * type.
 */
class ListElement
{
public:
    ListElement ()
        : prev(this)
        , next(this)
    {
    }

    void DynamicInit ()
    {
        prev = next = this;
    }

    bool Unlinked ()
    {
        return prev == this && next == this;
    }

    ListElement * prev;
    ListElement * next;
};

/**
 * \brief   Typesafe doubly-linked intrusive list.
 *
 * Suggested usage:
 *
 * \code
 *
 * class Apple {
 *   public:
 *     ListElement node;
 * };
 *
 * List<Apple, &Apple::node> list;
 *
 * Apple golden;
 * Apple red;
 *
 * list.Append(&golden);
 * list.Prepend(&red);
 *
 * \endcode
 */
template <class T, ListElement T::* Ptr>
    class List
    {
    public:

        /**
         * \brief Delete-safe iterator used to traverse Lists over T
         *
         * Usage:
         *
         * \code
         *
         * List<Foo, &Foo::node> list;
         * ...
         *
         * for (List<Foo>::Iterator i = list.Begin(); i; i++) {
         *     Foo * element = *i;
         *     ...;
         *     if (someThing) {
         *         list.Remove(element);
         *     }
         * }
         *
         * \endcode
         */
        class Iterator
        {
        friend class List;

        private:
            Iterator (List<T, Ptr> * list, ListElement * elem)
                : mList(list)
                , mElem(elem)
            {
                mNextElem = mElem->next;
            }

            void Advance ()
            {
                mElem = mNextElem;
                mNextElem = mElem->next;
            }

        private:
            List<T, Ptr> *  mList;
            ListElement *   mElem;
            ListElement *   mNextElem;

        public:
            /**
             * \brief   Pointer-notation operator. Returns the element
             *          that this iterator instance positionally refers
             *          to.
             */
            T * operator -> ()
            {
                return mList->elemFromHead(mElem);
            }

            /**
             * \brief   Pointer-dereference operator. Returns the element
             *          that this iterator instance positionally refers
             *          to.
             */
            T * operator * ()
            {
                return mList->elemFromHead(mElem);
            }

            /**
             * \brief   Test whether the iterator points to a valid element
             */
            operator bool ()
            {
                return mElem != &mList->mHead;
            }

            /**
             * \brief   Post-increment operator
             */
            Iterator operator ++ (int dummy)
            {
                Iterator prevalue = *this;
                Advance();
                return prevalue;
            }

            /**
             * \brief   Pre-increment operator
             */
            Iterator operator ++ ()
            {
                Advance();
                return *this;
            }
        };

        typedef Iterator Iter;

        List ()
        {
        }

        ~List ()
        {
            assert(Empty());
        }

        /**
         * \brief   Perform explicit run-time initialization of
         *          a List instance, exactly like a constructor
         *          would.
         */
        void DynamicInit () {
            mHead.DynamicInit();
        }

        /**
         * \brief   Fetch an iterator positioned initially at the
         *          first element of this List.
         */
        Iterator Begin () {
            return Iterator(this, mHead.next);
        }

        /**
         * \brief   Test whether any elements are inserted into this List
         */
        bool Empty () {
            return &mHead == mHead.next;
        }

        /**
         * \brief   Insert an element at the beginning of this list
         */
        void Prepend (T * element) {
            ListElement * elementHead = &(element->*Ptr);
            elementHead->prev = &mHead;
            elementHead->next = mHead.next;
            mHead.next->prev = elementHead;
            mHead.next = elementHead;
        }

        /**
         * \brief   Insert an element at the end of this list
         */
        void Append (T * element) {
            ListElement * elementHead = &(element->*Ptr);
            elementHead->prev = mHead.prev;
            elementHead->next = &mHead;
            mHead.prev->next = elementHead;
            mHead.prev = elementHead;
        }

        /**
         * \brief   Remove an element from this list
         */
        static void Remove (T * element) {
            ListElement * elementHead = &(element->*Ptr);
            elementHead->prev->next = elementHead->next;
            elementHead->next->prev = elementHead->prev;
            elementHead->next = elementHead->prev = elementHead;
        }

        /**
         * \brief   Fetch the first element in this list
         */
        T * First () {
            return Empty() ? 0 : elemFromHead(mHead.next);
        }

        /**
         * \brief   Fetch the last element in this list
         */
        T * Last () {
            return Empty() ? 0 : elemFromHead(mHead.prev);
        }

        /**
         * \brief   Fetch and remove the first element in this list
         */
        T * PopFirst () {
            T * ret = elemFromHead(mHead.next);
            Remove(ret);
            return ret;
        }

        /**
         *  \brief  Fetch and remove the last element in this list
         */
        T * PopLast () {
            T * ret = elemFromHead(mHead.prev);
            Remove(ret);
            return ret;
        }

        /**
         * \brief   Fetch the element of the list consecutively
         *          subsequent to the argument
         */
        T * Next (T * element) {
            ListElement * elementHead = &(element->*Ptr);
            return elemFromHead(elementHead->next);
        }

        /**
         * \brief   Fetch the element of the list consecutively
         *          previous to the argument
         */
        T * Prev (T * element) {
            ListElement * elementHead = &(element->*Ptr);
            return elemFromHead(elementHead->prev);
        }

    private:
        /**
         * \brief   Sentintel node that holds the head and tail
         *          pointers.
         */
        ListElement         mHead;

        /**
         * \brief   Do pointer magic to determine the element which
         *          contains head. Head *MUST* be currently linked
         *          into this list.
         */
        T * elemFromHead (ListElement * head) {

            ListElement * zeroHead = &(static_cast<T*>(0)->*Ptr);
            uintptr_t offset = reinterpret_cast<uintptr_t>(zeroHead);

            T * result = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(head) - offset);
            return result;
        }
    };

#endif /* __LIST_HPP__ */

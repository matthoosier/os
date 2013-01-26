#ifndef __REF_LIST_HPP__
#define __REF_LIST_HPP__

#include <stdint.h>

#include <kernel/list.hpp>
#include <kernel/smart-ptr.hpp>

/**
 * \brief   Typesafe doubly-linked intrusive list over elements
 *          referred to by reference-counting pointers.
 *
 * Suggested usage:
 *
 * \code
 *
 * class Apple : public RefCounted {
 *   public:
 *     ListElement node;
 * };
 *
 * RefList<Apple, &Apple::node> list;
 *
 * RefPtr<Apple> golden(new Apple);
 * RefPtr<Apple> red(new Apple);
 *
 * list.Append(golden);
 * list.Prepend(red);
 *
 * \endcode
 *
 * \class RefList ref-list.hpp kernel/ref-list.hpp
 */
template <class T, ListElement T::* Ptr>
    class RefList
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
         *     RefPtr<Foo> element = *i;
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
        friend class RefList;

        private:
            Iterator (RefList<T, Ptr> * list, ListElement * elem)
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
            RefList<T, Ptr> *   mList;
            ListElement *       mElem;
            ListElement *       mNextElem;

        public:
            /**
             * \brief   Pointer-notation operator. Returns the element
             *          that this iterator instance positionally refers
             *          to.
             */
            RefPtr<T> operator -> ()
            {
                return RefPtr<T>(mList->elemFromHead(mElem));
            }

            /**
             * \brief   Pointer-dereference operator. Returns the element
             *          that this iterator instance positionally refers
             *          to.
             */
            RefPtr<T> operator * ()
            {
                return RefPtr<T>(mList->elemFromHead(mElem));
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

        RefList ()
        {
        }

        ~RefList ()
        {
            assert(Empty());
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
            assert((&mHead == mHead.next) == (&mHead == mHead.prev));
            return &mHead == mHead.next;
        }

        /**
         * \brief   Insert an element at the beginning of this list
         */
        void Prepend (RefPtr<T> element) {
            T * rawElement = *element;
            ListElement * elementHead = &(rawElement->*Ptr);
            elementHead->prev = &mHead;
            elementHead->next = mHead.next;
            mHead.next->prev = elementHead;
            mHead.next = elementHead;

            element->Ref();
        }

        /**
         * \brief   Insert an element at the end of this list
         */
        void Append (RefPtr<T> element) {
            T * rawElement = *element;
            ListElement * elementHead = &(rawElement->*Ptr);
            elementHead->prev = mHead.prev;
            elementHead->next = &mHead;
            mHead.prev->next = elementHead;
            mHead.prev = elementHead;

            element->Ref();
        }

        /**
         * \brief   Remove an element from this list
         */
        static void Remove (RefPtr<T> element) {
            T * rawElement = *element;
            ListElement * elementHead = &(rawElement->*Ptr);
            elementHead->prev->next = elementHead->next;
            elementHead->next->prev = elementHead->prev;
            elementHead->next = elementHead->prev = elementHead;

            element->Unref();
        }

        /**
         * \brief   Fetch the first element in this list
         */
        RefPtr<T> First () {
            RefPtr<T> ret;

            if (!Empty()) {
                ret.Reset(elemFromHead(mHead.next));
            }

            return ret;
        }

        /**
         * \brief   Fetch the last element in this list
         */
        RefPtr<T> Last () {
            RefPtr<T> ret;

            if (!Empty()) {
                ret.Reset(elemFromHead(mHead.prev));
            }

            return ret;
        }

        /**
         * \brief   Fetch and remove the first element in this list
         */
        RefPtr<T> PopFirst () {
            RefPtr<T> ret(elemFromHead(mHead.next));
            Remove(ret);
            return ret;
        }

        /**
         *  \brief  Fetch and remove the last element in this list
         */
        RefPtr<T> PopLast () {
            RefPtr<T> ret(elemFromHead(mHead.prev));
            Remove(ret);
            return ret;
        }

        /**
         * \brief   Fetch the element of the list consecutively
         *          subsequent to the argument
         */
        RefPtr<T> Next (RefPtr<T> element) {
            T * rawElement = *element;
            ListElement * elementHead = &(rawElement->*Ptr);
            return RefPtr<T>(elemFromHead(elementHead->next));
        }

        /**
         * \brief   Fetch the element of the list consecutively
         *          previous to the argument
         */
        RefPtr<T> Prev (RefPtr<T> element) {
            T * rawElement = *element;
            ListElement * elementHead = &(rawElement->*Ptr);
            return RefPtr<T>(elemFromHead(elementHead->prev));
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

#endif /* __REF_LIST_HPP__ */

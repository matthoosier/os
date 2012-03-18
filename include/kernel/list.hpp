#ifndef __LIST_HPP__
#define __LIST_HPP__

#include <assert.h>

template <class T>
    class ListElement
    {
    public:
        ListElement()
            : prev(0)
            , next(0)
        {
        }

        T * prev;
        T * next;
    };

template <class T>
    class List
    {
    public:

        /**
         * Delete-safe iterator. Suggested use:
         *
         * List<Foo> list;
         * ...
         *
         * for (List<Foo>::Iterator i = list.Begin(); i; i++) {
         *     Foo * element = *i;
         *     ...;
         *     if (someThing) {
         *         list.Remove(element);
         *     }
         * }
         */
        class Iterator
        {
        friend class List;

        private:
            Iterator (List<T> * list, T * elem)
                : mList(list)
                , mElem(elem)
            {
                mNextElem = NextFromElement(mElem);
            }

            T * NextFromElement (T * element)
            {
                return element ? mList->Next(element) : 0;
            }

            void Advance ()
            {
                mElem = mNextElem;
                mNextElem = NextFromElement(mElem);
            }

        private:
            List<T> *   mList;
            T *         mElem;
            T *         mNextElem;

        public:
            T * operator -> ()
            {
                return mElem;
            }

            T * operator * ()
            {
                return mElem;
            }

            operator bool ()
            {
                return mElem != 0;
            }

            Iterator operator ++ (int dummy)
            {
                Iterator prevalue = *this;
                Advance();
                return prevalue;
            }

            Iterator operator ++ ()
            {
                Advance();
                return *this;
            }
        };

        List (ListElement<T> T::* links)
            : head(0)
            , tail(0)
            , ptrLinks(links)
        {
        }

        ~List ()
        {
            assert(Empty());
        }

        Iterator Begin ()
        {
            return Iterator(this, head);
        }

        bool Empty ()
        {
            assert((head && tail) || (!head && !tail));

            return !head;
        }

        void Prepend (T * element)
        {
            ListElement<T> * elementLinks = &(element->*ptrLinks);

            if (Empty()) {
                elementLinks->prev = elementLinks->next = 0;
                head = tail = element;
            }
            else {
                ListElement<T> * headLinks = &(head->*ptrLinks);
                elementLinks->next = head;
                headLinks->prev = element;
                head = element;
            }
        }

        void Append (T * element) {
            ListElement<T> * elementLinks = &(element->*ptrLinks);

            if (Empty()) {
                elementLinks->prev = elementLinks->next = 0;
                head = tail = element;
            }
            else {
                ListElement<T> * tailLinks = &(tail->*ptrLinks);
                elementLinks->prev = tail;
                tailLinks->next = element;
                tail = element;
            }
        }

        void Remove (T * element) {
            ListElement<T> * elementLinks = &(element->*ptrLinks);

            assert(!Empty());

            if (head == element && tail == element) {
                head = tail = 0;
            }
            else if (head == element) {
                head = elementLinks->next;
                ListElement<T> * headLinks = &(head->*ptrLinks);
                headLinks->prev = 0;
            }
            else if (tail == element) {
                tail = elementLinks->prev;
                ListElement<T> * tailLinks = &(tail->*ptrLinks);
                tailLinks->next = 0;
            }
            else {
                ListElement<T> * prevLinks = &(elementLinks->prev->*ptrLinks);
                ListElement<T> * nextLinks = &(elementLinks->next->*ptrLinks);
                prevLinks->next = elementLinks->next;
                nextLinks->prev = elementLinks->prev;
            }

            elementLinks->prev = elementLinks->next = 0;
        }

        T * First () {
            return head;
        }

        T * Last () {
            return tail;
        }

        T * Next (T * element) {
            ListElement<T> * elementLinks = &(element->*ptrLinks);
            return elementLinks->next;
        }

        T * Prev (T * element) {
            ListElement<T> * elementLinks = &(element->*ptrLinks);
            return elementLinks->prev;
        }

    private:
        T * head;
        T * tail;
        ListElement<T> T::* ptrLinks;
    };

#endif /* __LIST_HPP__ */

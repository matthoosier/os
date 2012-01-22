#ifndef __LIST_H__
#define __LIST_H__

/*! \file kernel/list.h */

#include <stddef.h>

#include <sys/decls.h>

BEGIN_DECLS

/**
 * \brief   The basic pair of links which are embedded inside a
 *          data structure in order to make it eligible to be
 *          contained inside a linked list.
 */
struct list_head
{
    struct list_head * prev;
    struct list_head * next;
};

/**
 * \brief   Evaluates to an r-value suitable for assigning
 *          to a #list_head variable named \em name.
 *
 * \param name  the identifier of the #list_head instance
 *              this expression will be assigned to
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * \brief   Dynamically initialize a linked list head
 */
static inline void INIT_LIST_HEAD (struct list_head *list)
{
    list->prev = list;
    list->next = list;
}

/**
 * \brief   Declare and initialize a #list_head instance named
 *          \em name
 *
 * \param name  The identifier by which the declared #list_head
 *              shall be known
 */
#define LIST_HEAD(name) \
        struct list_head name = LIST_HEAD_INIT(name)

/**
 * \brief   Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 *
 * \memberof list_head
 */
static inline void __list_add (struct list_head *new_elem,
                               struct list_head *prev,
                               struct list_head *next)
{
    next->prev = new_elem;
    new_elem->next = next;
    new_elem->prev = prev;
    prev->next = new_elem;
}

/**
 * \brief  Delete a list entry by making the prev/next entries
 *         point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 *
 * \memberof list_head
 */
static inline void __list_del (struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * \brief       add a new entry
 *
 * \param new_elem  new entry to be added
 * \param head      list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 *
 * \memberof list_head
 */
static inline void list_add (struct list_head *new_elem, struct list_head *head)
{
    __list_add(new_elem, head, head->next);
}

/**
 * \brief       add a new entry
 *
 * \param new_elem  new entry to be added
 * \param head      list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.

 * \memberof list_head
 */
static inline void list_add_tail (struct list_head *new_elem, struct list_head *head)
{
    __list_add(new_elem, head->prev, head);
}

/**
 * \brief       deletes entry from list
 *
 * \param entry the element to delete from the list
 *
 * Note: list_empty on entry does not return true after this, the entry
 * is in an undefined state.
 *
 * \memberof list_head
 */
static inline void list_del (struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

/**
 * \brief       deletes entry from list and reinitializes it.
 *
 * \param entry the element to delete from the list
 *
 * \memberof list_head
 */
static inline void list_del_init (struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/**
 * \brief       delete from one list and add as another's head
 *
 * \param list  the entry to move
 * \param head  the head that will precede our entry, on the new list
 *
 * \memberof list_head
 */
static inline void list_move (struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/**
 * \brief       tests whether @list is the last entry in the list @head
 *
 * \param list  the entry to test
 * \param head  the head of the list
 *
 * \memberof list_head
 */
static inline int list_is_last (const struct list_head *list,
                                const struct list_head *head)
{
    return list->next == head;
}

/**
 * \brief       tests whether a list is empty
 *
 * \param head  the list to test
 *
 * \memberof list_head
 */
static inline int list_empty (const struct list_head *head)
{
    return head->next == head;
}

/**
 * \brief   iterate over a list
 *
 * \param pos       the pointer-to-#list_head to use as a loop counter
 * \param head      pointer to the head for your list
 */
#define list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * \brief   iterate over a list safe against removal of entries
 *
 * \param pos           the pointer-to-#list_head to use as a loop cursor
 * \param n             another pointer-to-#list_head to use as temp storage
 * \param head          pointer to the head for your list
 */
#define list_for_each_safe(pos, n, head)                        \
        for (pos = (head)->next, n = pos->next; pos != (head);  \
            pos = n, n = pos->next)

/**
 * \brief   get the struct for this entry
 *
 * \param ptr       the #list_head pointer
 * \param type      the type of the struct this is embedded in
 * \param member    the name of the #list_head within the struct.
 */
#define list_entry(ptr, type, member) \
        container_of(ptr, type, member)

/**
 * \brief   get the first element from a list
 *
 * \param ptr           the #list_head to take the element from
 * \param type          the type of the struct this is embedded in.
 * \param member        the name of the #list_head within the struct
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * \brief   cast a member of a structure out to the containing structure
 *
 * \param ptr       the pointer to the member
 * \param type      the type of the container struct this is embedded in
 * \param member    the name of the member within the struct
 */
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

/**
 * \brief   iterate over list of given type
 *
 * \param pos               the type * to use as a loop counter
 * \param head              the head for your list
 * \param member            the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)                      \
    for (pos = list_entry((head)->next, typeof(*pos), member);      \
        &pos->member != (head);                                     \
        pos = list_entry(pos->member.next, typeof(*pos), member))

END_DECLS

#endif /* __LIST_H__ */

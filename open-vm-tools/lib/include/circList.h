/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 *   circList.h --
 *
 * macros, prototypes and struct definitions for double-linked
 * circular lists.
 */

#ifndef _CIRCLIST_H_
#define _CIRCLIST_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"
#include "vmware.h"

typedef struct ListItem {
   struct ListItem *prev;
   struct ListItem *next;
} ListItem;

/* A list with no elements is a null pointer. */
#define   LIST_ITEM_DEF(name)   \
   ListItem * name = NULL

#define   LIST_EMPTY(l)      ((l) == NULL)

/* initialize list item */
#define   INIT_LIST_ITEM(p)   \
   do {   \
      (p)->prev = (p)->next = (p);   \
   } while (0)

/* check if initialized */
#define   IS_LIST_ITEM_INITIALIZED(li)   \
   (((li) == (li)->prev) && ((li) == (li)->next))

/* return first element in the list */
#define   LIST_FIRST(l)      (l)
#define   LIST_FIRST_CHK(l)   (l)

/* return last element in the list */
#define   LIST_LAST(l)      ((l)->prev)
#define   LIST_LAST_CHK(l)   (LIST_EMPTY(l) ? NULL : LIST_LAST(l))

/*
 * LIST_CONTAINER - get the struct for this entry (like list_entry)
 * @ptr: the &struct ListItem pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list struct within the struct.
 */
#define LIST_CONTAINER(ptr, type, member) \
   ((type *)((char *)(ptr) - offsetof(type, member)))

/*
 * delete item from the list
 */
#define   LIST_DEL            DelListItem

/*
 * link two lists together
 */
#define   LIST_SPLICE         SpliceLists

/*
 * Split a list into two lists
 */
#define   LIST_SPLIT          SplitLists

/*
 * Add item to front of stack. List pointer points to new head.
 */
#define   LIST_PUSH           PushListItem

/*
 * Add item at back of queue. List pointer only changes if list was empty.
 */
#define   LIST_QUEUE          QueueListItem

/*
 * Get the list size.
 */
#define   LIST_SIZE           GetListSize

/*
 * LIST_SCAN_FROM scans the list from "from" up until "until".
 * The loop variable p should not be destroyed in the process.
 * "from" is an element in the list where to start scanning.
 * "until" is the element where search should stop.
 * member is the field to use for the search - either "next" or "prev".
 */
#define   LIST_SCAN_FROM(p, from, until, member)   \
   for (p = (from); (p) != NULL;   \
      (p) = (((p)->member == (until)) ? NULL : (p)->member))

/* scan the entire list (non-destructively) */
#define   LIST_SCAN(p, l)   \
   LIST_SCAN_FROM(p, LIST_FIRST(l), LIST_FIRST(l), next)


/* scan a list backward from last element to first (non-destructively) */
#define   LIST_SCAN_BACK(p, l)   \
   LIST_SCAN_FROM(p, LIST_LAST_CHK(l), LIST_LAST(l), prev)

/* scan the entire list where loop element may be destroyed */
#define   LIST_SCAN_SAFE(p, pn, l)   \
   if (!LIST_EMPTY(l))  \
      for (p = (l), (pn) = NextListItem(p, l); (p) != NULL;   \
           (p) = (pn), (pn) = NextListItem(p, l))

/* scan the entire list backwards where loop element may be destroyed */
#define   LIST_SCAN_BACK_SAFE(p, pn, l)   \
   if (!LIST_EMPTY(l))  \
      for (p = LIST_LAST(l), (pn) = PrevListItem(p, l); (p) != NULL;   \
           (p) = (pn), (pn) = PrevListItem(p, l))


/* function definitions */

/*
 *----------------------------------------------------------------------
 *
 * NextListItem --
 *
 *      Returns the next member of a doubly linked list, or NULL if last.
 *      Assumes: p is member of the list headed by head.
 *
 * Result:
 *      If head or p is NULL, return NULL. Otherwise,
 *      next list member (or null if last).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE ListItem *
NextListItem(ListItem *p,        // IN
             ListItem *head)     // IN
{
   if (head == NULL || p == NULL) {
      return NULL;
   }
   /* both p and head are non-null */
   p = p->next;
   return p == head ? NULL : p;
}


/*
 *----------------------------------------------------------------------
 *
 * PrevListItem --
 *
 *      Returns the prev member of a doubly linked list, or NULL if first.
 *      Assumes: p is member of the list headed by head.
 *
 * Result:
 *      If head or prev is NULL, return NULL. Otherwise,
 *      prev list member (or null if first).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE ListItem *
PrevListItem(ListItem *p,        // IN
             ListItem *head)     // IN
{
   if (head == NULL || p == NULL) {
      return NULL;
   }
   /* both p and head are non-null */
   return p == head ? NULL : p->prev;
}


/*
 *----------------------------------------------------------------------
 *
 * DelListItem --
 *
 *      Deletes a member of a doubly linked list, possibly modifies the
 *      list header itself.
 *      Assumes neither p nor headp is null and p is a member of *headp.
 *
 * Result:
 *      None
 *
 * Side effects:
 *      Modifies *headp.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DelListItem(ListItem *p,         // IN
            ListItem **headp)    // IN/OUT
{
   ListItem *next;

   ASSERT(p);
   ASSERT(headp);

   next = p->next;
   if (p == next) {
      *headp = NULL;
   } else {
      next->prev = p->prev;
      p->prev->next = next;
      if (*headp == p) {
         *headp = next;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * QueueListItem --
 *
 *      Adds a new member to the back of a doubly linked list (queue)
 *      Assumes neither p nor headp is null and p is not a member of *headp.
 *
 * Result:
 *      None
 *
 * Side effects:
 *      Modifies *headp.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
QueueListItem(ListItem *p,              // IN
              ListItem **headp)         // IN/OUT
{
   ListItem *head;

   head = *headp;
   if (LIST_EMPTY(head)) {
      INIT_LIST_ITEM(p);
      *headp = p;
   } else {
      p->prev = head->prev;
      p->next = head;
      p->prev->next = p;
      head->prev = p;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * PushListItem --
 *
 *      Adds a new member to the front of a doubly linked list (stack)
 *      Assumes neither p nor headp is null and p is not a member of *headp.
 *
 * Result:
 *      None
 *
 * Side effects:
 *      Modifies *headp.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
PushListItem(ListItem *p,               // IN
             ListItem **headp)          // IN/OUT
{
   QueueListItem(p, headp);
   *headp = p;
}


/*
 *----------------------------------------------------------------------
 *
 * SpliceLists --
 *
 *      Make a single list {l1 l2} from {l1} and {l2} and return it.
 *      It is okay for one or both lists to be NULL.
 *      No checking is done. It is assumed that l1 and l2 are two
 *      distinct lists.
 *
 * Result:
 *      A list { l1 l2 }.
 *
 * Side effects:
 *      Modifies l1 and l2 list pointers.
 *
 *----------------------------------------------------------------------
 */

static INLINE ListItem *
SpliceLists(ListItem *l1,      // IN
            ListItem *l2)      // IN
{
   ListItem *l1Last, *l2Last;

   if (LIST_EMPTY(l1)) {
      return l2;
   }

   if (LIST_EMPTY(l2)) {
      return l1;
   }

   l1Last = l1->prev;   /* last elem of l1 */
   l2Last = l2->prev;   /* last elem of l2 */

   /*
    *    l1 -> ... -> l1Last    l2 -> ... l2Last
    */
   l1Last->next = l2;
   l2->prev = l1Last;

   l1->prev = l2Last;
   l2Last->next = l1;

   return l1;
}


/*
 *----------------------------------------------------------------------
 *
 * SplitLists --
 *
 *      Make a list l = {l1 l2} into two separate lists {l1} and {l2}, where:
 *      l = { ... x -> p -> ... } split into:
 *      l1 = { ... -> x }
 *      l2 = { p -> ... }
 *      Assumes neither p nor l is null and p is a member of l.
 *      If p is the first element of l, then l1 will be NULL.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      Sets *l1p and *l2p to the resulting two lists.
 *      Modifies l's pointers.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
SplitLists(ListItem *p,         // IN
           ListItem *l,         // IN
           ListItem **l1p,      // OUT
           ListItem **l2p)      // OUT
{
   ListItem *last;

   if (p == LIST_FIRST(l)) {   /* first element */
      *l1p = NULL;
      *l2p = l;
      return;
   }

   last = l->prev;

   *l1p = l;
   p->prev->next = l;
   l->prev = p->prev;

   *l2p = p;
   p->prev = last;
   last->next = p;
}


/*
 *----------------------------------------------------------------------
 *
 * GetListSize --
 *
 *	Return the number of items in the list.
 *
 * Result:
 *	The number of items in the list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
GetListSize(ListItem *head)     // IN
{
   ListItem *li;
   int ret = 0;

   LIST_SCAN(li, head) {
      ret++;
   }
   return ret;
}

#endif /* _CIRCLIST_H_ */

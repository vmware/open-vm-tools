/*********************************************************
 * Copyright (C) 1998-2017,2019 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ListItem {
   struct ListItem *prev;
   struct ListItem *next;
} ListItem;


/*
 *----------------------------------------------------------------------
 *
 * CircList_IsEmpty --
 *
 *      A NULL list is an empty list.
 *
 * Result:
 *      TRUE if list is empty, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
CircList_IsEmpty(const ListItem *item)  // IN
{
   return item == NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * CircList_InitItem --
 *
 *      Initialize item as a single-element circular list.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
CircList_InitItem(ListItem *item)  // OUT
{
   item->prev = item->next = item;
}


/*
 *----------------------------------------------------------------------
 *
 * CircList_First --
 *
 *      Return first item in the list.
 *
 * Result:
 *      First item.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE ListItem *
CircList_First(ListItem *item)  // IN
{
   return item;
}


/*
 *----------------------------------------------------------------------
 *
 * CircList_Last --
 *
 *      Return last item in the list.
 *
 * Result:
 *      Last item.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE ListItem *
CircList_Last(ListItem *item)
{
   return item->prev;
}


/*
 * CIRC_LIST_CONTAINER - get the struct for this entry (like list_entry)
 * @ptr: the &struct ListItem pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list struct within the struct.
 */
#define CIRC_LIST_CONTAINER(ptr, type, member) \
   VMW_CONTAINER_OF(ptr, type, member)
/* 
 * Historical name, left here to reduce churn.
 * TODO: remove, all LIST_CONTAINER uses should be
 * VMW_CONTAINER_OF and stop depending on circList.h
 * to provide the definition.
 */
#define LIST_CONTAINER(ptr, type, member) VMW_CONTAINER_OF(ptr, type, member)

/*
 * LIST_SCAN_FROM scans the list from "from" up until "until".
 * The loop variable p should not be destroyed in the process.
 * "from" is an element in the list where to start scanning.
 * "until" is the element where search should stop.
 * member is the field to use for the search - either "next" or "prev".
 */
#define CIRC_LIST_SCAN_FROM(p, from, until, member)   \
   for (p = (from); (p) != NULL;   \
      (p) = (((p)->member == (until)) ? NULL : (p)->member))

/* scan the entire list (non-destructively) */
#define CIRC_LIST_SCAN(p, l)   \
   CIRC_LIST_SCAN_FROM(p, CircList_First(l), CircList_First(l), next)


/* scan the entire list where loop element may be destroyed */
#define CIRC_LIST_SCAN_SAFE(p, pn, l)   \
   if (!CircList_IsEmpty(l))  \
      for (p = (l), (pn) = CircList_Next(p, l); (p) != NULL;   \
           (p) = (pn), (pn) = CircList_Next(p, l))

/* scan the entire list backwards where loop element may be destroyed */
#define CIRC_LIST_SCAN_BACK_SAFE(p, pn, l)   \
   if (!CircList_IsEmpty(l))  \
      for (p = CircList_Last(l), (pn) = CircList_Prev(p, l); (p) != NULL;   \
           (p) = (pn), (pn) = CircList_Prev(p, l))


/*
 *----------------------------------------------------------------------
 *
 * CircList_Next --
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
CircList_Next(ListItem *p,        // IN
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
 * CircList_Prev --
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
CircList_Prev(ListItem *p,        // IN
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
 * CircList_DeleteItem --
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
CircList_DeleteItem(ListItem *p,         // IN
                    ListItem **headp)    // IN/OUT
{
   ListItem *next;

   ASSERT(p != NULL);
   ASSERT(headp != NULL);

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
 * CircList_Queue --
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
CircList_Queue(ListItem *p,              // IN
               ListItem **headp)         // IN/OUT
{
   ListItem *head;

   head = *headp;
   if (CircList_IsEmpty(head)) {
      CircList_InitItem(p);
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
 * CircList_Push --
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
CircList_Push(ListItem *p,               // IN
              ListItem **headp)          // IN/OUT
{
   CircList_Queue(p, headp);
   *headp = p;
}


/*
 *----------------------------------------------------------------------
 *
 * CircList_InsertAfter --
 *
 *      Adds a new member to the list after the provided item.  Assumes p
 *      is not a member of a list already.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
CircList_InsertAfter(ListItem *p,      // IN:
                     ListItem *after)  // IN:
{
   p->prev = after;
   p->next = after->next;
   p->next->prev = p;
   after->next = p;
}


/*
 *----------------------------------------------------------------------
 *
 * CircList_Splice --
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
CircList_Splice(ListItem *l1,      // IN
                ListItem *l2)      // IN
{
   ListItem *l1Last, *l2Last;

   if (CircList_IsEmpty(l1)) {
      return l2;
   }

   if (CircList_IsEmpty(l2)) {
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


#if 0  /* Presently unused, enable if a use is found */
/*
 *----------------------------------------------------------------------
 *
 * CircList_Split --
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
CircList_Split(ListItem *p,         // IN
               ListItem *l,         // IN
               ListItem **l1p,      // OUT
               ListItem **l2p)      // OUT
{
   ListItem *last;

   if (p == CircList_First(l)) {   /* first element */
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
#endif


/*
 *----------------------------------------------------------------------
 *
 * CircList_Size --
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
CircList_Size(ListItem *head)     // IN
{
   ListItem *li;
   int ret = 0;

   CIRC_LIST_SCAN(li, head) {
      ret++;
   }
   return ret;
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _CIRCLIST_H_ */

/*********************************************************
 * Copyright (C) 1998-2017,2020 VMware, Inc. All rights reserved.
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

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * dbllnklst.h --
 *
 *    Double linked lists
 *
 *    Both circular and anchored (linear) lists are supported.
 *    See bora/lib/misc/dbllnklst.c for sample code showing both use cases.
 */

#ifndef _DBLLNKLST_H_
#define _DBLLNKLST_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define DblLnkLst_OffsetOf(type, field) ((intptr_t)&((type *)0)->field)

#define DblLnkLst_Container(addr, type, field) \
   ((type *)((char *)(addr) - DblLnkLst_OffsetOf(type, field)))

#define DblLnkLst_ForEach(curr, head)                   \
      for (curr = (head)->next; curr != (head); curr = (curr)->next)

/* Safe from list element removal within loop body. */
#define DblLnkLst_ForEachSafe(curr, nextElem, head)             \
      for (curr = (head)->next, nextElem = (curr)->next;        \
           curr != (head);                                      \
           curr = nextElem, nextElem = (curr)->next)

typedef struct DblLnkLst_Links {
   struct DblLnkLst_Links *prev;
   struct DblLnkLst_Links *next;
} DblLnkLst_Links;


/*
 * Functions
 *
 * DblLnkLst_LinkFirst, DblLnkLst_LinkLast, and DblLnkLst_Swap are specific
 * to anchored lists.  The rest are for both circular and anchored lists.
 */


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_Init --
 *
 *    Initialize a member of a doubly linked list
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_Init(DblLnkLst_Links *l) // OUT
{
   l->prev = l->next = l;
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_Link --
 *
 *    Merge two doubly linked lists into one
 *
 *    The operation is commutative
 *    The operation is inversible (its inverse is DblLnkLst_Unlink)
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_Link(DblLnkLst_Links *l1, // IN/OUT
               DblLnkLst_Links *l2) // IN/OUT
{
   DblLnkLst_Links *tmp;

   (tmp      = l1->prev)->next = l2;
   (l1->prev = l2->prev)->next = l1;
    l2->prev = tmp                 ;
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_Unlink --
 *
 *    Split one doubly linked list into two
 *
 *    No check is performed: the caller must ensure that both members
 *    belong to the same doubly linked list
 *
 *    The operation is commutative
 *    The operation is inversible (its inverse is DblLnkLst_Link)
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_Unlink(DblLnkLst_Links *l1, // IN/OUT
                 DblLnkLst_Links *l2) // IN/OUT
{
   DblLnkLst_Links *tmp;

   tmp       = l1->prev            ;
   (l1->prev = l2->prev)->next = l1;
   (l2->prev = tmp     )->next = l2;
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_Unlink1 --
 *
 *    Unlink an element from its list.
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_Unlink1(DblLnkLst_Links *l) // IN/OUT
{
   DblLnkLst_Unlink(l, l->next);
}


/*
 *----------------------------------------------------------------------------
 *
 * DblLnkLst_IsLinked --
 *
 *    Determines whether an element is linked with any other elements.
 *
 * Results:
 *    TRUE if link is linked, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
DblLnkLst_IsLinked(DblLnkLst_Links const *l) // IN
{
   /*
    * A DblLnkLst_Links is either linked to itself (not linked) or linked to
    * other elements in a list (linked).
    */
   return l->prev != l;
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_LinkFirst --
 *
 *    Insert 'l' at the beginning of the list anchored at 'head'
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_LinkFirst(DblLnkLst_Links *head, // IN/OUT
                    DblLnkLst_Links *l)    // IN/OUT
{
   DblLnkLst_Link(head->next, l);
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_LinkLast --
 *
 *    Insert 'l' at the end of the list anchored at 'head'
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_LinkLast(DblLnkLst_Links *head, // IN/OUT
                   DblLnkLst_Links *l)    // IN/OUT
{
   DblLnkLst_Link(head, l);
}


/*
 *----------------------------------------------------------------------
 *
 * DblLnkLst_Swap --
 *
 *    Swap all entries between the list anchored at 'head1' and the list
 *    anchored at 'head2'.
 *
 *    The operation is commutative
 *    The operation is inversible (its inverse is itself)
 *
 * Result
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE void
DblLnkLst_Swap(DblLnkLst_Links *head1,  // IN/OUT
               DblLnkLst_Links *head2)  // IN/OUT
{
   DblLnkLst_Links const tmp = *head1;

   if (DblLnkLst_IsLinked(head2)) {
      (head1->prev = head2->prev)->next = head1;
      (head1->next = head2->next)->prev = head1;
   } else {
      DblLnkLst_Init(head1);
   }

   if (tmp.prev != head1) {
      (head2->prev = tmp.prev)->next = head2;
      (head2->next = tmp.next)->prev = head2;
   } else {
      DblLnkLst_Init(head2);
   }
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _DBLLNKLST_H_ */

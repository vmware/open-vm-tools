/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

#include "vmware.h"
#include "dbllnklst.h"

/*
 * dbllnklst.c --
 *
 *    Light (but nonetheless powerful) implementation of doubly linked lists
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

void
DblLnkLst_Init(DblLnkLst_Links *l) // IN
{
   ASSERT(l);

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

void
DblLnkLst_Link(DblLnkLst_Links *l1, // IN
               DblLnkLst_Links *l2) // IN
{
   DblLnkLst_Links *tmp;

   ASSERT(l1);
   ASSERT(l2);

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

void
DblLnkLst_Unlink(DblLnkLst_Links *l1, // IN
                 DblLnkLst_Links *l2) // IN
{
   DblLnkLst_Links *tmp;

   ASSERT(l1);
   ASSERT(l2);

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

void
DblLnkLst_Unlink1(DblLnkLst_Links *l) // IN
{
   ASSERT(l);

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

Bool
DblLnkLst_IsLinked(DblLnkLst_Links const *l) // IN
{
   ASSERT(l);

   ASSERT((l->prev == l && l->next == l) ||
          (l->prev != l && l->next != l));

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

void
DblLnkLst_LinkFirst(DblLnkLst_Links *head, // IN
                    DblLnkLst_Links *l)    // IN
{
   ASSERT(head);
   ASSERT(l);

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

void
DblLnkLst_LinkLast(DblLnkLst_Links *head, // IN
                   DblLnkLst_Links *l)    // IN
{
   ASSERT(head);
   ASSERT(l);

   DblLnkLst_Link(head, l);
}


#if 0
/*
 * Test code (which also demonstrates how to use this library)
 */

/*
 * Add the double linked list capability to any of your data structure just by
 * adding a DblLnkLst_Links field inside it. It is not required that the field
 * comes first, but if it does, the execution will be slighly faster.
 *
 * Here we create a doubly linked list of integers
 */

#include <stdlib.h>
#include <stdio.h>

typedef struct member {
   int i;
   DblLnkLst_Links l;
} member;


/* Member constructor */
member *
make_member(int i)
{
   member *m;

   m = malloc(sizeof(*m));
   DblLnkLst_Init(&m->l);
   m->i = i;

   return m;
}


/* Dump a circular list */
void
dump_circular(const member *c) // IN
{
   const member *current;

   printf("forward: ");
   current = c;
   do {
      printf("%d ", current->i);
      current = DblLnkLst_Container(current->l.next, member, l);
   } while (current != c);
   printf("backward: ");
   do {
      printf("%d ", current->i);
      current = DblLnkLst_Container(current->l.prev, member, l);
   } while (current != c);
   printf("\n");
}


/* Dump an anchored list */
void
dump_anchored(const DblLnkLst_Links *h) // IN
{
   DblLnkLst_Links *cur_l;

   printf("forward: ");
   for (cur_l = h->next; cur_l != h; cur_l = cur_l->next) {
      member *current;

      current = DblLnkLst_Container(cur_l, member, l);
      printf("%d ", current->i);
   }
   printf("backward: ");
   for (cur_l = h->prev; cur_l != h; cur_l = cur_l->prev) {
      member *current;

      current = DblLnkLst_Container(cur_l, member, l);
      printf("%d ", current->i);
   }
   printf("\n");
}


/* Test code entry point */
int
main(int argc,    // IN
     char **argv) // IN
{
   member *c1;
   member *c2;
   member *c3;
   member *c4;

   DblLnkLst_Links h;
   member *a1;
   member *a2;
   member *a3;

   printf("Circular list: there is no origin\n");

   /* Create the 1st member */
   c1 = make_member(1);
   /* Special case: there is no list to merge with, initially */

   /* Add the 2nd member _after_ the 1st one */
   c2 = make_member(2);
   DblLnkLst_Link(&c1->l, &c2->l);

   /* Add the 3rd member _after_ the 2nd one */
   c3 = make_member(3);
   DblLnkLst_Link(&c1->l, &c3->l);

   /* Add the 4th member _before_ the 3rd one */
   c4 = make_member(4);
   DblLnkLst_Link(&c3->l, &c4->l);

   printf("See it from this member...\n");
   dump_circular(c1);
   printf("...Or from this one\n");
   dump_circular(c4);

   printf("\n");
   printf("Anchored (linear) list: it has a beginning and an end\n");

   /* Create the 'head' of the list */
   DblLnkLst_Init(&h);

   /* Add the 1st member at the _end_ */
   a1 = make_member(5);
   DblLnkLst_LinkLast(&h, &a1->l);

   /* Add the 2nd member at the _beginning_ */
   a2 = make_member(6);
   DblLnkLst_LinkFirst(&h, &a2->l);

   /* Add the 3rd member _before_ the 1st one */
   a3 = make_member(7);
   DblLnkLst_Link(&a1->l, &a3->l);

   dump_anchored(&h);

   printf("\n");
   printf("Merge both lists: the result is an anchored list\n");

   DblLnkLst_Link(&h, &c4->l);

   dump_anchored(&h);

   printf("\n");
   printf("Remove a member\n");

   DblLnkLst_Unlink1(&c3->l);

   dump_anchored(&h);

   printf("\n");
   printf("Split the result in two lists: an anchored one and a circular "
          "one\n");
   DblLnkLst_Unlink(&h, &a1->l);

   dump_anchored(&h);
   dump_circular(a1);

   return 0;
}
#endif

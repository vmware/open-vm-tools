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
 * 3. Neither the name of VMware Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission of VMware Inc.
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

#include "vmware.h"
#include "dbllnklst.h"

/*
 * dbllnklst.c --
 *
 *    Light (but nonetheless powerful) implementation of doubly linked lists
 */


/*
 * XXX This file is empty because I made all the functions inline.
 * -- edward
 */


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

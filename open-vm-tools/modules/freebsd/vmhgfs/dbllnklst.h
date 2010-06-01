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

/*
 * dbllnklst.h --
 *
 *    Double linked lists
 */

#ifndef _DBLLNKLST_H_
#define _DBLLNKLST_H_

#include "vm_basic_types.h"

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"


#define DblLnkLst_OffsetOf(type, field) ((intptr_t)&((type *)0)->field)

#define DblLnkLst_Container(addr, type, field) \
   ((type *)((char *)addr - DblLnkLst_OffsetOf(type, field)))

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


/* Functions for both circular and anchored lists. --hpreg */

void DblLnkLst_Init(DblLnkLst_Links *l);
void DblLnkLst_Link(DblLnkLst_Links *l1, DblLnkLst_Links *l2);
void DblLnkLst_Unlink(DblLnkLst_Links *l1, DblLnkLst_Links *l2);
void DblLnkLst_Unlink1(DblLnkLst_Links *l);
Bool DblLnkLst_IsLinked(DblLnkLst_Links const *l);

/* Functions specific to anchored lists. --hpreg */

void DblLnkLst_LinkFirst(DblLnkLst_Links *head, DblLnkLst_Links *l);
void DblLnkLst_LinkLast(DblLnkLst_Links *head, DblLnkLst_Links *l);


#endif /* _DBLLNKLST_H_ */

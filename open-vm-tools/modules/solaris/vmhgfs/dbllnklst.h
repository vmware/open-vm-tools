/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
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

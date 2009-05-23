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

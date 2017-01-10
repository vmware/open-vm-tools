/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
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
 * request.h --
 *
 * Prototypes for request initialization, allocation, and list manipulation
 * functions.
 *
 */

#ifndef __REQUEST_H_
#define __REQUEST_H_


#include <sys/ksynch.h> /* mutexes and condition variables */

#include "hgfsSolaris.h"
#include "debug.h"

#include "dbllnklst.h"  /* Double link list types */

/*
 * Macros
 */

/* Since the DblLnkLst_Links in the superinfo is just an anchor, we want to
 * skip it (e.g., get the container for the next element) */
#define HGFS_REQ_LIST_HEAD(si)           \
            (DblLnkLst_Container(si->reqList.next, HgfsReq, listNode))
#define HGFS_REQ_LIST_HEAD_NODE(si)      \
            (si->reqList.next)
#define HGFS_FREE_REQ_LIST_HEAD(si)      \
            (DblLnkLst_Container(si->reqFreeList.next, HgfsReq, listNode))
#define HGFS_FREE_REQ_LIST_HEAD_NODE(si) \
            (si->reqFreeList.next)


/*
 * Functions
 */
void HgfsInitRequestList(HgfsSuperInfo *sip);
void HgfsCancelAllRequests(HgfsSuperInfo *sip);
INLINE Bool HgfsListIsEmpty(DblLnkLst_Links *listAnchor);
HgfsReq *HgfsGetNewReq(HgfsSuperInfo *sip);
void HgfsDestroyReq(HgfsSuperInfo *sip, HgfsReq *oldReq);
int HgfsSendRequest(HgfsSuperInfo *sip, HgfsReq *req);
INLINE void HgfsWakeWaitingClient(HgfsSuperInfo *sip, HgfsReq *req);

#endif /* __REQUEST_H_ */

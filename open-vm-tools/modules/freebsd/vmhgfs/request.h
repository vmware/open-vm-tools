/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * request.h --
 *
 *	Declarations for the HgfsRequest module.  This interface abstracts Hgfs
 *	request processing from the filesystem driver.
 */

#ifndef _REQUEST_H_
#define _REQUEST_H_

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "dbllnklst.h"  /* Double link list types */

/*
 * Each request will traverse through this set of states.  File systems may
 * query the state of their request, but they may not update it.
 */
typedef enum {
   HGFS_REQ_UNUSED = 1,
   HGFS_REQ_ALLOCATED,
   HGFS_REQ_SUBMITTED,
   HGFS_REQ_ABANDONED,
   HGFS_REQ_ERROR,
   HGFS_REQ_COMPLETED
} HgfsKReqState;

/*
 * Opaque request handler used by the file system code.  Allocated during
 * HgfsKReq_AllocRequest and released at HgfsKReq_ReleaseRequest.
 */
typedef struct HgfsKReqObject *         HgfsKReqHandle;

/*
 * Opaque request object container for the file system.  File systems snag one
 * of these during HgfsKReq_InitSip & relinquish during HgfsKReq_UninitSip.
 */
typedef struct HgfsKReqContainer *      HgfsKReqContainerHandle;


/*
 * Global functions (prototypes)
 */

extern int                     HgfsKReq_SysInit(void);
extern int                     HgfsKReq_SysFini(void);

extern HgfsKReqContainerHandle HgfsKReq_AllocateContainer(void);
extern void                    HgfsKReq_FreeContainer(HgfsKReqContainerHandle handle);
extern void                    HgfsKReq_CancelRequests(HgfsKReqContainerHandle handle);
extern Bool                    HgfsKReq_ContainerIsEmpty(HgfsKReqContainerHandle handle);

extern HgfsKReqHandle          HgfsKReq_AllocateRequest(HgfsKReqContainerHandle handle, int *ret);
extern void                    HgfsKReq_ReleaseRequest(HgfsKReqContainerHandle container,
                                                       HgfsKReqHandle oldRequest);
extern int                     HgfsKReq_SubmitRequest(HgfsKReqHandle req);
extern HgfsKReqState           HgfsKReq_GetState(HgfsKReqHandle req);

extern uint32_t                HgfsKReq_GetId(HgfsKReqHandle req);
extern char *                  HgfsKReq_GetPayload(HgfsKReqHandle req);
extern char *                  HgfsKReq_GetPayload_V3(HgfsKReqHandle req);
extern char *                  HgfsKRep_GetPayload_V3(HgfsKReqHandle req);
extern size_t                  HgfsKReq_GetPayloadSize(HgfsKReqHandle req);
extern void                    HgfsKReq_SetPayloadSize(HgfsKReqHandle req,
                                                       size_t newSize);


#endif /* _REQUEST_H_ */


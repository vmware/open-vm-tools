/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * request.c --
 *
 * Functions dealing with the creation, deletion, and sending of HGFS
 * requests are defined here.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <asm/atomic.h>
#include <linux/list.h>
#include <linux/signal.h>
#include "compat_kernel.h"
#include "compat_sched.h"
#include "compat_semaphore.h"
#include "compat_slab.h"
#include "compat_spinlock.h"

#include "module.h"
#include "request.h"
#include "transport.h"
#include "fsutil.h"
#include "vm_assert.h"

/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestInit --
 *
 *    Initializes new request structure.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsRequestInit(HgfsReq *req,       // IN: request to initialize
                int requestId)      // IN: ID assigned to the request
{
   ASSERT(req);

   kref_init(&req->kref);
   INIT_LIST_HEAD(&req->list);
   init_waitqueue_head(&req->queue);
   req->id = requestId;
   req->payloadSize = 0;
   req->state = HGFS_REQ_STATE_ALLOCATED;
   req->numEntries = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsGetNewRequest --
 *
 *    Allocates and initializes new request structure.
 *
 * Results:
 *    On success the new struct is returned with all fields
 *    initialized. Returns NULL on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsGetNewRequest(void)
{
   static atomic_t hgfsIdCounter = ATOMIC_INIT(0);
   HgfsReq *req;

   req = HgfsTransportAllocateRequest(HGFS_PACKET_MAX);
   if (req == NULL) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: can't allocate memory\n", __func__));
      return NULL;
   }

   HgfsRequestInit(req, atomic_inc_return(&hgfsIdCounter) - 1);

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCopyRequest --
 *
 *    Allocates and initializes new request structure and copies
 *    existing request into it.
 *
 * Results:
 *    On success the new struct is returned with all fields
 *    initialized. Returns NULL on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsCopyRequest(HgfsReq *req)   // IN: request to be copied
{
   HgfsReq *newReq;

   ASSERT(req);

   newReq = HgfsTransportAllocateRequest(req->bufferSize);
   if (newReq == NULL) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: can't allocate memory\n", __func__));
      return NULL;
   }

   HgfsRequestInit(newReq, req->id);

   memcpy(newReq->dataPacket, req->dataPacket,
          req->numEntries * sizeof (req->dataPacket[0]));

   newReq->numEntries = req->numEntries;
   newReq->payloadSize = req->payloadSize;
   memcpy(newReq->payload, req->payload, req->payloadSize);

   return newReq;
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsSendRequest --
 *
 *    Send out an HGFS request via transport layer, and wait for the reply.
 *
 * Results:
 *    Returns zero on success, negative number on error.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSendRequest(HgfsReq *req)       // IN/OUT: Outgoing request
{
   int ret;

   ASSERT(req);
   ASSERT(req->payloadSize <= req->bufferSize);
   req->state = HGFS_REQ_STATE_UNSENT;

   LOG(10, (KERN_DEBUG "VMware hgfs: HgfsSendRequest: Sending request id %d\n",
           req->id));
   ret = HgfsTransportSendRequest(req);
   LOG(10, (KERN_DEBUG "VMware hgfs: HgfsSendRequest: request finished, "
           "return %d\n", ret));

   return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestFreeMemory --
 *
 *    Frees memory allocated for a request.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void HgfsRequestFreeMemory(struct kref *kref)
{
   HgfsReq *req = container_of(kref, HgfsReq, kref);

   LOG(10, (KERN_DEBUG "VMware hgfs: %s: freeing request %d\n",
            __func__, req->id));
   HgfsTransportFreeRequest(req);
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestPutRef --
 *
 *    Decrease reference count of HGFS request.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    May cause request to be destroyed.
 *
 *----------------------------------------------------------------------
 */

void
HgfsRequestPutRef(HgfsReq *req) // IN: Request
{
   if (req) {
      LOG(10, (KERN_DEBUG "VMware hgfs: %s: request %d\n",
               __func__, req->id));
      kref_put(&req->kref, HgfsRequestFreeMemory);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestGetRef --
 *
 *    Increment reference count of HGFS request.
 *
 * Results:
 *    Pointer to the same HGFS request.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsRequestGetRef(HgfsReq *req) // IN: Request
{
   if (req) {
      LOG(10, (KERN_DEBUG "VMware hgfs: %s: request %d\n",
               __func__, req->id));
      kref_get(&req->kref);
   }

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReplyStatus --
 *
 *    Return reply status.
 *
 * Results:
 *    Returns reply status as per the protocol.
 *    XXX: Needs changes when vmci headers are added.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsStatus
HgfsReplyStatus(HgfsReq *req)  // IN
{
   HgfsReply *rep;

   rep = (HgfsReply *)(HGFS_REQ_PAYLOAD(req));

   return rep->status;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCompleteReq --
 *
 *    Marks request as completed and wakes up sender.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsCompleteReq(HgfsReq *req)       // IN: Request
{
   ASSERT(req);

   req->state = HGFS_REQ_STATE_COMPLETED;
   /* Wake up the client process waiting for the reply to this request. */
   wake_up(&req->queue);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFailReq --
 *
 *    Marks request as failed and calls HgfsCompleteReq to wake up
 *    sender.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void HgfsFailReq(HgfsReq *req,   // IN: erequest to be marked failed
                 int error)     // IN: error code
{
   HgfsReply *reply = req->payload;

   reply->id = req->id;
   reply->status = error;

   req->payloadSize = sizeof *reply;
   HgfsCompleteReq(req);
}

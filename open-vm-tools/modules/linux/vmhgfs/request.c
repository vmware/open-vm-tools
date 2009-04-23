/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

static uint64 hgfsIdCounter = 0;
static spinlock_t hgfsIdLock = SPIN_LOCK_UNLOCKED;


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetNewRequest --
 *
 *    Get a new request structure off the free list and initialize it.
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
   HgfsReq *req = NULL;

   req = kmem_cache_alloc(hgfsReqCache, GFP_KERNEL);
   if (req == NULL) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNewRequest: "
              "can't allocate memory\n"));
      return NULL;
   }
   init_waitqueue_head(&req->queue);
   req->payloadSize = 0;
   req->state = HGFS_REQ_STATE_ALLOCATED;
   /* Setup the packet prefix. */
   memcpy(req->packet, HGFS_SYNC_REQREP_CLIENT_CMD,
          HGFS_SYNC_REQREP_CLIENT_CMD_LEN);
   spin_lock(&hgfsIdLock);
   req->id = hgfsIdCounter;
   hgfsIdCounter++;
   spin_unlock(&hgfsIdLock);
   return req;
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
   ASSERT(req->payloadSize <= HGFS_PACKET_MAX);

   req->state = HGFS_REQ_STATE_UNSENT;

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendRequest: Sending request id %d\n",
           req->id));

   ret = HgfsTransportSendRequest(req);
   if (ret == 0) { /* Send succeeded. */
      wait_event(req->queue, req->state == HGFS_REQ_STATE_COMPLETED);
   } /* else send failed. */

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendRequest: request finished, "
           "return %d\n", ret));
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFreeRequest --
 *
 *    Free an HGFS request.
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
HgfsFreeRequest(HgfsReq *req) // IN: Request to free
{
   ASSERT(hgfsReqCache);

   if (req != NULL) {
      kmem_cache_free(hgfsReqCache, req);
   }
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
 *    Copies the reply packet into the request structure and wakes up
 *    the associated client.
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
HgfsCompleteReq(HgfsReq *req,       // IN: Request
                char const *reply,  // IN: Reply packet
                size_t replySize)   // IN: Size of reply packet
{
   ASSERT(req);
   ASSERT(reply);
   ASSERT(replySize <= HGFS_PACKET_MAX);

   memcpy(HGFS_REQ_PAYLOAD(req), reply, replySize);
   req->payloadSize = replySize;
   req->state = HGFS_REQ_STATE_COMPLETED;
   ASSERT(!list_empty(&req->list));
   list_del_init(&req->list);
   /* Wake up the client process waiting for the reply to this request. */
   wake_up(&req->queue);
}

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
#include <asm/semaphore.h>
#include <linux/list.h>
#include <linux/signal.h>
#include "compat_kernel.h"
#include "compat_sched.h"
#include "compat_slab.h"
#include "compat_spinlock.h"

#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"

static int HgfsWaitRequestReply(HgfsReq *req);

/*
 * Private function implementations.
 */


/*
 *----------------------------------------------------------------------
 *
 * HgfsWaitRequestReply --
 *
 *    Wait for the reply to a request that we sent.
 *
 * Results:
 *    Returns zero when the answer has been received, -ERESTARTSYS if
 *    interrupted, or -EPROTO if there was a backdoor error. It is important
 *    that -ERESTARTSYS be returned in the event of a signal getting caught,
 *    because calling functions test the return value to determine
 *    whether or not to free the request object.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsWaitRequestReply(HgfsReq *req)  // IN/OUT: Request object
{
   int err = 0;
   long timeleft;

   ASSERT(req);

   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsWaitRequestReply: null req\n"));
      return -EINVAL;
   }
   
   timeleft = wait_event_timeout(req->queue, 
                                 (req->state == HGFS_REQ_STATE_COMPLETED ||
                                  req->state == HGFS_REQ_STATE_ERROR),
                                 HGFS_REQUEST_TIMEOUT);
   /* 
    * Did we time out? If so, abandon the request. We have to be careful, 
    * because a timeout means that the request is still on a list somewhere.
    */
   if (timeleft == 0) {
      spin_lock(&hgfsBigLock);
      if (!list_empty(&req->list)) {
         list_del_init(&req->list);
      }
      spin_unlock(&hgfsBigLock);

      /* 
       * Notice that we're completely ignoring any pending signals. That's
       * because the request timed out; it was not interrupted. There's no
       * point in having the client retry the syscall (through -ERESTARTSYS) if
       * it wasn't actually interrupted.
       */
      err = -EIO;
   } else if (req->state == HGFS_REQ_STATE_ERROR) {
      /* 
       * If the backdoor exploded, let's modify the return value so the client
       * knows about it. We only care about this if we didn't timeout.
       */
      err = -EPROTO;
   }

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsWaitRequestReply: request finished, "
           "code %d\n", err));
   return err;
}


/*
 * Public function implementations.
 */

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

   /* Atomically increment counter and set ID. */
   spin_lock(&hgfsBigLock);
   req->id = hgfsIdCounter++;
   spin_unlock(&hgfsBigLock);

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSendRequest --
 *
 *    Add an HGFS request to the request queue, wake up the backdoor
 *    handler, and wait for the reply.
 *
 * Results:
 *    Returns zero on success, -ERESTARTSYS if interrupted (this value will
 *    be returned by HgfsWaitRequestReply), negative error
 *    otherwise. Callers use the -ERESTARTSYS return value to determine
 *    whether they should free the request object before exiting.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSendRequest(HgfsReq *req)       // IN/OUT: Outgoing request
{
   int error;

   ASSERT(req);
   ASSERT(req->payloadSize <= HGFS_PACKET_MAX);

   req->state = HGFS_REQ_STATE_UNSENT;

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendRequest: Sending request id %d\n", 
           req->id));

   /* 
    * Add the request to the queue, wake up the backdoor handler thread, and
    * wait for a reply.
    */
   spin_lock(&hgfsBigLock);
   list_add_tail(&req->list, &hgfsReqsUnsent);
   spin_unlock(&hgfsBigLock);

   set_bit(HGFS_REQ_THREAD_SEND, &hgfsReqThreadFlags);
   wake_up_interruptible(&hgfsReqThreadWait);
   error = HgfsWaitRequestReply(req);

   return error; 
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
   
   /* Atomically decrement counter. */
   spin_lock(&hgfsBigLock);
   hgfsIdCounter--;
   spin_unlock(&hgfsBigLock);

   if (req != NULL) {
      kmem_cache_free(hgfsReqCache, req);
   }
}


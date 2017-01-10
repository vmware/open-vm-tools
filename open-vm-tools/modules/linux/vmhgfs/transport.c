/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * transport.c --
 *
 * This file handles the transport mechanisms available for HGFS.
 * This acts as a glue between the HGFS filesystem driver and the
 * actual transport channels (backdoor, tcp, vsock, ...).
 *
 * The sends happen in the process context, where as a kernel thread
 * handles the asynchronous replies. A queue of pending replies is
 * maintained and is protected by a spinlock. The channel opens and close
 * is protected by a mutex.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/list.h>
#include "compat_mutex.h"
#include "compat_sched.h"
#include "compat_spinlock.h"
#include "compat_version.h"

/* Must be included after semaphore.h. */
#include <linux/timer.h>
/* Must be included after sched.h. */
#include <linux/interrupt.h> /* for spin_lock_bh */


#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "transport.h"
#include "vm_assert.h"

static HgfsTransportChannel *hgfsChannel;     /* Current active channel. */
static compat_mutex_t hgfsChannelLock;        /* Lock to protect hgfsChannel. */
static struct list_head hgfsRepPending;       /* Reply pending queue. */
static spinlock_t hgfsRepQueueLock;           /* Reply pending queue lock. */

/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportOpenChannel --
 *
 *     Opens given communication channel with HGFS server.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsTransportOpenChannel(HgfsTransportChannel *channel)
{
   Bool ret;

   switch (channel->status) {
   case HGFS_CHANNEL_UNINITIALIZED:
   case HGFS_CHANNEL_DEAD:
      ret = FALSE;
      break;

   case HGFS_CHANNEL_CONNECTED:
      ret = TRUE;
      break;

   case HGFS_CHANNEL_NOTCONNECTED:
      ret = channel->ops.open(channel);
      if (ret) {
         channel->status = HGFS_CHANNEL_CONNECTED;
      }
      break;

   default:
      ret = FALSE;
      ASSERT(0); /* Not reached. */
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportCloseChannel --
 *
 *     Closes currently open communication channel. Has to be called
 *     while holdingChannelLock.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsTransportCloseChannel(HgfsTransportChannel *channel)
{
   if (channel->status == HGFS_CHANNEL_CONNECTED ||
       channel->status == HGFS_CHANNEL_DEAD) {

      channel->ops.close(channel);
      channel->status = HGFS_CHANNEL_NOTCONNECTED;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportSetupNewChannel --
 *
 *     Find a new workable channel.
 *
 * Results:
 *     TRUE on success, otherwise FALSE.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsTransportSetupNewChannel(void)
{
   HgfsTransportChannel *newChannel;

   newChannel = HgfsGetBdChannel();
   LOG(10, (KERN_DEBUG LGPFX "%s CHANNEL: Bd channel\n", __func__));
   ASSERT(newChannel);
   hgfsChannel = newChannel;
   return HgfsTransportOpenChannel(newChannel);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransporAddPendingRequest --
 *
 *     Adds a request to the hgfsRepPending queue.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsTransportAddPendingRequest(HgfsReq *req)   // IN: Request to add
{
   ASSERT(req);

   spin_lock_bh(&hgfsRepQueueLock);
   list_add_tail(&req->list, &hgfsRepPending);
   spin_unlock_bh(&hgfsRepQueueLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportRemovePendingRequest --
 *
 *     Dequeues the request from the hgfsRepPending queue.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

void
HgfsTransportRemovePendingRequest(HgfsReq *req)   // IN: Request to dequeue
{
   ASSERT(req);

   spin_lock_bh(&hgfsRepQueueLock);
   list_del_init(&req->list);
   spin_unlock_bh(&hgfsRepQueueLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportFlushPendingRequests --
 *
 *     Complete all submitted requests with an error, called when
 *     we are about to tear down communication channel.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsTransportFlushPendingRequests(void)
{
   struct HgfsReq *req;

   spin_lock_bh(&hgfsRepQueueLock);

   list_for_each_entry(req, &hgfsRepPending, list) {
      if (req->state == HGFS_REQ_STATE_SUBMITTED) {
         LOG(6, (KERN_DEBUG LGPFX "%s: injecting error reply to req id: %d\n",
                 __func__, req->id));
         HgfsFailReq(req, -EIO);
      }
   }

   spin_unlock_bh(&hgfsRepQueueLock);
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportGetPendingRequest --
 *
 *     Attempts to locate request with specified ID in the queue of
 *     pending (waiting for server's reply) requests.
 *
 * Results:
 *     NULL if request not found; otherwise address of the request
 *     structure.
 *
 * Side effects:
 *     Increments reference count of the request.
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsTransportGetPendingRequest(HgfsHandle id)   // IN: id of the request
{
   HgfsReq *cur, *req = NULL;

   spin_lock_bh(&hgfsRepQueueLock);

   list_for_each_entry(cur, &hgfsRepPending, list) {
      if (cur->id == id) {
         ASSERT(cur->state == HGFS_REQ_STATE_SUBMITTED);
         req = HgfsRequestGetRef(cur);
         break;
      }
   }

   spin_unlock_bh(&hgfsRepQueueLock);

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportAllocateRequest --
 *
 *     Allocates HGFS request structre using channel-specific allocator.
 *
 * Results:
 *     NULL on failure; otherwisepointer to newly allocated request.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsTransportAllocateRequest(size_t bufferSize)   // IN: size of the buffer
{
   HgfsReq *req = NULL;
   /*
    * We use a temporary variable to make sure we stamp the request with
    * same channel as we used to make allocation since hgfsChannel can
    * be changed while we do allocation.
    */
   HgfsTransportChannel *currentChannel = hgfsChannel;

   ASSERT(currentChannel);

   req = currentChannel->ops.allocate(bufferSize);
   if (req) {
         req->transportId = currentChannel;
   }

   return req;
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportFreeRequest --
 *
 *     Free HGFS request structre using channel-specific free function.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

void
HgfsTransportFreeRequest(HgfsReq *req)   // IN: size of the buffer
{
   /*
    * We cannot use hgfsChannel structre because global channel could
    * changes in the meantime. We remember the channel when we do
    * allocation and call the same channel for de-allocation. Smart.
    */

   HgfsTransportChannel *channel = (HgfsTransportChannel *)req->transportId;
   channel->ops.free(req);
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportSendRequest --
 *
 *     Sends the request via channel communication.
 *
 * Results:
 *     Zero on success, non-zero error on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

int
HgfsTransportSendRequest(HgfsReq *req)   // IN: Request to send
{
   HgfsReq *origReq = req;
   int ret = -EIO;

   ASSERT(req);
   ASSERT(req->state == HGFS_REQ_STATE_UNSENT);
   ASSERT(req->payloadSize <= req->bufferSize);

   compat_mutex_lock(&hgfsChannelLock);

   HgfsTransportAddPendingRequest(req);

   do {

      if (unlikely(hgfsChannel->status != HGFS_CHANNEL_CONNECTED)) {
         if (hgfsChannel->status == HGFS_CHANNEL_DEAD) {
            HgfsTransportCloseChannel(hgfsChannel);
            HgfsTransportFlushPendingRequests();
         }

         if (!HgfsTransportSetupNewChannel()) {
            ret = -EIO;
            goto out;
         }
      }

      ASSERT(hgfsChannel->ops.send);

      /* If channel changed since we created request we need to adjust */
     if (req->transportId != hgfsChannel) {

         HgfsTransportRemovePendingRequest(req);

         if (req != origReq) {
            HgfsRequestPutRef(req);
         }

         req = HgfsCopyRequest(origReq);
         if (req == NULL) {
            req = origReq;
            ret = -ENOMEM;
            goto out;
         }

         HgfsTransportAddPendingRequest(req);
      }

      ret = hgfsChannel->ops.send(hgfsChannel, req);
      if (likely(ret == 0))
         break;

      LOG(4, (KERN_DEBUG LGPFX "%s: send failed with error %d\n",
              __func__, ret));

      if (ret == -EINTR) {
         /* Don't retry when we are interrupted by some signal. */
         goto out;
      }

      hgfsChannel->status = HGFS_CHANNEL_DEAD;

   } while (1);

   ASSERT(req->state == HGFS_REQ_STATE_COMPLETED ||
          req->state == HGFS_REQ_STATE_SUBMITTED);

out:
   compat_mutex_unlock(&hgfsChannelLock);

   if (likely(ret == 0)) {
      /*
       * Send succeeded, wait for the reply.
       * Right now, we cannot cancel request once they
       * are dispatched to the host.
       */
      wait_event(req->queue,
                 req->state == HGFS_REQ_STATE_COMPLETED);
   }

   HgfsTransportRemovePendingRequest(req);

   /*
    * If we used a copy of request because we changed transport we
    * need to copy payload back into original request.
    */
   if (req != origReq) {
      ASSERT(req->payloadSize <= origReq->bufferSize);
      origReq->payloadSize = req->payloadSize;
      memcpy(origReq->payload, req->payload, req->payloadSize);
      HgfsRequestPutRef(req);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportInit --
 *
 *     Initialize the transport.
 *
 *     Starts the reply thread, for handling incoming packets on the
 *     connected socket.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

void
HgfsTransportInit(void)
{
   INIT_LIST_HEAD(&hgfsRepPending);
   spin_lock_init(&hgfsRepQueueLock);
   compat_mutex_init(&hgfsChannelLock);

   compat_mutex_lock(&hgfsChannelLock);

   hgfsChannel = HgfsGetBdChannel();
   ASSERT(hgfsChannel);

   compat_mutex_unlock(&hgfsChannelLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportMarkDead --
 *
 *     Marks current channel as dead so it can be cleaned up and
 *     fails all submitted requests.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

void
HgfsTransportMarkDead(void)
{
   LOG(8, (KERN_DEBUG LGPFX "%s entered.\n", __func__));

   compat_mutex_lock(&hgfsChannelLock);

   if (hgfsChannel) {
      hgfsChannel->status = HGFS_CHANNEL_DEAD;
   }
   HgfsTransportFlushPendingRequests();

   compat_mutex_unlock(&hgfsChannelLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportExit --
 *
 *     Teardown the transport.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     Cleans up everything, frees queues, closes channel.
 *
 *----------------------------------------------------------------------
 */

void
HgfsTransportExit(void)
{
   LOG(8, (KERN_DEBUG LGPFX "%s entered.\n", __func__));

   compat_mutex_lock(&hgfsChannelLock);
   ASSERT(hgfsChannel);
   HgfsTransportCloseChannel(hgfsChannel);
   hgfsChannel = NULL;
   compat_mutex_unlock(&hgfsChannelLock);

   ASSERT(list_empty(&hgfsRepPending));
   LOG(8, (KERN_DEBUG LGPFX "%s exited.\n", __func__));
}



/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
#include "compat_kthread.h"
#include "compat_list.h"
#include "compat_mutex.h"
#include "compat_sched.h"
#include "compat_spinlock.h"
#include "compat_version.h"

/* Must be included after semaphore.h. */
#include <linux/timer.h>
/* Must be included after sched.h. */
#include <linux/smp_lock.h>

#include "bdhandler.h"
#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "tcp.h"
#include "transport.h"
#include "vm_assert.h"

COMPAT_KTHREAD_DECLARE_STOP_INFO();
static HgfsTransportChannel *hgfsChannel;     /* Current active channel. */
static compat_mutex_t hgfsChannelLock;        /* Lock to protect hgfsChannel. */
static struct list_head hgfsRepPending;       /* Reply pending queue. */
static spinlock_t hgfsRepQueueLock;           /* Reply pending queue lock. */

#define HgfsRequestId(req) ((HgfsRequest *)req)->id


/*
 * Private function implementations.
 */

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
   hgfsChannel = HgfsGetVSocketChannel();
   if (hgfsChannel != NULL) {
      if (hgfsChannel->ops.open(hgfsChannel)) {
         return TRUE;
      }
   }

   hgfsChannel = HgfsGetTcpChannel();
   if (hgfsChannel != NULL) {
      if (hgfsChannel->ops.open(hgfsChannel)) {
         return TRUE;
      }
   }

   hgfsChannel = HgfsGetBdChannel();
   if (hgfsChannel != NULL) {
      if (hgfsChannel->ops.open(hgfsChannel)) {
         return TRUE;
      }
   }

   hgfsChannel = NULL;
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportStopCurrentChannel --
 *
 *     Teardown current channel and stop current receive thread.
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
HgfsTransportStopCurrentChannel(void)
{
   if (hgfsChannel) {
      hgfsChannel->ops.exit(hgfsChannel);
      hgfsChannel = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportChannelFailover --
 *
 *     Called when current channel doesn't work. Find a new channel
 *     for transport.
 *
 * Results:
 *     TRUE on success, otherwise FALSE;
 *
 * Side effects:
 *     Teardown current opened channel and the receive thread, set up
 *     new channel and new receive thread.
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsTransportChannelFailover(void) {
   Bool ret = FALSE;
   HgfsTransportStopCurrentChannel();
   ret = HgfsTransportSetupNewChannel();
   LOG(8, ("VMware hgfs: %s result: %s.\n", __func__, ret ? "TRUE" : "FALSE"));
   return ret;
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

   spin_lock(&hgfsRepQueueLock);
   list_add_tail(&req->list, &hgfsRepPending);
   spin_unlock(&hgfsRepQueueLock);
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

static void
HgfsTransportRemovePendingRequest(HgfsReq *req)   // IN: Request to dequeue
{
   ASSERT(req);

   spin_lock(&hgfsRepQueueLock);
   if (!list_empty(&req->list)) {
      list_del_init(&req->list);
   }
   spin_unlock(&hgfsRepQueueLock);
}


/*
 * Public function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportProcessPacket --
 *
 *     Helper function to process received packets, called by the channel
 *     handler thread.
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
HgfsTransportProcessPacket(char *receivedPacket,    //IN: received packet
                           size_t receivedSize)     //IN: packet size
{
   struct list_head *cur, *next;
   HgfsHandle id;
   Bool found = FALSE;

   /* Got the reply. */

   ASSERT(receivedPacket != NULL && receivedSize > 0);
   id = HgfsRequestId(receivedPacket);
   LOG(8, ("VMware hgfs: %s entered.\n", __func__));
   LOG(6, (KERN_DEBUG "VMware hgfs: %s: req id: %d\n", __func__, id));
   /*
    * Search through hgfsRepPending queue for the matching id and wake up
    * the associated waiting process. Delete the req from the queue.
    */
   spin_lock(&hgfsRepQueueLock);
   list_for_each_safe(cur, next, &hgfsRepPending) {
      HgfsReq *req;
      req = list_entry(cur, HgfsReq, list);
      if (req->id == id) {
         ASSERT(req->state == HGFS_REQ_STATE_SUBMITTED);
         HgfsCompleteReq(req, receivedPacket, receivedSize);
         found = TRUE;
         break;
      }
   }
   spin_unlock(&hgfsRepQueueLock);

   if (!found) {
      LOG(4, ("VMware hgfs: %s: No matching id, dropping reply\n",
              __func__));
   }
   LOG(8, ("VMware hgfs: %s exited.\n", __func__));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportBeforeExitingRecvThread --
 *
 *     The cleanup work to do before the recv thread exits, including
 *     completing pending requests with error.
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
HgfsTransportBeforeExitingRecvThread(void)
{
   struct list_head *cur, *next;

   /* Walk through hgfsRepPending queue and reply them with error. */
   spin_lock(&hgfsRepQueueLock);
   list_for_each_safe(cur, next, &hgfsRepPending) {
      HgfsReq *req;
      HgfsReply reply;

      /* XXX: Make the request senders be aware of this error. */
      reply.status = -EIO;
      req = list_entry(cur, HgfsReq, list);
      LOG(6, ("VMware hgfs: %s: injecting error reply to req id: %d\n",
              __func__, req->id));
      HgfsCompleteReq(req, (char *)&reply, sizeof reply);
   }
   spin_unlock(&hgfsRepQueueLock);
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
   int ret;
   ASSERT(req);
   ASSERT(req->state == HGFS_REQ_STATE_UNSENT);
   ASSERT(req->payloadSize <= HGFS_PACKET_MAX);

   compat_mutex_lock(&hgfsChannelLock);

   /* Try opening the channel. */
   if (!hgfsChannel && !HgfsTransportSetupNewChannel()) {
      compat_mutex_unlock(&hgfsChannelLock);
      return -EPROTO;
   }

   ASSERT(hgfsChannel->ops.send);

   HgfsTransportAddPendingRequest(req);

   while ((ret = hgfsChannel->ops.send(hgfsChannel, req)) != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: send failed. Return %d\n",
              __func__, ret));
      if (ret == -EINTR) {
         /* Don't retry when we are interrupted by some signal. */
         goto out;
      }
      if (!hgfsChannel->ops.open(hgfsChannel) && !HgfsTransportChannelFailover()) {
         /* Can't establish a working channel, just report error. */
         ret = -EIO;
         goto out;
      }
   }

   ASSERT(req->state == HGFS_REQ_STATE_COMPLETED ||
          req->state == HGFS_REQ_STATE_SUBMITTED);

out:
   compat_mutex_unlock(&hgfsChannelLock);

   if (ret == 0) { /* Send succeeded. */
      if (wait_event_interruptible(req->queue,
                                   req->state == HGFS_REQ_STATE_COMPLETED)) {
         ret = -EINTR; /* Interrupted by some signal. */
      }
   } /* else send failed. */

   if (ret < 0) {
      HgfsTransportRemovePendingRequest(req);
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

   hgfsChannel = NULL;
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
   LOG(8, ("VMware hgfs: %s entered.\n", __func__));
   compat_mutex_lock(&hgfsChannelLock);
   HgfsTransportStopCurrentChannel();
   compat_mutex_unlock(&hgfsChannelLock);

   ASSERT(list_empty(&hgfsRepPending));
   LOG(8, ("VMware hgfs: %s exited.\n", __func__));
}

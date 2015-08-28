/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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

/*
 * transport.c --
 *
 * This file handles the transport mechanisms available for HGFS.
 * This acts as a glue between the HGFS filesystem driver and the
 * actual transport channels (backdoor, tcp, vsock, ...).
 *
 * The sends happen in the process context, where as a thread
 * handles the asynchronous replies. A queue of pending replies is
 * maintained and is protected by a lock. The channel opens and close
 * is protected by a mutex.
 */



#include "bdhandler.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "transport.h"
#include "vm_assert.h"

static HgfsTransportChannel *gHgfsActiveChannel;     /* Current active channel. */
static pthread_mutex_t gHgfsActiveChannelLock;       /* Current active channel lock. */

static struct list_head gHgfsPendingRequests;        /* Pending requests queue. */
static pthread_mutex_t gHgfsPendingRequestsLock;     /* Pending requests queue lock. */


#define HgfsRequestId(req) ((HgfsRequest *)req)->id

static void HgfsTransportChannelClose(HgfsTransportChannel **channel);

/*
 * Private function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportChannelOpen --
 *
 *     Open a new workable channel.
 *
 * Results:
 *     TRUE on success and the new channel, otherwise FALSE and NULL.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsTransportChannelOpen(HgfsTransportChannel **channel) // IN: active channel
{
   Bool result = FALSE;

   *channel = HgfsBdChannelInit();
   if (NULL != *channel) {
      if ((*channel)->ops.open(*channel)) {
         result = TRUE;
      } else {
         HgfsTransportChannelClose(channel);
      }
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportChannelClose --
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
HgfsTransportChannelClose(HgfsTransportChannel **channel) // active channel
{
   if (NULL != *channel) {
      HgfsTransportChannel *closeChannel = *channel;

      closeChannel->ops.close(closeChannel);
      closeChannel->ops.exit(closeChannel);
      *channel = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportChannelReset --
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
HgfsTransportChannelReset(HgfsTransportChannel **channel) // IN: active channel
{
   Bool ret = FALSE;
   HgfsTransportChannelClose(channel);
   ret = HgfsTransportChannelOpen(channel);
   LOG(8, ("Result: %s.\n", ret ? "TRUE" : "FALSE"));
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportEnqueueRequest --
 *
 *     Add the request to the gHgfsPendingRequests queue.
 *
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsTransportEnqueueRequest(HgfsReq *req)   // IN: Request to add
{
   ASSERT(req);

   pthread_mutex_lock(&gHgfsPendingRequestsLock);
   list_add_tail(&req->list, &gHgfsPendingRequests);
   pthread_mutex_unlock(&gHgfsPendingRequestsLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTransportDequeueRequest --
 *
 *     Removes the request from the gHgfsPendingRequests queue.
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
HgfsTransportDequeueRequest(HgfsReq *req)   // IN: Request to dequeue
{
   ASSERT(req);

   pthread_mutex_lock(&gHgfsPendingRequestsLock);
   if (!list_empty(&req->list)) {
      list_del_init(&req->list);
   }
   pthread_mutex_unlock(&gHgfsPendingRequestsLock);
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
   LOG(8, ("Entered.\n"));
   LOG(6, ("Req id: %d\n", id));
   /*
    * Search through gHgfsPendingRequests queue for the matching id and wake up
    * the associated waiting process. Delete the req from the queue.
    */
   pthread_mutex_lock(&gHgfsPendingRequestsLock);
   list_for_each_safe(cur, next, &gHgfsPendingRequests) {
      HgfsReq *req;
      req = list_entry(cur, HgfsReq, list);
      if (req->id == id) {
         ASSERT(req->state == HGFS_REQ_STATE_SUBMITTED);
         HgfsCompleteReq(req, receivedPacket, receivedSize);
         found = TRUE;
         break;
      }
   }
   pthread_mutex_unlock(&gHgfsPendingRequestsLock);

   if (!found) {
      LOG(4, ("No matching id, dropping reply.\n"));
   }
   LOG(8, ("Exited.\n"));
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

   /* Walk through gHgfsPendingRequests queue and reply them with error. */
   pthread_mutex_lock(&gHgfsPendingRequestsLock);
   list_for_each_safe(cur, next, &gHgfsPendingRequests) {
      HgfsReq *req;
      HgfsReply reply;

      req = list_entry(cur, HgfsReq, list);
      LOG(6, ("Injecting error reply to req id: %d\n", req->id));
      HgfsCompleteReq(req, (char *)&reply, sizeof reply);
   }
   pthread_mutex_unlock(&gHgfsPendingRequestsLock);
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
   ASSERT(req->payloadSize <= HGFS_LARGE_PACKET_MAX);

   pthread_mutex_lock(&gHgfsActiveChannelLock);

   /* Try opening the channel. */
   if (NULL == gHgfsActiveChannel &&
       !HgfsTransportChannelOpen(&gHgfsActiveChannel)) {
      pthread_mutex_unlock(&gHgfsActiveChannelLock);
      return -EPROTO;
   }

   ASSERT(gHgfsActiveChannel->ops.send);

   HgfsTransportEnqueueRequest(req);

   ret = gHgfsActiveChannel->ops.send(gHgfsActiveChannel, req);
   if (ret < 0) {
      LOG(4, ("Send failed, status = %d. Try reopening the channel ...\n",
              ret));
      if (gHgfsActiveChannel->ops.open(gHgfsActiveChannel) &&
          HgfsTransportChannelReset(&gHgfsActiveChannel)) {
         ret = gHgfsActiveChannel->ops.send(gHgfsActiveChannel, req);
      }
   }

   ASSERT(req->state == HGFS_REQ_STATE_COMPLETED ||
          req->state == HGFS_REQ_STATE_SUBMITTED ||
          req->state == HGFS_REQ_STATE_UNSENT);

   pthread_mutex_unlock(&gHgfsActiveChannelLock);

   if (ret < 0) {
      HgfsTransportDequeueRequest(req);
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
 *     Zero on success and negative one on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

int
HgfsTransportInit(void)
{
   int res;
   INIT_LIST_HEAD(&gHgfsPendingRequests);
   res = pthread_mutex_init(&gHgfsPendingRequestsLock, NULL);
   if( res != 0) {
      return -1;
   }
   res = pthread_mutex_init(&gHgfsActiveChannelLock, NULL);
   if( res != 0) {
      return -1;
   }
   gHgfsActiveChannel = NULL;
   return 0;
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
   LOG(8, ("Entered.\n"));
   pthread_mutex_lock(&gHgfsActiveChannelLock);
   HgfsTransportChannelClose(&gHgfsActiveChannel);
   pthread_mutex_unlock(&gHgfsActiveChannelLock);

   ASSERT(list_empty(&gHgfsPendingRequests));
   LOG(8, ("Exited.\n"));
}

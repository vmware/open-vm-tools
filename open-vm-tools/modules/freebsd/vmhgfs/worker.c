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
 * worker.c --
 *
 *	Worker thread to process issue Guest -> Host Hgfs requests.
 */

#if defined __FreeBSD__
#  include <sys/libkern.h>
#endif

#include "hgfs_kernel.h"
#include "request.h"
#include "requestInt.h"
#include "os.h"
#include "channel.h"


/*
 * Local data
 */

/*
 * Process structure filled in when the worker thread is created.
 */
OS_THREAD_T hgfsKReqWorkerThread;

/*
 * See requestInt.h.
 */
HgfsKReqWState hgfsKReqWorkerState;
HgfsTransportChannel *gHgfsChannel = NULL;
OS_MUTEX_T *gHgfsChannelLock = NULL;


/*
 * Global (module) functions
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

Bool
HgfsSetupNewChannel(void)
{
   Bool ret;

   os_mutex_lock(gHgfsChannelLock);

   if (gHgfsChannel && gHgfsChannel->status == HGFS_CHANNEL_CONNECTED) {
      ret = TRUE;
      goto exit;
   }

   gHgfsChannel = HgfsGetVmciChannel();
   if (gHgfsChannel) {
      if ((ret = gHgfsChannel->ops.open(gHgfsChannel))) {
         goto exit;
      }
   }

   /* Every client using this code is expected to have backdoor enabled. */
   gHgfsChannel = HgfsGetBdChannel();
   ret = gHgfsChannel->ops.open(gHgfsChannel);

exit:
   if (ret) {
      gHgfsChannel->status = HGFS_CHANNEL_CONNECTED;
      DEBUG(VM_DEBUG_ALWAYS, "Channel: %s\n", gHgfsChannel->name);
   } else {
      gHgfsChannel->status = HGFS_CHANNEL_NOTCONNECTED;
   }

   os_mutex_unlock(gHgfsChannelLock);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsKReqWorker --
 *
 *      Main routine for Hgfs client worker thread.  This thread is responsible
 *      for all Hgfs communication with the host via the backdoor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsKReqWorker(void *arg)
{
   DblLnkLst_Links *currNode, *nextNode;
   HgfsKReqWState *ws = (HgfsKReqWState *)arg;
   HgfsKReqObject *req;
   int ret = 0;
   HgfsTransportChannel *channel;

   ws->running = TRUE;

   gHgfsChannelLock = os_mutex_alloc_init(HGFS_FS_NAME "_channellck");
   if (!gHgfsChannelLock) {
      goto exit;
   }

   ret = HgfsSetupNewChannel();
   if (!ret) {
      DEBUG(VM_DEBUG_INFO, "VMware hgfs: %s: ohoh no channel yet.\n", __func__);
   }

   for (;;) {
      /*
       * This loop spends most of its time sleeping until signalled by another
       * thread.  We expect to be signalled only if either there is work to do
       * or if the module is being unloaded.
       */

      os_mutex_lock(hgfsKReqWorkItemLock);

      while (!ws->exit && !DblLnkLst_IsLinked(&hgfsKReqWorkItemList)) {
         os_cv_wait(&hgfsKReqWorkItemCv, hgfsKReqWorkItemLock);
      }

      if (ws->exit) {
         /* Note that the list lock is still held. */
         break;
      }

      /*
       * We have work to do!  Hooray!  Start by locking the request and pulling
       * it from the work item list.  (The list's reference is transferred to
       * us, so we'll decrement the request's reference count when we're
       * finished with it.)
       *
       * With the request locked, make a decision based on the request's state.
       * Typically a request will be in the SUBMITTED state, but if its owner
       * aborted an operation or the file system cancelled it, it may be listed
       * as ABANDONED.  If either of the latter are true, then we don't bother
       * with any further processing.
       *
       * Because we're not sure how long the backdoor operation will take, we
       * yield the request's state lock before calling HgfsBd_Dispatch.  Upon
       * return, we must test the state again (see above re: cancellation),
       * and then we finally update the state & signal any waiters.
       */

      currNode = hgfsKReqWorkItemList.next;
      DblLnkLst_Unlink1(currNode);
      req = DblLnkLst_Container(currNode, HgfsKReqObject, pendingNode);

      os_mutex_lock(req->stateLock);

      channel = req->channel;
      switch (req->state) {
      case HGFS_REQ_SUBMITTED:
         if (channel->status != HGFS_CHANNEL_CONNECTED) {
            req->state = HGFS_REQ_ERROR;
            os_cv_signal(&req->stateCv);
            os_mutex_unlock(req->stateLock);
            os_mutex_unlock(hgfsKReqWorkItemLock);
            goto done;
         }
         break;
      case HGFS_REQ_ABANDONED:
      case HGFS_REQ_ERROR:
         os_mutex_unlock(req->stateLock);
         os_mutex_unlock(hgfsKReqWorkItemLock);
         goto done;
      default:
         panic("Request object (%p) in unknown state: %u", req, req->state);
      }
      os_mutex_unlock(req->stateLock);

      /*
       * We're done with the work item list for now.  Unlock it and let the file
       * system add requests while we're busy.
       */
      os_mutex_unlock(hgfsKReqWorkItemLock);

      ret = channel->ops.send(gHgfsChannel, req);

      if (ret != 0) {
         /*
          * If the channel was previously open, make sure it's dead and gone
          * now. We do this because subsequent requests deserve a chance to
          * reopen it.
          */
         os_mutex_lock(gHgfsChannelLock);
         gHgfsChannel->ops.close(gHgfsChannel);
         os_mutex_unlock(gHgfsChannelLock);
      }

done:

      /*
       * If transport wants to process request async then it should have
       * taken reference for itself. If not we free it.
       */
      if (os_add_atomic(&req->refcnt, -1) == 1) {
         os_zone_free(hgfsKReqZone, req);
      }
   }

   /*
    * NB:  The work item lock is still held.
    *
    * XXX There may be some request on the sentList. what should we do about them ?
    */

   /*
    * We're signaled to exit.  Remove any items from the pending request list
    * before exiting.
    */
   DblLnkLst_ForEachSafe(currNode, nextNode, &hgfsKReqWorkItemList) {
      req = DblLnkLst_Container(currNode, HgfsKReqObject, pendingNode);
      DblLnkLst_Unlink1(currNode);
      os_mutex_lock(req->stateLock);
      req->state = HGFS_REQ_ERROR;
      os_cv_signal(&req->stateCv);
      os_mutex_unlock(req->stateLock);

      /*
       * If we held the final reference to a request, free it.
       */
      if (os_add_atomic(&req->refcnt, -1) == 1) {
         os_zone_free(hgfsKReqZone, req);
      }
   }

   os_mutex_unlock(hgfsKReqWorkItemLock);

   ws->running = FALSE;

   if (gHgfsChannel && gHgfsChannel->status == HGFS_CHANNEL_CONNECTED) {
      gHgfsChannel->ops.close(gHgfsChannel);
   }

   if (gHgfsChannelLock) {
      os_mutex_free(gHgfsChannelLock);
   }
exit:
   os_thread_exit(0);
}

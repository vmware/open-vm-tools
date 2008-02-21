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
 * bdhandler.c --
 *
 * Background thread for handling backdoor requests and replies.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include "compat_completion.h"
#include "compat_kernel.h"
#include "compat_list.h"
#include "compat_sched.h"
#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_version.h"

/* Must be included after sched.h. */
#include <linux/smp_lock.h>

#include "hgfsBd.h"
#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "bdhandler.h"
#include "module.h"
#include "request.h"
#include "vm_assert.h"
#include "rpcout.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 9)
int errno;  /* compat_exit() needs global errno variable. */
#endif

static inline void HgfsWakeWaitingClient(HgfsReq *req);
static inline void HgfsCompleteReq(HgfsReq *req,
                                   char const *reply,
                                   size_t replySize);
static void HgfsSendUnsentReqs(void);

/*
 * Private function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsWakeWaitingClient --
 *
 *    Wakes up the client process waiting for the reply to this
 *    request.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static inline void
HgfsWakeWaitingClient(HgfsReq *req)  // IN: Request
{
   ASSERT(req);
   wake_up(&req->queue);
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

static inline void 
HgfsCompleteReq(HgfsReq *req,       // IN: Request
                char const *reply,  // IN: Reply packet
                size_t replySize)   // IN: Size of reply packet
{
   ASSERT(replySize <= HGFS_PACKET_MAX);

   memcpy(HGFS_REQ_PAYLOAD(req), reply, replySize);
   req->payloadSize = replySize;
   req->state = HGFS_REQ_STATE_COMPLETED;
   list_del_init(&req->list);
   HgfsWakeWaitingClient(req);         
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSendUnsentReqs --
 *
 *      Process the unsent list and send requests to the backdoor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsSendUnsentReqs(void)
{
   char const *replyPacket;
   struct list_head *cur, *tmp;
   HgfsReq *req;
   size_t payloadSize;

   spin_lock(&hgfsBigLock);
   list_for_each_safe(cur, tmp, &hgfsReqsUnsent) {
      req = list_entry(cur, HgfsReq, list);

      /* 
       * A big "wtf" from the driver is in order. Perhaps by "wtf" I really
       * mean BUG_ON().
       */
      ASSERT(req->state == HGFS_REQ_STATE_UNSENT);
      if (req->state != HGFS_REQ_STATE_UNSENT) {
         LOG(2, (KERN_DEBUG "VMware hgfs: HgfsSendUnsentReqs: Found request "
                 "on unsent list in the wrong state, ignoring\n"));
         continue;
      }

      ASSERT(req->payloadSize <= HGFS_PACKET_MAX);
      payloadSize = req->payloadSize;
      LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendUnsentReqs: Sending packet "
              "over backdoor\n"));

      /* 
       * We should attempt to reopen the backdoor channel with every request, 
       * because the HGFS server in the host can be enabled or disabled at any 
       * time.
       */
      if (!HgfsBd_OpenBackdoor(&hgfsRpcOut)) {
         req->state = HGFS_REQ_STATE_ERROR;
         list_del_init(&req->list);
         printk(KERN_WARNING "VMware hgfs: HGFS is disabled in the host\n");
         HgfsWakeWaitingClient(req);
      } else if (HgfsBd_Dispatch(hgfsRpcOut, HGFS_REQ_PAYLOAD(req),
                                 &payloadSize, &replyPacket) == 0) {

         /* Request sent successfully. Copy the reply and wake the client. */
         HgfsCompleteReq(req, replyPacket, payloadSize);
         LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendUnsentReqs: Backdoor "
                 "reply received\n"));
      } else {
         
         /* Pass the error into the request. */
         req->state = HGFS_REQ_STATE_ERROR;
         list_del_init(&req->list);
         LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendUnsentReqs: Backdoor "
                 "error\n"));
         HgfsWakeWaitingClient(req);

         /*
          * If the channel was previously open, make sure it's dead and gone
          * now. We do this because subsequent requests deserve a chance to
          * reopen it.
          */
         HgfsBd_CloseBackdoor(&hgfsRpcOut);
      }
   }
   spin_unlock(&hgfsBigLock);
}


/*
 * Public function implementations.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsResetOps --
 *
 *      Reset ops with more than one opcode back to the desired opcode.
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
HgfsResetOps(void)
{
   atomic_set(&hgfsVersionOpen, HGFS_OP_OPEN_V2);
   atomic_set(&hgfsVersionGetattr, HGFS_OP_GETATTR_V2);
   atomic_set(&hgfsVersionSetattr, HGFS_OP_SETATTR_V2);
   atomic_set(&hgfsVersionSearchRead, HGFS_OP_SEARCH_READ_V2);
   atomic_set(&hgfsVersionCreateDir, HGFS_OP_CREATE_DIR_V2);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBdHandler --
 *
 *    Function run in background thread to pick up HGFS requests from
 *    the filesystem half of the driver, send them over the backdoor,
 *    get replies, and send them back to the filesystem.
 *
 * Results:
 *    Always returns zero.
 *
 * Side effects:
 *    Processes entries from hgfsReqQ.
 *
 *----------------------------------------------------------------------
 */

int 
HgfsBdHandler(void *data)
{
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsBdHandler: Thread starting\n"));
   compat_daemonize(HGFS_NAME);
   compat_set_freezable();

   for (;;) {

      /* Sleep, waiting for a request, a poll time, or exit. */
      wait_event_interruptible(hgfsReqThreadWait,
                               test_bit(HGFS_REQ_THREAD_SEND, 
                                        &hgfsReqThreadFlags) ||
                               test_bit(HGFS_REQ_THREAD_EXIT, 
                                        &hgfsReqThreadFlags));
      
      /* 
       * First, check for suspend. I'm not convinced that this actually
       * has to come first, but whatever.
       */
      if (compat_try_to_freeze()) {
	 LOG(6, (KERN_DEBUG 
		 "VMware hgfs: HgfsBdHandler: Closing backdoor after resume\n"));
	 HgfsBd_CloseBackdoor(&hgfsRpcOut);
      }

      /* Send outgoing requests. */
      if (test_and_clear_bit(HGFS_REQ_THREAD_SEND, &hgfsReqThreadFlags)) {
         LOG(8, (KERN_DEBUG "VMware hgfs: HgfsBdHandler: Sending requests\n"));
         HgfsSendUnsentReqs();
      }

      /* Kill yourself. */
      if (test_and_clear_bit(HGFS_REQ_THREAD_EXIT, &hgfsReqThreadFlags)) { 
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsBdHandler: Told to exit\n"));
         break;
      }
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsBdHandler: Closing backdoor\n"));
   HgfsBd_CloseBackdoor(&hgfsRpcOut);

   /* Signal our parent that we're exiting, and exit, all at once. */
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsBdHandler: Thread exiting\n"));
   compat_complete_and_exit(&hgfsReqThreadDone, 0);
   NOT_REACHED();
}

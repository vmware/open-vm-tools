/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * hgfsBdGlue.c --
 *
 * Glue to communicate directly with backdoor code instead of offloading
 * it to guestd
 */

#include "hgfsSolaris.h"
#include "request.h"
#include "hgfsBdGlue.h"
#include "hgfsBd.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "debug.h"

static RpcOut *hgfsRpcOut;
static void *packetBuffer;

/*
 * Public function implementations.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBackdoorSendRequest --
 *
 *    Send one request through backdoor and wait for the result.
 *
 * Results:
 *    0 on success, standard UNIX error code upon failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsBackdoorSendRequest(HgfsReq *req)     // IN/OUT: Request to be sent
{
   char const *replyPacket;
   size_t packetSize;

   ASSERT(req->state == HGFS_REQ_SUBMITTED);

   ASSERT(req->packetSize <= HGFS_PACKET_MAX);
   bcopy(req->packet, packetBuffer, req->packetSize);
   packetSize = req->packetSize;

   DEBUG(VM_DEBUG_COMM,
         "HgfsBackdoorSendRequest: Sending packet over backdoor\n");

   /*
    * We should attempt to reopen the backdoor channel with every request,
    * because the HGFS server in the host can be enabled or disabled at any
    * time.
    */
   if (!HgfsBd_OpenBackdoor(&hgfsRpcOut)) {

      DEBUG(VM_DEBUG_COMM,
            "HgfsBackdoorSendRequest: HGFS is disabled in the host\n");

      req->state = HGFS_REQ_ERROR;
      return ENOSYS;

   } else if (HgfsBd_Dispatch(hgfsRpcOut, packetBuffer,
                              &packetSize, &replyPacket) == 0) {

      DEBUG(VM_DEBUG_COMM,
            "HgfsBackdoorSendRequest: backdoor reply received\n");

      /* Request was sent successfully. Copy the reply and return to the client. */
      ASSERT(packetSize <= HGFS_PACKET_MAX);

      bcopy(replyPacket, req->packet, packetSize);
      req->packetSize = packetSize;
      req->state = HGFS_REQ_COMPLETED;

   } else {

      DEBUG(VM_DEBUG_COMM,
            "HgfsBackdoorSendRequest: backdoor error\n");

      /* Pass the error into the request. */
      req->state = HGFS_REQ_ERROR;

      /*
       * If the channel was previously open, make sure it's dead and gone
       * now. We do this because subsequent requests deserve a chance to
       * reopen it.
       */
      HgfsBd_CloseBackdoor(&hgfsRpcOut);
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBackdoorCancelRequest --
 *
 *    Cancel request stub. Since backdoor is synchronous transport this
 *    function should never be called in practice.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsBackdoorCancelRequest(HgfsReq *req)      // IN: Request to be cancelled
{
   DEBUG(VM_DEBUG_COMM, "HgfsBackdoorCancelRequest: %p\n", req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBackdoorInit --
 *
 *    This function initializes backdoor transport by allocating transfer
 *    buffer.
 *
 * Results:
 *    TRUE if buffer was allocated succsessfully, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Bool
HgfsBackdoorInit(void)
{
   packetBuffer = HgfsBd_GetBuf();

   return packetBuffer != NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBackdoorCleanup --
 *
 *    This function closes backdoor channel. It is supposed to be
 *    called when we unmount the filesystem.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

void
HgfsBackdoorCleanup(void)
{
   DEBUG(VM_DEBUG_COMM, "HgfsBackdoorCleanup: Closing backdoor\n");

   HgfsBd_CloseBackdoor(&hgfsRpcOut);
   HgfsBd_PutBuf(packetBuffer);
}


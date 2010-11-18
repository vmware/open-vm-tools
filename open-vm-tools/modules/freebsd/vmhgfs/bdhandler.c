/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 */

#include "hgfsBd.h"
#include "rpcout.h"
#include "channel.h"

static Bool HgfsBdChannelOpen(HgfsTransportChannel *channel);
static void HgfsBdChannelClose(HgfsTransportChannel *channel);
static HgfsKReqObject * HgfsBdChannelAllocate(size_t payloadSize, int flags);
void HgfsBdChannelFree(HgfsKReqObject *req, size_t payloadSize);
static int HgfsBdChannelSend(HgfsTransportChannel *channel, HgfsKReqObject *req);

static HgfsTransportChannel gBdChannel = {
   .name = "backdoor",
   .ops.open = HgfsBdChannelOpen,
   .ops.close = HgfsBdChannelClose,
   .ops.allocate = HgfsBdChannelAllocate,
   .ops.free = HgfsBdChannelFree,
   .ops.send = HgfsBdChannelSend,
   .priv = NULL,
   .status = HGFS_CHANNEL_NOTCONNECTED
};


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelOpen --
 *
 *      Open the backdoor in an idempotent way.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsBdChannelOpen(HgfsTransportChannel *channel) // IN: Channel
{
   Bool ret;

   if ((ret = HgfsBd_OpenBackdoor((RpcOut **)&channel->priv))) {
      DEBUG(VM_DEBUG_INFO, "VMware hgfs: %s: backdoor opened.\n", __func__);
      ASSERT(channel->priv != NULL);
      channel->status = HGFS_CHANNEL_CONNECTED;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelClose --
 *
 *      Close the backdoor in an idempotent way.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsBdChannelClose(HgfsTransportChannel *channel) // IN: Channel
{
   int ret;

   if (channel->priv == NULL) {
      return;
   }

   ret = HgfsBd_CloseBackdoor((RpcOut **)&channel->priv);
   if (!ret) {
      DEBUG(VM_DEBUG_FAIL, "VMware hgfs: %s: Failed to close backdoor.\n", __func__);
   } else {
      DEBUG(VM_DEBUG_INFO, "VMware hgfs: %s: backdoor closed.\n", __func__);
   }
   channel->status = HGFS_CHANNEL_NOTCONNECTED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelAllocate --
 *
 *      Allocate request in a way that is suitable for sending through
 *      backdoor.
 *
 * Results:
 *      NULL on failure; otherwise address of the new request.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsKReqObject *
HgfsBdChannelAllocate(size_t payloadSize,   // IN: Size of allocation
                      int flags)            // IN:
{
   return os_malloc(payloadSize, flags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelFree --
 *
 *     Free previously allocated request.
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
HgfsBdChannelFree(HgfsKReqObject *req,    // IN:
                  size_t payloadSize)     // IN:
{
   ASSERT(req);
   os_free(req, payloadSize);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBdChannelSend --
 *
 *     Send a request via backdoor.
 *
 * Results:
 *     0 on success, negative error on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsBdChannelSend(HgfsTransportChannel *channel, // IN: Channel
                  HgfsKReqObject *req)           // IN: request to send
{
   char const *replyPacket = NULL;
   int ret;

   ASSERT(req);

   DEBUG(VM_DEBUG_INFO, "VMware hgfs: %s: backdoor sending.\n", __func__);

   bcopy(HGFS_SYNC_REQREP_CLIENT_CMD, req->__rpc_packet._command,
         HGFS_SYNC_REQREP_CLIENT_CMD_LEN);

   ret = HgfsBd_Dispatch(channel->priv, req->payload, &req->payloadSize,
                         &replyPacket);
   os_mutex_lock(req->stateLock);

   /*
    * We have a response.  (Maybe.)  Re-lock the request, update its state,
    * etc.
    */
   if ((ret == 0) && (req->state == HGFS_REQ_SUBMITTED)) {
      DEBUG(VM_DEBUG_INFO, "VMware hgfs: %s: Success in backdoor.\n", __func__);
      bcopy(replyPacket, req->payload, req->payloadSize);
      req->state = HGFS_REQ_COMPLETED;
   } else {
      DEBUG(VM_DEBUG_INFO, "hgfs: %s: Error in backdoor.\n", __func__);
      req->state = HGFS_REQ_ERROR;
   }

   os_cv_signal(&req->stateCv);
   os_mutex_unlock(req->stateLock);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetBdChannel --
 *
 *     Get backdoor channel.
 *
 * Results:
 *     Always return pointer to back door channel.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

HgfsTransportChannel*
HgfsGetBdChannel(void)
{
   return &gBdChannel;
}


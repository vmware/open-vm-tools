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
 * bdhandler.c --
 *
 * Background thread for handling backdoor requests and replies.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>

#include "transport.h"
#include "hgfsBd.h"
#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "rpcout.h"
#include "vm_assert.h"


static Bool HgfsBdChannelOpen(HgfsTransportChannel *channel);
static void HgfsBdChannelClose(HgfsTransportChannel *channel);
static HgfsReq * HgfsBdChannelAllocate(size_t payloadSize);
void HgfsBdChannelFree(HgfsReq *req);
static int HgfsBdChannelSend(HgfsTransportChannel *channel, HgfsReq *req);

static HgfsTransportChannel channel = {
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
   Bool ret = FALSE;

   ASSERT(channel->status == HGFS_CHANNEL_NOTCONNECTED);

   if (HgfsBd_OpenBackdoor((RpcOut **)&channel->priv)) {
      LOG(8, ("VMware hgfs: %s: backdoor opened.\n", __func__));
      ret = TRUE;
      ASSERT(channel->priv != NULL);
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
   ASSERT(channel->priv != NULL);

   HgfsBd_CloseBackdoor((RpcOut **)&channel->priv);
   ASSERT(channel->priv == NULL);

   LOG(8, ("VMware hgfs: %s: backdoor closed.\n", __func__));
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

static HgfsReq *
HgfsBdChannelAllocate(size_t payloadSize) // IN: size of requests payload
{
   HgfsReq *req;

   req = kmalloc(sizeof(*req) + HGFS_SYNC_REQREP_CLIENT_CMD_LEN + payloadSize,
                 GFP_KERNEL);
   if (likely(req)) {
      /* Setup the packet prefix. */
      memcpy(req->buffer, HGFS_SYNC_REQREP_CLIENT_CMD,
             HGFS_SYNC_REQREP_CLIENT_CMD_LEN);

      req->payload = req->buffer + HGFS_SYNC_REQREP_CLIENT_CMD_LEN;
      req->bufferSize = payloadSize;
   }

   return req;
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
HgfsBdChannelFree(HgfsReq *req)
{
   ASSERT(req);
   kfree(req);
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
                  HgfsReq *req)                  // IN: request to send
{
   char const *replyPacket = NULL;
   size_t payloadSize;
   int ret;

   ASSERT(req);
   ASSERT(req->state == HGFS_REQ_STATE_UNSENT);
   ASSERT(req->payloadSize <= req->bufferSize);

   LOG(8, ("VMware hgfs: %s: backdoor sending.\n", __func__));
   payloadSize = req->payloadSize;
   ret = HgfsBd_Dispatch(channel->priv, HGFS_REQ_PAYLOAD(req), &payloadSize,
                         &replyPacket);
   if (ret == 0) {
      LOG(8, ("VMware hgfs: %s: Backdoor reply received.\n", __func__));
      /* Request sent successfully. Copy the reply and wake the client. */
      ASSERT(replyPacket);
      ASSERT(payloadSize <= req->bufferSize);
      memcpy(HGFS_REQ_PAYLOAD(req), replyPacket, payloadSize);
      req->payloadSize = payloadSize;
      HgfsCompleteReq(req);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetBdChannel --
 *
 *     Initialize backdoor channel.
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
   return &channel;
}

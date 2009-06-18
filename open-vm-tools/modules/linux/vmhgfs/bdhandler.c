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

#include <linux/errno.h>

#include "bdhandler.h"
#include "hgfsBd.h"
#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "rpcout.h"
#include "transport.h"
#include "vm_assert.h"

HgfsTransportChannel bdChannel;


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

   compat_mutex_lock(&channel->connLock);
   switch (channel->status) {
   case HGFS_CHANNEL_UNINITIALIZED:
      ret = FALSE;
      break;
   case HGFS_CHANNEL_CONNECTED:
      ret = TRUE;
      break;
   case HGFS_CHANNEL_NOTCONNECTED:
      if (HgfsBd_OpenBackdoor((RpcOut **)&channel->priv)) {
         LOG(8, ("VMware hgfs: %s: backdoor opened.\n", __func__));
         bdChannel.status = HGFS_CHANNEL_CONNECTED;
         ret = TRUE;
         ASSERT(channel->priv != NULL);
      } else {
         ret = FALSE;
      }
      break;
   default:
      ASSERT(0); /* Not reached. */
   }

   compat_mutex_unlock(&channel->connLock);
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
   compat_mutex_lock(&channel->connLock);
   if (channel->status == HGFS_CHANNEL_CONNECTED) {
      ASSERT(channel->priv != NULL);
      HgfsBd_CloseBackdoor((RpcOut **)&channel->priv);
      ASSERT(channel->priv == NULL);
      channel->status = HGFS_CHANNEL_NOTCONNECTED;
   }
   compat_mutex_unlock(&channel->connLock);
   LOG(8, ("VMware hgfs: %s: backdoor closed.\n", __func__));
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
   ASSERT(req->payloadSize <= HGFS_PACKET_MAX);

   compat_mutex_lock(&channel->connLock);

   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Backdoor not opened\n", __func__));
      compat_mutex_unlock(&channel->connLock);
      return -ENOTCONN;
   }

   payloadSize = req->payloadSize;
   LOG(8, ("VMware hgfs: %s: backdoor sending.\n", __func__));
   ret = HgfsBd_Dispatch(channel->priv, HGFS_REQ_PAYLOAD(req), &payloadSize,
                         &replyPacket);
   if (ret == 0) {
      /* Request sent successfully. Copy the reply and wake the client. */
      ASSERT(replyPacket);
      HgfsCompleteReq(req, replyPacket, payloadSize);
      LOG(8, (KERN_DEBUG "VMware hgfs: HgfsSendUnsentReqs: Backdoor "
              "reply received\n"));
   } else {
      channel->priv = NULL;
      channel->status = HGFS_CHANNEL_NOTCONNECTED;
   }
   compat_mutex_unlock(&channel->connLock);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBdChannelExit --
 *
 *     Tear down the channel.
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
HgfsBdChannelExit(HgfsTransportChannel *channel)  // IN
{
   compat_mutex_lock(&channel->connLock);
   if (channel->priv != NULL) {
      HgfsBd_CloseBackdoor((RpcOut **)&channel->priv);
      ASSERT(channel->priv == NULL);
   }
   channel->status = HGFS_CHANNEL_UNINITIALIZED;
   compat_mutex_unlock(&channel->connLock);
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
   bdChannel.name = "backdoor";
   bdChannel.ops.open = HgfsBdChannelOpen;
   bdChannel.ops.close = HgfsBdChannelClose;
   bdChannel.ops.send = HgfsBdChannelSend;
   bdChannel.ops.recv = NULL;
   bdChannel.ops.exit = HgfsBdChannelExit;
   bdChannel.priv = NULL;
   compat_mutex_init(&bdChannel.connLock);
   bdChannel.status = HGFS_CHANNEL_NOTCONNECTED;
   return &bdChannel;
}

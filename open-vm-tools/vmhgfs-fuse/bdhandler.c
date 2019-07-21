/*********************************************************
 * Copyright (C) 2013,2019 VMware, Inc. All rights reserved.
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
 * bdhandler.c --
 *
 * Background thread for handling backdoor requests and replies.
 */

/* Must come before any kernel header file. */


#include "bdhandler.h"
#include "hgfsBd.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "rpcout.h"
#include "transport.h"
#include "vm_assert.h"

static HgfsTransportChannel bdChannel;


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelOpen --
 *
 *      Open the backdoor in an idempotent way.
 *
 * Results:
 *      Existing or updated channel status, HGFS_CHANNEL_CONNECTED on success.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsChannelStatus
HgfsBdChannelOpen(HgfsTransportChannel *channel) // IN: Channel
{
   pthread_mutex_lock(&channel->connLock);
   switch (channel->status) {
   case HGFS_CHANNEL_UNINITIALIZED:
      LOG(8, ("Backdoor uninitialized.\n"));
      break;
   case HGFS_CHANNEL_CONNECTED:
      LOG(8, ("Backdoor already connected.\n"));
      break;
   case HGFS_CHANNEL_NOTCONNECTED:
      if (HgfsBd_OpenBackdoor((RpcOut **)&channel->priv)) {
         LOG(8, ("Backdoor opened and connected.\n"));
         channel->status = HGFS_CHANNEL_CONNECTED;
         ASSERT(channel->priv != NULL);
      } else {
         LOG(8, ("ERROR: Backdoor cannot connect.\n"));
      }
      break;
   default:
      ASSERT(0); /* Not reached. */
      LOG(2, ("ERROR: Backdoor status %d is unknown resetting.\n",
              channel->status));
      channel->status = HGFS_CHANNEL_UNINITIALIZED;
   }

   pthread_mutex_unlock(&channel->connLock);
   return channel->status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdChannelCloseInt --
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
HgfsBdChannelCloseInt(HgfsTransportChannel *channel) // IN: Channel
{
   if (channel->status == HGFS_CHANNEL_CONNECTED) {
      ASSERT(channel->priv != NULL);
      HgfsBd_CloseBackdoor((RpcOut **)&channel->priv);
      ASSERT(channel->priv == NULL);
      channel->status = HGFS_CHANNEL_NOTCONNECTED;
   }
   LOG(8, ("Backdoor closed.\n"));
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
   pthread_mutex_lock(&channel->connLock);
   HgfsBdChannelCloseInt(channel);
   pthread_mutex_unlock(&channel->connLock);
   LOG(8, ("Backdoor closed.\n"));
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
   ASSERT(req->payloadSize <= HgfsLargePacketMax(FALSE));

   pthread_mutex_lock(&channel->connLock);

   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, ("Backdoor not opened.\n"));
      pthread_mutex_unlock(&channel->connLock);
      return -ENOTCONN;
   }

   payloadSize = req->payloadSize;
   LOG(8, ("Backdoor sending.\n"));
   ret = HgfsBd_Dispatch(channel->priv, HGFS_REQ_PAYLOAD(req), &payloadSize,
                         &replyPacket);
   if (ret == 0) {
      LOG(8, ("Backdoor reply received.\n"));
      /* Request sent successfully. Copy the reply and wake the client. */
      ASSERT(replyPacket);
      HgfsCompleteReq(req, replyPacket, payloadSize);
   } else {
      /* Map rpc failure to EIO. */
      ret = -EIO;
   }

   pthread_mutex_unlock(&channel->connLock);

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
   pthread_mutex_lock(&channel->connLock);
   HgfsBdChannelCloseInt(channel);
   channel->status = HGFS_CHANNEL_UNINITIALIZED;
   pthread_mutex_unlock(&channel->connLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsBdChannelInit --
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
HgfsBdChannelInit(void)
{
   bdChannel.name = "backdoor";
   bdChannel.ops.open = HgfsBdChannelOpen;
   bdChannel.ops.close = HgfsBdChannelClose;
   bdChannel.ops.send = HgfsBdChannelSend;
   bdChannel.ops.recv = NULL;
   bdChannel.ops.exit = HgfsBdChannelExit;
   bdChannel.priv = NULL;
   pthread_mutex_init(&bdChannel.connLock, NULL);
   bdChannel.status = HGFS_CHANNEL_NOTCONNECTED;
   return &bdChannel;
}

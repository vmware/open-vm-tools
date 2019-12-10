/*********************************************************
 * Copyright (C) 2008-2016,2018-2019 VMware, Inc. All rights reserved.
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

/**
 * @file bdoorChannel.c
 *
 *    Implements a backdoor-based RPC channel. This is based on the
 *    RpcIn / RpcOut libraries.
 */

#include "vm_assert.h"
#include "rpcChannelInt.h"
#if defined(NEED_RPCIN)
#include "rpcin.h"
#endif
#include "rpcout.h"
#include "util.h"
#include "debug.h"

typedef struct BackdoorChannel {
   RpcOut           *out;
} BackdoorChannel;


/**
 * Starts the RpcIn loop and the RpcOut channel.
 *
 * No-op if channels are already started.
 *
 * @param[in]  chan     The RPC channel instance.
 *
 * @return TRUE on success.
 */

static gboolean
BkdoorChannelStart(RpcChannel *chan)
{
   gboolean ret = TRUE;
   BackdoorChannel *bdoor = chan->_private;

#if defined(NEED_RPCIN)
   ret = chan->in == NULL || chan->inStarted;
   if (ret) {
      ret = RpcOut_start(bdoor->out);
      if (!ret) {
         if (chan->inStarted) {
            RpcIn_stop(chan->in);
            chan->inStarted = FALSE;
         }
      }
   }
#else
   ret = RpcOut_start(bdoor->out);
#endif
   chan->outStarted = ret;
   return ret;
}


/**
 * Stops a channel, keeping internal state so that it can be restarted later.
 * It's safe to call this function more than once.
 *
 * @internal This function does a best effort at tearing down the host-side
 *           channels, but if the host returns any failure, it still shuts
 *           down the guest channels. See bug 388777 for details.
 *
 * @param[in]  chan     The RPC channel instance.
 */

static void
BkdoorChannelStop(RpcChannel *chan)
{
   BackdoorChannel *bdoor = chan->_private;

   if (bdoor->out != NULL) {
      if (chan->outStarted) {
         RpcOut_stop(bdoor->out);
         chan->outStarted = FALSE;
      }
   } else {
      ASSERT(!chan->outStarted);
   }
}


/**
 * Shuts down the RpcIn channel. Due to the "split brain" nature of the backdoor,
 * if this function fails, it's possible that while the "out" channel was shut
 * down the "in" one wasn't, for example, although that's unlikely.
 *
 * @param[in]  chan     The RPC channel instance.
 */

static void
BkdoorChannelShutdown(RpcChannel *chan)
{
   BackdoorChannel *bdoor = chan->_private;
   BkdoorChannelStop(chan);
   RpcOut_Destruct(bdoor->out);
   g_free(bdoor);
   chan->_private = NULL;
}


/**
 * Sends the data using the RpcOut library.
 *
 * rpcStatus is valid only when function returns success.
 *
 * @param[in]  chan        The RPC channel instance.
 * @param[in]  data        Data to send.
 * @param[in]  dataLen     Number of bytes to send.
 * @param[out] rpcStatus   Status of RPC command.
 * @param[out] result      Response from other side.
 * @param[out] resultLen   Number of bytes in response.
 *
 * @return The status from the remote end (TRUE if RPC was sent successfully).
 */

static gboolean
BkdoorChannelSend(RpcChannel *chan,
                  char const *data,
                  size_t dataLen,
                  Bool *rpcStatus,
                  char **result,
                  size_t *resultLen)
{
   gboolean ret = FALSE;
   const char *reply;
   size_t replyLen;
   BackdoorChannel *bdoor = chan->_private;

   if (!chan->outStarted) {
      goto exit;
   }

   ret = RpcOut_send(bdoor->out, data, dataLen, rpcStatus, &reply, &replyLen);

   /*
    * This is a hack to try to work around bug 393650 without having to revert
    * to the old behavior of opening and closing an RpcOut channel for every
    * outgoing message. The issue here is that it's possible for the code to
    * try to write to the channel when a "reset" has just happened. In these
    * cases, the current RpcOut channel is not valid anymore, and we'll get an
    * error.
    *
    * So, if the error is one of those messages, restart the RpcOut channel and
    * try to send the message again. If this second attempt fails, then give up.
    *
    * This is not 100% break-proof: a reset can still occur after we open the
    * new channel and before we try to re-send the message. But that's a race
    * that we can't easily fix, and exists even in code that just uses the
    * RpcOut_SendOne() API. Also, if some host handler returns an error that
    * starts with "RpcOut:", it will trigger this; but I don't think we have
    * any such handlers.
    */
   if (!ret) {
      Debug("RpcOut failure, restarting channel.\n");
      RpcOut_stop(bdoor->out);
      if (RpcOut_start(bdoor->out)) {
         ret = RpcOut_send(bdoor->out, data, dataLen, rpcStatus,
                           &reply, &replyLen);
      } else {
         Warning("Couldn't restart RpcOut channel; bad things may happen "
                 "until the RPC channel is reset.\n");
         chan->outStarted = FALSE;
      }
   }

   /*
    * A lot of this logic is just replicated from rpcout.c:RpcOut_SendOneRaw().
    * Look there for comments about a few details.
    */
   if (result != NULL) {
      if (reply != NULL) {
         *result = Util_SafeMalloc(replyLen + 1);
         memcpy(*result, reply, replyLen);
         (*result)[replyLen] = '\0';
      } else {
         *result = NULL;
      }
   }

   if (resultLen != NULL) {
      *resultLen = replyLen;
   }

exit:
   return ret;
}


/**
 * Return the channel type.
 *
 * @param[in]  chan     RpcChannel
 *
 * @return backdoor channel type.
 */

static RpcChannelType
BkdoorChannelGetType(RpcChannel *chan)
{
   return RPCCHANNEL_TYPE_BKDOOR;
}


/**
 * Helper function to setup RpcChannel callbacks.
 *
 * @param[in]  chan     RpcChannel
 */

static void
BackdoorChannelSetCallbacks(RpcChannel *chan)
{
   static RpcChannelFuncs funcs = {
      BkdoorChannelStart,
      BkdoorChannelStop,
      BkdoorChannelSend,
      NULL,
      BkdoorChannelShutdown,
      BkdoorChannelGetType,
      NULL,
      NULL
   };

   ASSERT(chan);
   chan->funcs = &funcs;
}


/**
 * Creates a new RpcChannel channel that uses the backdoor for communication.
 *
 * @return A new channel instance (never NULL).
 */

RpcChannel *
BackdoorChannel_New(void)
{
   RpcChannel *ret;
   BackdoorChannel *bdoor;

   ret = RpcChannel_Create();
   bdoor = g_malloc0(sizeof *bdoor);

   bdoor->out = RpcOut_Construct();
   ASSERT(bdoor->out != NULL);

#if defined(NEED_RPCIN)
   ret->inStarted = FALSE;
#endif
   ret->outStarted = FALSE;

   BackdoorChannelSetCallbacks(ret);
   ret->_private = bdoor;
   g_mutex_init(&ret->outLock);

   return ret;
}


/**
 * Fall back to backdoor when another type of RpcChannel fails to start.
 *
 * @param[in]  chan     RpcChannel
 *
 * @return TRUE on success.
 */

gboolean
BackdoorChannel_Fallback(RpcChannel *chan)
{
   BackdoorChannel *bdoor;

   ASSERT(chan);
   ASSERT(chan->_private == NULL);

   bdoor = g_malloc0(sizeof *bdoor);
   bdoor->out = RpcOut_Construct();
   ASSERT(bdoor->out != NULL);

   BackdoorChannelSetCallbacks(chan);
   chan->_private = bdoor;

   return chan->funcs->start(chan);
}


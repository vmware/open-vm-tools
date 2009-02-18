/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
#include "rpcout.h"
#include "util.h"

/** Max amount of time (in .01s) that the RpcIn loop will sleep for. */
#define RPCIN_MAX_DELAY    10

typedef struct BackdoorChannel {
   RpcIn   *in;
   RpcOut  *out;
   gboolean inStarted;
   gboolean outStarted;
} BackdoorChannel;


/**
 * Stops a channel, optionally destroying the channel. It's safe to call
 * this function more than once.
 *
 * @param[in]  chan     The RPC channel instance.
 * @param[in]  destroy  Whether to destroy the channels.
 *
 * @return TRUE on success.
 */

static Bool
RpcInStopChannel(RpcChannel *chan,
                 gboolean destroy)
{
   gboolean ret = TRUE;
   BackdoorChannel *bdoor = chan->_private;

   ASSERT(chan->appName != NULL);
   if (bdoor->out != NULL) {
      if (!bdoor->outStarted || RpcOut_stop(bdoor->out)) {
         if (destroy) {
            RpcOut_Destruct(bdoor->out);
            bdoor->out = NULL;
         }
         bdoor->outStarted = FALSE;
      } else {
         ret = FALSE;
      }
   } else {
      ASSERT(!bdoor->outStarted);
   }

   if (bdoor->in != NULL) {
      if (!bdoor->inStarted || RpcIn_stop(bdoor->in)) {
         if (destroy) {
            RpcIn_Destruct(bdoor->in);
            bdoor->in = NULL;
         }
         bdoor->inStarted = FALSE;
      } else {
         ret = FALSE;
      }
   } else {
      ASSERT(!bdoor->inStarted);
   }

   return ret;
}


/**
 * Starts the RpcIn loop and the RpcOut channel.
 *
 * @param[in]  chan     The RPC channel instance.
 *
 * @return TRUE on success.
 */

static Bool
RpcInStart(RpcChannel *chan)
{
   gboolean ret;
   BackdoorChannel *bdoor = chan->_private;

   ASSERT(chan->appName != NULL);
   ASSERT(!bdoor->inStarted);
   ASSERT(!bdoor->outStarted);

   ret = RpcIn_start(bdoor->in, RPCIN_MAX_DELAY, RpcChannel_Error, chan);
   if (ret) {
      ret = RpcOut_start(bdoor->out);
      if (!ret) {
         RpcIn_stop(bdoor->in);
      }
   }
   bdoor->inStarted = TRUE;
   bdoor->outStarted = TRUE;
   return ret;
}


/**
 * Shuts down the RpcIn channel. Due to the "split brain" nature of the backdoor,
 * if this function fails, it's possible that while the "out" channel was shut
 * down the "in" one wasn't, for example, although that's unlikely.
 *
 * @param[in]  chan     The RPC channel instance.
 *
 * @return TRUE on success.
 */

static Bool
RpcInShutdown(RpcChannel *chan)
{
   return RpcInStopChannel(chan, TRUE);
}


/**
 * Stops the channel from receiving messages and releases the channel, but
 * don't clean up the internal channel state. This allows the channel to be
 * restarted later if needed. The "out" channel is also stopped, so sending
 * a message on the channel while it's stopped will fail.
 *
 * @param[in]  chan     The RPC channel instance.
 *
 * @return TRUE on success.
 */

static Bool
RpcInStop(RpcChannel *chan)
{
   return RpcInStopChannel(chan, FALSE);
}


/**
 * Sends the data using the RpcOut library.
 *
 * @param[in]  chan        The RPC channel instance.
 * @param[in]  data        Data to send.
 * @param[in]  dataLen     Number of bytes to send.
 * @param[out] result      Response from other side.
 * @param[out] resultLen   Number of bytes in response.
 *
 * @return The status from the remote end (TRUE if call was successful).
 */

static Bool
RpcInSend(RpcChannel *chan,
          char *data,
          size_t dataLen,
          char **result,
          size_t *resultLen)
{
   gboolean ret;
   const char *reply;
   size_t replyLen;
   BackdoorChannel *bdoor = chan->_private;

   ASSERT(chan->appName != NULL);

   if (!bdoor->outStarted) {
      return FALSE;
   }

   ret = RpcOut_send(bdoor->out, data, dataLen, &reply, &replyLen);

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

   return ret;
}


/**
 * Creates a new RpcIn channel.
 *
 * @param[in]  mainCtx     The app's main context.
 *
 * @return A new channel instance (never NULL).
 */

RpcChannel *
RpcChannel_NewBackdoorChannel(GMainContext *mainCtx)
{
   RpcChannel *ret;
   BackdoorChannel *bdoor;

   ret = g_malloc0(sizeof *ret);

   bdoor = g_malloc(sizeof *bdoor);
   bdoor->in = RpcIn_Construct(mainCtx, RpcChannel_Dispatch, ret);
   ASSERT(bdoor->in != NULL);

   bdoor->out = RpcOut_Construct();
   ASSERT(bdoor->out != NULL);

   bdoor->inStarted = FALSE;
   bdoor->outStarted = FALSE;

   ret->start = RpcInStart;
   ret->stop = RpcInStop;
   ret->send = RpcInSend;
   ret->shutdown = RpcInShutdown;
   ret->_private = bdoor;

   return ret;
}


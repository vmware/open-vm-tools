/*********************************************************
 * Copyright (C) 2013-2016,2018-2019 VMware, Inc. All rights reserved.
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
 * vsockChannel.c --
 *
 *    Implement RpcChannel using vsocket.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "simpleSocket.h"
#include "rpcChannelInt.h"
#include "util.h"
#include "debug.h"

#define LGPFX "VSockChan: "

typedef struct VSockOut {
   SOCKET fd;
   char *payload;
   int payloadLen;
   RpcChannelType type;
} VSockOut;

typedef struct VSockChannel {
   VSockOut          *out;
} VSockChannel;

static void VSockChannelShutdown(RpcChannel *chan);


/*
 *-----------------------------------------------------------------------------
 *
 * VSockCreateConn --
 *
 *      Create vsocket connection. we try a privileged connection first,
 *      fallback to unprivileged one if that fails.
 *
 * Result:
 *      a valid socket/fd on success or INVALID_SOCKET on failure.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
VSockCreateConn(gboolean *isPriv)        // OUT
{
   ApiError apiErr;
   int sysErr;
   SOCKET fd;

   Debug(LGPFX "Creating privileged vsocket ...\n");
   fd = Socket_ConnectVMCI(VMCI_HYPERVISOR_CONTEXT_ID,
                           GUESTRPC_RPCI_VSOCK_LISTEN_PORT,
                           TRUE, &apiErr, &sysErr);

   if (fd != INVALID_SOCKET) {
      Debug(LGPFX "Successfully created priv vsocket %d\n", fd);
      *isPriv = TRUE;
      return fd;
   }

   if (apiErr == SOCKERR_BIND && sysErr == SYSERR_EACCESS) {
      Debug(LGPFX "Creating unprivileged vsocket ...\n");
      fd = Socket_ConnectVMCI(VMCI_HYPERVISOR_CONTEXT_ID,
                              GUESTRPC_RPCI_VSOCK_LISTEN_PORT,
                              FALSE, &apiErr, &sysErr);
      if (fd != INVALID_SOCKET) {
         Debug(LGPFX "Successfully created unpriv vsocket %d\n", fd);
         *isPriv = FALSE;
         return fd;
      }
   }

   Debug(LGPFX "Failed to create vsocket channel, %d, %d\n", apiErr, sysErr);
   return INVALID_SOCKET;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockOutConstruct --
 *
 *      Constructor for the VSockOut object
 *
 * Results:
 *      New VSockOut object.
 *
 * Side effects:
 *      Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

static VSockOut *
VSockOutConstruct(void)
{
   VSockOut *out = calloc(1, sizeof *out);

   if (out != NULL) {
      out->fd = INVALID_SOCKET;
      out->type = RPCCHANNEL_TYPE_INACTIVE;
   }
   return out;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockOutDestruct --
 *
 *      Destructor for the VSockOut object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees VSockOut object memory.
 *
 *-----------------------------------------------------------------------------
 */

static void
VSockOutDestruct(VSockOut *out)        // IN
{

   ASSERT(out);
   ASSERT(out->fd == INVALID_SOCKET);

   free(out->payload);
   free(out);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockOutStart --
 *
 *      Open the channel
 *
 * Result:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VSockOutStart(VSockOut *out)      // IN
{
   gboolean isPriv;

   ASSERT(out);
   ASSERT(out->fd == INVALID_SOCKET);

   out->fd = VSockCreateConn(&isPriv);
   if (out->fd != INVALID_SOCKET) {
      out->type = isPriv ? RPCCHANNEL_TYPE_PRIV_VSOCK :
                           RPCCHANNEL_TYPE_UNPRIV_VSOCK;
   }
   return out->fd != INVALID_SOCKET;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockOutStop --
 *
 *    Close the channel
 *
 * Result
 *    None
 *
 * Side-effects
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VSockOutStop(VSockOut *out)    // IN
{
   ASSERT(out);

   if (out->fd != INVALID_SOCKET) {
      Socket_Close(out->fd);
      out->fd = INVALID_SOCKET;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockOutSend --
 *
 *    Make VMware synchronously execute a TCLO command
 *
 *    Unlike the other send varieties, VSockOutSend requires that the
 *    caller pass non-NULL reply and repLen arguments.
 *
 * Result
 *    TRUE if RPC was sent successfully. 'reply' contains the result of the rpc.
 *    rpcStatus tells if the RPC command was processed successfully.
 *
 *    FALSE if RPC could not be sent successfully. 'reply' will contain a
 *    description of the error.
 *
 *    In both cases, the caller should not free the reply.
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VSockOutSend(VSockOut *out,        // IN
             const char *request,  // IN
             size_t reqLen,        // IN
             Bool *rpcStatus,      // OUT
             const char **reply,   // OUT
             size_t *repLen)       // OUT
{
   ASSERT(out);
   ASSERT(out->fd != INVALID_SOCKET);

   *reply = NULL;
   *repLen = 0;

   Debug(LGPFX "Sending request for conn %d,  reqLen=%d\n",
         out->fd, (int)reqLen);

   if (!Socket_SendPacket(out->fd, request, reqLen)) {
      *reply = "VSockOut: Unable to send data for the RPCI command";
      goto error;
   }

   free(out->payload);
   out->payload = NULL;

   if (!Socket_RecvPacket(out->fd, &out->payload, &out->payloadLen)) {
      *reply = "VSockOut: Unable to receive the result of the RPCI command";
      goto error;
   }

   if (out->payloadLen < 2 ||
       ((out->payload[0] != '1') && (out->payload[0] != '0')) ||
       out->payload[1] != ' ') {
      *reply = "VSockOut: Invalid format for the result of the RPCI command";
      goto error;
   }

   *reply = out->payload + 2;
   *repLen = out->payloadLen - 2;

   Debug("VSockOut: recved %d bytes for conn %d\n", out->payloadLen, out->fd);

   *rpcStatus = out->payload[0] == '1';
   return TRUE;

error:
   *repLen = strlen(*reply);
   *rpcStatus = FALSE;
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelOnStartErr --
 *
 *      Callback function to cleanup after channel start failure.
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
VSockChannelOnStartErr(RpcChannel *chan)    // IN
{
   VSockChannel *vsock = chan->_private;

   VSockOutDestruct(vsock->out);
   g_free(vsock);
   chan->_private = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelStart --
 *
 *      Starts the RpcIn loop and the VSockOut channel.
 *
 * Results:
 *      TRUE on success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VSockChannelStart(RpcChannel *chan)    // IN
{
   gboolean ret = TRUE;
   VSockChannel *vsock = chan->_private;

#if defined(NEED_RPCIN)
   ret = chan->in == NULL || chan->inStarted;
#endif

   if (ret) {
      ret = VSockOutStart(vsock->out);
   }
   chan->outStarted = ret;

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelStop --
 *
 *      Stops a channel, keeping internal state so that it can be restarted
 *      later. It's safe to call this function more than once.
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
VSockChannelStop(RpcChannel *chan)   // IN
{
   VSockChannel *vsock = chan->_private;

   if (vsock->out != NULL) {
      if (chan->outStarted) {
         VSockOutStop(vsock->out);
         chan->outStarted = FALSE;
      }
   } else {
      ASSERT(!chan->outStarted);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelShutdown --
 *
 *      Shuts down the Rpc channel.
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
VSockChannelShutdown(RpcChannel *chan)    // IN
{
   VSockChannel *vsock = chan->_private;

   VSockChannelStop(chan);
   VSockOutDestruct(vsock->out);
   g_free(vsock);
   chan->_private = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelSend --
 *
 *      Sends the data using the vsocket channel.
 *      If the caller is not interested in the reply, result and resultLen
 *      can be set to NULL, otherwise, the caller *must* free the result
 *      whether the call is successful or not to avoid memory leak.
 *
 *      rpcStatus tells if VMware could process the RPC command successully.
 *      It is valid only when function returns success.
 *
 * Result:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VSockChannelSend(RpcChannel *chan,      // IN
                 char const *data,      // IN
                 size_t dataLen,        // IN
                 Bool *rpcStatus,       // OUT
                 char **result,         // OUT optional
                 size_t *resultLen)     // OUT optional
{
   gboolean ret = FALSE;
   VSockChannel *vsock = chan->_private;
   const char *reply = NULL;
   size_t replyLen = 0;

   if (!chan->outStarted) {
      goto exit;
   }

   /*
    * We propagate all replies from VSockOutSend: either a reply of the RPC
    * result or a description of the error on failure.
    */
   ret = VSockOutSend(vsock->out, data, dataLen, rpcStatus, &reply, &replyLen);

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


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannel_GetType --
 *
 *      Return the channel type that being used.
 *
 * Result:
 *      return the channel type.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static RpcChannelType
VSockChannelGetType(RpcChannel *chan)
{
   VSockChannel *vsock = chan->_private;

   if (vsock->out != NULL) {
      return vsock->out->type;
   } else {
      return RPCCHANNEL_TYPE_INACTIVE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannelStopRpcOut --
 *
 *      Stop the RpcOut channel
 *
 * Result:
 *      return TRUE on success.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VSockChannelStopRpcOut(RpcChannel *chan)
{
   VSockChannel *vsock = chan->_private;
   VSockOutStop(vsock->out);
   chan->outStarted = FALSE;

   return TRUE;
}




/*
 *-----------------------------------------------------------------------------
 *
 * VSockChannel_New --
 *
 *      Creates a new RpcChannel channel that uses the vsocket for
 *      communication.
 *
 * Result:
 *      return A new channel instance (never NULL).
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

RpcChannel *
VSockChannel_New(void)
{
   RpcChannel *chan;
   VSockChannel *vsock;

   static RpcChannelFuncs funcs = {
      VSockChannelStart,
      VSockChannelStop,
      VSockChannelSend,
      NULL,
      VSockChannelShutdown,
      VSockChannelGetType,
      VSockChannelOnStartErr,
      VSockChannelStopRpcOut
   };

   chan = RpcChannel_Create();
   vsock = g_malloc0(sizeof *vsock);

   vsock->out = VSockOutConstruct();
   ASSERT(vsock->out != NULL);

#if defined(NEED_RPCIN)
   chan->inStarted = FALSE;
#endif
   chan->outStarted = FALSE;

   chan->_private = vsock;
   chan->funcs = &funcs;
   g_mutex_init(&chan->outLock);

   return chan;
}

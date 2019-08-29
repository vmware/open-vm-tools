/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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

/*********************************************************
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
 * rpcout.c --
 *
 *    Remote Procedure Call between VMware and guest applications
 *    C implementation.
 *
 *    This module contains implements the out (guest=>host) direction only.
 *    The in and out modules are separate since some applications (e.g.
 *    drivers that want to do RPC-based logging) only want/need/can have the
 *    out direction.
 */


#if defined(__KERNEL__) || defined(_KERNEL) || defined(KERNEL)
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <string.h>
#   include <stdlib.h>
#   include <stdarg.h>
#   include "str.h"
#   include "debug.h"
#endif

#include "vmware.h"
#include "rpcout.h"
#include "message.h"


/*
 * The RpcOut object
 */

struct RpcOut {
   Message_Channel channel;
   Bool started;
};


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOutInitialize --
 *
 *      Initializes an already allocated RpcOut object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcOutInitialize(RpcOut *rpcOut)
{
   memset(rpcOut, 0, sizeof *rpcOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_Construct --
 *
 *      Constructor for the RpcOut object
 *
 * Results:
 *      New RpcOut object.
 *
 * Side effects:
 *      Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

RpcOut *
RpcOut_Construct(void)
{
   RpcOut *rpcOut = malloc(sizeof *rpcOut);
   RpcOutInitialize(rpcOut);
   return rpcOut;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_Destruct --
 *
 *      Destructor for the RpcOut object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees RpcOut object memory.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcOut_Destruct(RpcOut *out) // IN
{
   ASSERT(out != NULL);
   ASSERT(!out->started);

   free(out);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_startWithReceiveBuffer --
 *
 *      Open the channel.  This variant of RpcOut_start allows the
 *      caller to pre-allocate the receiver buffer, but by doing so it
 *      allows for some simple operations without the need to call
 *      malloc.
 *
 *      Passing in NULL and size 0 will cause this function to fall
 *      back to using malloc.
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

Bool
RpcOut_startWithReceiveBuffer(RpcOut *out, char *receiveBuffer,
                              size_t receiveBufferSize)
{
   ASSERT(out != NULL);
   ASSERT(!out->started);
   out->started = Message_OpenAllocated(RPCI_PROTOCOL_NUM, &out->channel,
                                        receiveBuffer, receiveBufferSize);
   if (!out->started) {
      Debug("RpcOut: couldn't open channel with RPCI protocol\n");
   }
   return out->started;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_start --
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

Bool
RpcOut_start(RpcOut *out) // IN
{
   return RpcOut_startWithReceiveBuffer(out, NULL, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_send --
 *
 *    Make VMware synchroneously execute a TCLO command
 *
 *    Unlike the other send varieties, RpcOut_send requires that the
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

Bool
RpcOut_send(RpcOut *out,         // IN
            char const *request, // IN
            size_t reqLen,       // IN
            Bool *rpcStatus,     // OUT
            char const **reply,  // OUT
            size_t *repLen)      // OUT
{
   unsigned char *myReply;
   size_t myRepLen;
   Bool success;

   ASSERT(out != NULL);
   ASSERT(out->started);

   if (Message_Send(&out->channel, (const unsigned char *)request, reqLen) == FALSE) {
      *reply = "RpcOut: Unable to send the RPCI command";
      *repLen = strlen(*reply);

      return FALSE;
   }

   if (Message_Receive(&out->channel, &myReply, &myRepLen) == FALSE) {
      *reply = "RpcOut: Unable to receive the result of the RPCI command";
      *repLen = strlen(*reply);

      return FALSE;
   }

   if (myRepLen < 2
       || (   (success = strncmp((const char *)myReply, "1 ", 2) == 0) == FALSE
              && strncmp((const char *)myReply, "0 ", 2))) {
      *reply = "RpcOut: Invalid format for the result of the RPCI command";
      *repLen = strlen(*reply);

      return FALSE;
   }

   *rpcStatus = success;
   *reply = ((const char *)myReply) + 2;
   *repLen = myRepLen - 2;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_stop --
 *
 *    Close the channel
 *
 * Result
 *    TRUE on success
 *    FALSE on failure
 *
 * Side-effects
 *    Frees the result of the last command.
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcOut_stop(RpcOut *out) // IN
{
   Bool status = TRUE;

   ASSERT(out != NULL);

   if (out->started) {
      /* Try to close the channel */
      if (Message_CloseAllocated(&out->channel) == FALSE) {
         Debug("RpcOut: couldn't close channel\n");
         status = FALSE;
      }

      out->started = FALSE;
   }

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_sendOne --
 *
 *    Make VMware execute a RPCI command
 *
 *    VMware closes a channel when it detects that there has been no activity
 *    on it for a while. Because we do not know how often this program will
 *    make VMware execute a RPCI, we open/close one channel per RPCI command
 *
 * Return value:
 *    TRUE on success. '*reply' contains an allocated result of the rpc
 *    FALSE on error. '*reply' contains an allocated description of the error
 *                    or NULL.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcOut_sendOne(char **reply,        // OUT: Result
               size_t *repLen,      // OUT: Length of the result
               char const *reqFmt,  // IN: RPCI command
               ...)                 // Unspecified
{
   va_list args;
   Bool status;
   char *request;
   size_t reqLen = 0;

   status = FALSE;

   /* Format the request string */
   va_start(args, reqFmt);
   request = Str_Vasprintf(&reqLen, reqFmt, args);
   va_end(args);

   /*
    * If Str_Vasprintf failed, write NULL into the reply if the caller wanted
    * a reply back.
    */
   if (request == NULL) {
      if (reply) {
         *reply = NULL;
      }
      return FALSE;
   }

   /*
    * If the command doesn't contain a space, add one to the end to maintain
    * compatibility with old VMXs.
    *
    * For a long time, the GuestRpc logic in the VMX was wired to expect a
    * trailing space in every command, even commands without arguments. That is
    * no longer true, but we must continue to add a trailing space because we
    * don't know whether we're talking to an old or new VMX.
    */
   if (strchr(request, ' ') == NULL) {
      char *tmp;

      tmp = Str_Asprintf(NULL, "%s ", request);
      free(request);
      request = tmp;

      /*
       * If Str_Asprintf failed, write NULL into reply if the caller wanted
       * a reply back.
       */
      if (request == NULL) {
         if (reply != NULL) {
            *reply = NULL;
         }
         return FALSE;
      }
   }

   status = RpcOut_SendOneRaw(request, reqLen, reply, repLen);

   free(request);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOutSendOneRawWork --
 *
 *    Helper function to make VMware execute a RPCI command.  See
 *    RpcOut_SendOneRaw and RpcOut_SendOneRawPreallocated.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcOutSendOneRawWork(void *request,         // IN: RPCI command
                     size_t reqLen,         // IN: Size of request buffer
                     char *callerReply,     // IN: caller supplied reply buffer
                     size_t callerReplyLen, // IN: size of caller supplied buf
                     char **reply,          // OUT: Result
                     size_t *repLen)        // OUT: Length of the result
{
   Bool status;
   Bool rpcStatus;
   /* Stack allocate so this can be used in kernel logging.  See 1389199. */
   RpcOut out;
   char const *myReply;
   size_t myRepLen;

   Debug("Rpci: Sending request='%s'\n", (char *)request);
   RpcOutInitialize(&out);
   if (!RpcOut_startWithReceiveBuffer(&out, callerReply, callerReplyLen)) {
      myReply = "RpcOut: Unable to open the communication channel";
      myRepLen = strlen(myReply);

      if (callerReply != NULL) {
         unsigned s = MIN(callerReplyLen - 1, myRepLen);
         ASSERT(reply == NULL);
         memcpy(callerReply, myReply, s);
         callerReply[s] = '\0';
      }
      if (reply != NULL) {
         *reply = NULL;
      }
      return FALSE;
   }

   status = RpcOut_send(&out, request, reqLen,
                        &rpcStatus, &myReply, &myRepLen);
   /* On failure, we already have the description of the error */

   Debug("Rpci: Sent request='%s', reply='%s', len=%"FMTSZ"u, "
         "status=%d, rpcStatus=%d\n",
         (char *)request, myReply, myRepLen, status, rpcStatus);

   if (reply != NULL) {
      /*
       * If we got a non-NULL reply, make a copy of it, because the reply
       * we got back is inside the channel buffer, which will get destroyed
       * at the end of this function.
       */
      if (myReply != NULL) {
         /*
          * We previously used strdup to duplicate myReply, but that
          * breaks if you are sending binary (not string) data over the
          * backdoor. Don't assume the data is a string.
          *
          * myRepLen is strlen(myReply), so we need an extra byte to
          * cover the NUL terminator.
          */
         *reply = malloc(myRepLen + 1);
         if (*reply != NULL) {
            memcpy(*reply, myReply, myRepLen);
            /*
             * The message layer already writes a trailing NUL but we might
             * change that someday, so do it again here.
             */
            (*reply)[myRepLen] = 0;
         }
      } else {
         /*
          * Our reply was NULL, so just pass the NULL back up to the caller.
          */
         *reply = NULL;
      }

      /*
       * Only set the length if the caller wanted it and if we got a good
       * reply.
       */
      if (repLen != NULL && *reply != NULL) {
         *repLen = myRepLen;
      }
   }

   if (RpcOut_stop(&out) == FALSE) {
      /*
       * We couldn't stop the channel. Free anything we allocated, give our
       * client a reply of NULL, and return FALSE.
       */

      if (reply != NULL) {
         free(*reply);
         *reply = NULL;
      }
      Debug("Rpci: unable to close the communication channel\n");
      status = FALSE;
   }

   return status && rpcStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_SendOneRaw --
 *
 *    Make VMware execute a RPCI command
 *
 *    VMware closes a channel when it detects that there has been no activity
 *    on it for a while. Because we do not know how often this program will
 *    make VMware execute a RPCI, we open/close one channel per RPCI command.
 *
 *    This function sends a message over the backdoor without using
 *    any of the Str_ functions on the request buffer; Str_Asprintf() in
 *    particular uses FormatMessage on Win32, which corrupts some UTF-8
 *    strings. Using this function directly instead of using RpcOut_SendOne()
 *    avoids these problems.
 *
 *    If this is not an issue, you can use RpcOut_sendOne(), which has
 *    varargs.
 *
 *    Note: It is the caller's responsibility to ensure that the RPCI command
 *          followed by a space appear at the start of the request buffer. See
 *          the command in RpcOut_sendOne for details.
 *
 * Return value:
 *    TRUE on success. '*reply' contains an allocated result of the rpc
 *    FALSE on error. '*reply' contains an allocated description of the
 *                    error or NULL.
 *
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcOut_SendOneRaw(void *request,       // IN: RPCI command
                  size_t reqLen,       // IN: Size of request buffer
                  char **reply,        // OUT: Result
                  size_t *repLen)      // OUT: Length of the result
{
   return RpcOutSendOneRawWork(request, reqLen, NULL, 0, reply, repLen);
}



/*
 *-----------------------------------------------------------------------------
 *
 * RpcOut_SendOneRawPreallocated --
 *
 *    Make VMware execute a RPCI command.
 *
 *    A variant of RpcOut_SendOneRaw in which the caller supplies the
 *    receive buffer instead of dynamically allocating it.  This
 *    allows the caller to call this function in situations where
 *    malloc is not allowed.  But if the response from the host is too
 *    large the rpc will fail instead of attempting to grow its own
 *    receive buffer.
 *
 *
 * Return value:
 *    TRUE on success. 'reply' contains an allocated result of the rpc
 *    FALSE on error. 'reply' contains an allocated description of the
 *                     error or NULL.
 *
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcOut_SendOneRawPreallocated(void *request,       // IN: RPCI command
                              size_t reqLen,       // IN: Size of request buffer
                              char *reply,         // OUT: Result
                              size_t repLen)       // IN: Length of the result
{
   return RpcOutSendOneRawWork(request, reqLen, reply, repLen, NULL, NULL);
}

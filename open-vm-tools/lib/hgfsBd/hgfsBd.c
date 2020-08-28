/*********************************************************
 * Copyright (C) 1998-2016,2019 VMware, Inc. All rights reserved.
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
 * hgfsBd.c --
 *
 *    Backdoor calls used by hgfs pserver. [bac]
 */

#if defined(__KERNEL__) || defined(_KERNEL) || defined(KERNEL)
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#   include <errno.h>
#   include "str.h"      // for Str_Strcpy
#   include "debug.h"
#endif

#include "vm_assert.h"
#include "rpcout.h"
#include "hgfs.h"     // for common HGFS definitions
#include "hgfsBd.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBdGetBufInt --
 *
 *    Allocates a buffer to send a hgfs request in. This can be either a
 *    HGFS_PACKET_MAX or HGFS_LARGE_PACKET_MAX size buffer depending on the
 *    external funciton called.
 *
 * Results:
 *    Pointer to a buffer that has the correct backdoor command prefix for 
 *    sending hgfs requests over the backdoor.
 *    NULL on failure (not enough memory).
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
HgfsBdGetBufInt(size_t bufSize)
{
   /* 
    * Allocate a buffer that is large enough for an HGFS packet and the 
    * synchronous HGFS command, write the command, and return a pointer that 
    * points into the buffer, after the command.
    */
   size_t len = bufSize + HGFS_SYNC_REQREP_CLIENT_CMD_LEN;
   char *buf = (char*) calloc(sizeof(char), len);

   if (!buf) {
      Debug("HgfsBd_GetBuf: Failed to allocate a bd buffer\n");
      return NULL;
   }

   Str_Strcpy(buf, HGFS_SYNC_REQREP_CLIENT_CMD, len);

   return buf + HGFS_SYNC_REQREP_CLIENT_CMD_LEN;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_GetBuf --
 *
 *    Get a buffer of size HGFS_PACKET_MAX to send hgfs requests in.
 *
 * Results:
 *    See HgfsBdGetBufInt.
 *
 * Side effects:
 *    Allocates memory that must be freed with a call to HgfsBd_PutBuf.
 *
 *-----------------------------------------------------------------------------
 */

char *
HgfsBd_GetBuf(void)
{
   return HgfsBdGetBufInt(HGFS_PACKET_MAX);
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_GetLargeBuf --
 *
 *    Get a buffer of size HGFS_LARGE_PACKET_MAX to send hgfs requests in.
 *
 * Results:
 *    See HgfsBdGetBufInt.
 *
 * Side effects:
 *    Allocates memory that must be freed with a call to HgfsBd_PutBuf.
 *
 *-----------------------------------------------------------------------------
 */

char *
HgfsBd_GetLargeBuf(void)
{
   return HgfsBdGetBufInt(HgfsLargePacketMax(FALSE));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_PutBuf --
 *
 *    Release a buffer obtained with HgfsBd_GetBuf.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsBd_PutBuf(char *buf) // IN
{
   ASSERT(buf);

   free(buf - HGFS_SYNC_REQREP_CLIENT_CMD_LEN);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_GetChannel --
 *
 *    Allocate a new RpcOut channel, and try to open the connection.
 *
 * Results:
 *    Pointer to the allocated, opened channel on success.
 *    NULL on failure (not enough memory, or failed to open the connection).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

RpcOut *
HgfsBd_GetChannel(void)
{
   RpcOut *out = RpcOut_Construct();
   Bool status;

   if (!out) {
      Debug("HgfsBd_GetChannel: Failed to allocate an RpcOut\n");
      return NULL;
   }

   status = RpcOut_start(out);
   if (status == FALSE) {
      RpcOut_Destruct(out);
      return NULL;
   }

   return out;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_CloseChannel --
 *
 *    Close the channel and free the RpcOut object.
 *
 * Results:
 *    TRUE if closing the channel succeeded, FALSE if it failed.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsBd_CloseChannel(RpcOut *out) // IN: Channel to close and free
{
   Bool success; 

   ASSERT(out);

   success = RpcOut_stop(out);
   if (success == TRUE) {
      RpcOut_Destruct(out);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_Dispatch --
 *
 *    Get a reply to an hgfs request. We call RpcOut_Sent, which
 *    returns a buffer with the reply in it, and we pass this back to
 *    the caller.
 *
 * Results:
 *    On success, returns zero. On failure, returns a negative error.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsBd_Dispatch(RpcOut *out,            // IN: Channel to send on
                char *packetIn,         // IN: Buf containing request packet
                size_t *packetSize,     // IN/OUT: Size of packet in/out
                char const **packetOut) // OUT: Buf containing reply packet
{
   Bool success;
   Bool rpcStatus;
   char const *reply;
   size_t replyLen;
   char *bdPacket = packetIn - HGFS_SYNC_REQREP_CLIENT_CMD_LEN;

   ASSERT(out);
   ASSERT(packetIn);
   ASSERT(packetSize);
   ASSERT(packetOut);

   memcpy(bdPacket, HGFS_SYNC_REQREP_CLIENT_CMD, HGFS_SYNC_REQREP_CLIENT_CMD_LEN);

   success = RpcOut_send(out, bdPacket, *packetSize + HGFS_CLIENT_CMD_LEN,
                         &rpcStatus, &reply, &replyLen);
   if (!success || !rpcStatus) {
      Debug("HgfsBd_Dispatch: RpcOut_send returned failure\n");
      return -1;
   }

   ASSERT(replyLen <= HgfsLargePacketMax(TRUE));
   *packetOut = reply;
   *packetSize = replyLen;

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_Enabled --
 *
 *    Test to see if hgfs is enabled on the host.
 *
 * Results:
 *    TRUE if hgfs is enabled.
 *    FALSE if hgfs is disabled.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsBd_Enabled(RpcOut *out,         // IN: RPCI Channel
               char *requestPacket) // IN: Buffer (obtained from HgfsBd_GetBuf)
{
   char const *replyPacket; // Buffer returned by HgfsBd_Dispatch
   size_t replyLen;
   Bool success;
   Bool rpcStatus;

   /*
    * Send a bogus (empty) request to the VMX. If hgfs is disabled on
    * the host side then the request will fail (because the RPCI call
    * itself will fail). If hgfs is enabled, we will get a packet back
    * (it will be an error packet because our request was malformed,
    * but we just discard it anyway).
    */
   success = RpcOut_send(out, requestPacket - HGFS_CLIENT_CMD_LEN,
                         HGFS_CLIENT_CMD_LEN,
                         &rpcStatus, &replyPacket, &replyLen);
   if (success && rpcStatus) {
      ASSERT(replyLen <= HgfsLargePacketMax(TRUE));
   }

   return success && rpcStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_OpenBackdoor --
 *
 *      Check if the HGFS channel is open, and, if not, open it. This is a
 *      one-stop convenience wrapper around HgfsBd_Enabled, HgfsBd_GetBuf, and
 *      HgfsBd_GetChannel.
 *
 * Results:
 *      TRUE if the backdoor is now open, regardless of its previous state.
 *      FALSE if the backdoor could not be opened.
 *
 * Side effects:
 *      May open a channel to the host.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsBd_OpenBackdoor(RpcOut **out) // IN/OUT: RPCI Channel
{
   char *packetBuffer = NULL;
   Bool success = FALSE;

   ASSERT(out);

   /* Short-circuit: backdoor is already open. */
   if (*out != NULL) {
      return TRUE;
   }

   /* Open the channel. */   
   *out = HgfsBd_GetChannel();
   if (*out == NULL) {
      return FALSE;
   }

   /* Allocate a buffer for use in pinging the HGFS server. */
   packetBuffer = HgfsBd_GetBuf();
   if (packetBuffer == NULL) {
      goto out;
   }

   /* Ping the HGFS server. */
   if (!HgfsBd_Enabled(*out, packetBuffer)) {
      goto out;
   }
   success = TRUE;

  out:
   if (packetBuffer != NULL) {
      HgfsBd_PutBuf(packetBuffer);
   }
   if (!success && *out != NULL) {
      HgfsBd_CloseChannel(*out);
      *out = NULL;
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBd_CloseBackdoor --
 *
 *      Closes the backdoor channel, if it's open.
 *
 * Results:
 *      TRUE if the channel is now closed, regardless of its previous state.
 *      FALSE if we could not close the channel.
 *
 * Side effects:
 *      May close the channel to the host.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsBd_CloseBackdoor(RpcOut **out) // IN/OUT: RPCI Channel
{
   Bool success = TRUE;

   ASSERT(out);

   if (*out != NULL) {
      if (!HgfsBd_CloseChannel(*out)) {
         success = FALSE;
      }
      *out = NULL;
   }

   return success;
}

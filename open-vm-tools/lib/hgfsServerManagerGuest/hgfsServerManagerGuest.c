/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * hgfsServerManagerGuest.c --
 *
 *    Functionality to utilize the hgfs server in bora/lib from within
 *    a guest application.
 *
 */

#include "rpcout.h"
#include "rpcin.h"
#include "hgfsServerPolicy.h"
#include "hgfsServer.h"
#include "hgfsChannel.h"
#include "hgfsServerManager.h"
#include "vm_assert.h"
#include "hgfs.h"
#include "vmware/guestrpc/tclodefs.h"


static Bool HgfsServerManagerRpcInDispatch(char const **result,
                                           size_t *resultLen,
                                           const char *name,
                                           const char *args,
                                           size_t argsSize,
                                           void *clientData);


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannel_Init --
 *
 *      Sets up the channel for HGFS.
 *
 *      NOTE: Initialize the Hgfs server for only for now.
 *      This will move into a separate file when full interface implemented.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsChannel_Init(void *data)     // IN: Unused rpc data
{
   HgfsServerSessionCallbacks *serverCBTable = NULL;
   return HgfsServer_InitState(&serverCBTable, NULL);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannel_Exit --
 *
 *      Close the channel for HGFS.
 *
 *      NOTE: Close open sessions in the HGFS server currently.
 *      This will move into a separate file when full interface implemented.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Closes the worker group and all the channels.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsChannel_Exit(void *data)  // IN: Unused rpc data
{
   ASSERT(data != NULL);
   HgfsServer_ExitState();
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManagerRpcInDispatch --
 *
 *    Handles hgfs requests.
 *
 * Results:
 *    TRUE on success, FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsServerManagerRpcInDispatch(char const **result,        // OUT
                               size_t *resultLen,          // OUT
                               const char *name,           // IN
                               const char *args,           // IN
                               size_t argsSize,            // IN
                               void *clientData)           // Unused
{
   size_t packetSize;
   static char packet[HGFS_LARGE_PACKET_MAX];


   ASSERT(clientData == NULL);

   if (argsSize == 0) {
      return RpcIn_SetRetVals(result, resultLen, "1 argument required", FALSE);
   }

   ASSERT(args[0] == ' ');
   packetSize = argsSize - 1;
   HgfsServer_ProcessPacket((char const *)(args + 1), packet, &packetSize);

   *result = packet;
   *resultLen = packetSize;
   return TRUE;
}



/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerManager_CapReg --
 *
 *    Tell the vmx that the specified guest app can (or no longer can) 
 *    receive hgfs requests.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerManager_CapReg(const char *appName, // IN
                         Bool enable)         // IN
{
   /*
    * Register/unregister this channel as an hgfs server.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.hgfs_server %s %s",
                       appName, enable ? "1" : "0")) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManager_Register --
 *
 *    Registers the hgfs server to be used in classic synchronous fashion.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    Hgfs packets sent to this channel will be handled.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsServerManager_Register(void *rpcIn,         // IN: RpcIn channel
                           const char *appName) // IN: App with HGFS server
{
   RpcIn *myRpcIn = (RpcIn *)rpcIn;

   /*
    * myRpcIn may be NULL for some cases. When we run the tools as
    * a guest application in a non-VMware VM, for example, we do not
    * have a backdoor.
    */
   ASSERT(appName);

   /*
    * Passing NULL here is safe because the shares maintained by the guest
    * policy server never change, invalidating the need for an invalidate
    * function.
    */
   if (!HgfsServerPolicy_Init(NULL)) {
      return FALSE;
   }

   if (!HgfsChannel_Init(myRpcIn)) {
      HgfsServerPolicy_Cleanup();
      return FALSE;
   }

   if (NULL != myRpcIn) {
      RpcIn_RegisterCallback(myRpcIn, HGFS_SYNC_REQREP_CMD,
                             HgfsServerManagerRpcInDispatch, NULL);
   }

   /*
    * Prior to WS55, the VMX did not know about the "hgfs_server"
    * capability. This doesn't mean that the HGFS server wasn't needed, it's
    * just that the capability was introduced in CS 225439 so that the VMX
    * could decide which HGFS server to communicate with.
    *
    * Long story short, we shouldn't care if this function fails.
    */
   if (NULL != myRpcIn) {
      HgfsServerManager_CapReg(appName, TRUE);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManager_Unregister --
 *
 *    Cleans up the hgfs server.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsServerManager_Unregister(void *rpcIn,         // IN: RpcIn channel
                             const char *appName) // IN: App with HGFS server

{
   RpcIn *myRpcIn = (RpcIn *)rpcIn;

   ASSERT(myRpcIn);
   ASSERT(appName);

   HgfsServerManager_CapReg(appName, FALSE);
   RpcIn_UnregisterCallback(myRpcIn, HGFS_SYNC_REQREP_CMD);
   HgfsChannel_Exit(myRpcIn);
   HgfsServerPolicy_Cleanup();
}

/*********************************************************
 * Copyright (C) 2006,2014-2019 VMware, Inc. All rights reserved.
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

#include <string.h>
#include "hgfsServerPolicy.h"
#include "hgfsChannelGuestInt.h"
#include "hgfsServerManager.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "vm_atomic.h"
#include "hgfs.h"

/*
 * Local for now and will be used in conjunction with the manager data passed
 * on registration.
 */
typedef struct HgfsServerMgrCountedCallbacks {
   HgfsServerMgrCallbacks  serverMgrCBTable; /* Hgfs server policy manager entry points. */
   Atomic_uint32           refCount;         /* Server data reference count. */
} HgfsServerMgrCountedCallbacks;

static HgfsServerMgrCountedCallbacks     gHgfsServerManagerGuestData;


 /*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManagerGet --
 *
 *      Increment server manager reference count.
 *
 * Results:
 *      The value of the reference count before the increment.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

uint32
HgfsServerManagerGet(HgfsServerMgrCountedCallbacks *serverMgrData)   // IN/OUT: ref count
{
   ASSERT(NULL != serverMgrData);
   return Atomic_ReadInc32(&serverMgrData->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManagerPut --
 *
 *      Decrement server manager reference count.
 *
 *      Teardown server manager object if removed the final reference.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsServerManagerPut(HgfsServerMgrCountedCallbacks *serverMgrData)   // IN/OUT: ref count
{
   ASSERT(NULL != serverMgrData);
   if (Atomic_ReadDec32(&serverMgrData->refCount) == 1) {
      HgfsServerPolicy_Cleanup();
      memset(serverMgrData, 0, sizeof *serverMgrData);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManager_ProcessPacket --
 *
 *    Handles hgfs requests from a client not by our
 *    registered RPC callback.
 *
 * Results:
 *    TRUE on success, FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool HgfsServerManager_ProcessPacket(HgfsServerMgrData *mgrData,  // IN: hgfs mgr
                                     char const *packetIn,        // IN: rqst
                                     size_t packetInSize,         // IN: rqst size
                                     char *packetOut,             // OUT: rep
                                     size_t *packetOutSize)       // IN/OUT: rep buf/data size
{
   Debug("%s: Processing Packet for %s.\n", __FUNCTION__, mgrData->appName);
   /* Pass to the channel to handle processing and the server. */
   return HgfsChannelGuest_Receive(mgrData,
                                   packetIn,
                                   packetInSize,
                                   packetOut,
                                   packetOutSize);
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
HgfsServerManager_Register(HgfsServerMgrData *data)   // IN: RpcIn channel
{
   HgfsServerMgrCallbacks *serverMgrCallbacks = &gHgfsServerManagerGuestData.serverMgrCBTable;
   uint32 serverMgrRefCount;

   ASSERT(data);
   ASSERT(data->appName);

   Debug("%s: Register %s.\n", __FUNCTION__, data->appName);

   /*
    * Reference the global server manager data. Initialize only for the first
    * caller to register.
    */
   serverMgrRefCount = HgfsServerManagerGet(&gHgfsServerManagerGuestData);
   if (0 == serverMgrRefCount) {
      Debug("%s: calling policy init %s.\n", __FUNCTION__, data->appName);
      /*
       * Passing NULL here is safe because the shares maintained by the guest
       * policy server never change, eliminating the need for an invalidate
       * function.
       */
      if (!HgfsServerPolicy_Init(NULL,
                                 &serverMgrCallbacks->enumResources)) {
         HgfsServerManagerPut(&gHgfsServerManagerGuestData);
         return FALSE;
      }
   }

   /*
    * The channel will reference count itself, initializing once, but store the
    * channel in the manager data object passed to us and return it to the caller.
    */
   if (!HgfsChannelGuest_Init(data, serverMgrCallbacks)) {
      HgfsServerManagerPut(&gHgfsServerManagerGuestData);
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerManager_InvalidateInactiveSessions --
 *
 *    Sends a request to invalidate all the inactive HGFS server sessions.
 *
 * Results:
 *    Number of active sessions remaining inside the HGFS server.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

uint32
HgfsServerManager_InvalidateInactiveSessions(HgfsServerMgrData *mgrData)  // IN: RpcIn channel
{
   ASSERT(mgrData);

   Debug("%s: Invalidate Inactive Sessions for %s.\n", __FUNCTION__,
         mgrData->appName);
   return HgfsChannelGuest_InvalidateInactiveSessions(mgrData);
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
HgfsServerManager_Unregister(HgfsServerMgrData *data)         // IN: RpcIn channel

{
   ASSERT(data);
   ASSERT(data->appName != NULL);

   Debug("%s: Unregister %s.\n", __FUNCTION__, data->appName);

   HgfsChannelGuest_Exit(data);
   HgfsServerManagerPut(&gHgfsServerManagerGuestData);
}

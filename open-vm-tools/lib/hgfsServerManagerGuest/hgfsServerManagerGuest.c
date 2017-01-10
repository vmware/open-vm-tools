/*********************************************************
 * Copyright (C) 2006,2014-2016 VMware, Inc. All rights reserved.
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
#include "hgfs.h"

/*
 * Local for now and will be used in conjuncutntion with the manager data passed
 * on registration.
 */
static HgfsServerMgrCallbacks gHgfsServerManagerGuestData = {
   {
      NULL,    // Filled by the policy manager
   }
};

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

   ASSERT(data);
   ASSERT(data->appName);

   /*
    * Passing NULL here is safe because the shares maintained by the guest
    * policy server never change, invalidating the need for an invalidate
    * function.
    * XXX - retrieve the enum of shares routines and will need to pass this
    * down through the channel guest into the HGFS server directly.
    */
   if (!HgfsServerPolicy_Init(NULL,
                              NULL,
                              &gHgfsServerManagerGuestData.enumResources)) {
      return FALSE;
   }

   if (!HgfsChannelGuest_Init(data, &gHgfsServerManagerGuestData)) {
      HgfsServerPolicy_Cleanup();
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

   HgfsChannelGuest_Exit(data);
   HgfsServerPolicy_Cleanup();
   memset(&gHgfsServerManagerGuestData, 0, sizeof gHgfsServerManagerGuestData);
}

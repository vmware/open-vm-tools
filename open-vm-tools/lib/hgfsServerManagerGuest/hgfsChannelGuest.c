/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * hgfsChannel.c --
 *
 *    Channel abstraction for the HGFS server.
 *
 */

#include <stdlib.h>
#include "vm_assert.h"
#include "vm_atomic.h"
#include "util.h"
#if defined(VMTOOLS_USE_GLIB)
#define G_LOG_DOMAIN "hgfsd"
#define Debug                 g_debug
#define Warning               g_warning
#include "vmware/tools/guestrpc.h"
#include "vmware/tools/utils.h"
// #include <glib.h>
#else
#include "debug.h"
#endif
#include "hgfsChannelGuestInt.h"
#include "hgfsServer.h"
#include "hgfsServerManager.h"


/* Transport channels context. Static. */
typedef struct HgfsChannelData {
   const char              *name;       /* Channel name. */
   HgfsGuestChannelCBTable *ops;        /* Channel operations. */
   uint32                  state;       /* Channel state (see flags below). */
   struct HgfsGuestConn    *connection; /* Opaque server connection */
} HgfsChannelData;

#define HGFS_CHANNEL_STATE_INIT     (1 << 0)
#define HGFS_CHANNEL_STATE_CBINIT   (1 << 1)

typedef uint32   HgfsChannelMgrState; /* Channel state (see flags below). */

#define HGFS_CHANNELMGR_STATE_SERVERINIT   (1 << 0)
#define HGFS_CHANNELMGR_STATE_CHANINIT     (1 << 1)

/* Static channel registration - assumes only one for now. */
static HgfsChannelData gHgfsChannels[] = {
   { "guest", &gGuestBackdoorOps, 0, NULL },
};

static HgfsChannelMgrState gHgfsChannelsMgrState = 0;

/*
 *----------------------------------------------------------------------------
 *
 * CHANNEL DATA FUNCTIONS
 *
 *----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelInitChannel --
 *
 *      Initializes a channel.
 *
 * Results:
 *      TRUE if the channel initialized, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelInitChannel(HgfsChannelData *channel)  // IN/OUT: channel object
{
   channel->state = HGFS_CHANNEL_STATE_INIT;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelExitChannel --
 *
 *      Teardown the channel.
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
HgfsChannelExitChannel(HgfsChannelData *channel) // IN/OUT: channel object
{
   if (channel->state & HGFS_CHANNEL_STATE_INIT) {
      channel->state = 0;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelActivateChannel --
 *
 *      Activate a channel by calling the channels init callback.
 *
 * Results:
 *      TRUE if a channel is active.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelActivateChannel(HgfsChannelData *channel,                  // IN/OUT: channel object
                           HgfsServerSessionCallbacks *serverCBTable, // IN: server callbacks
                           HgfsServerMgrData *mgrData)                // IN: mgrData
{
   Bool success = FALSE;
   struct HgfsGuestConn *connData = NULL;

   if (channel->ops->init(serverCBTable, mgrData->rpc, mgrData->rpcCallback, &connData)) {
      channel->state |= HGFS_CHANNEL_STATE_CBINIT;
      channel->connection = connData;
      success = TRUE;
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelDeactivateChannel --
 *
 *      Deactivate a channel by calling the channels exit callback.
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
HgfsChannelDeactivateChannel(HgfsChannelData *channel,   // IN/OUT: channel object
                             HgfsServerMgrData *mgrData) // IN: mgr handle
{
   channel->ops->exit(channel->connection);
   channel->state &= ~HGFS_CHANNEL_STATE_CBINIT;
   channel->connection = NULL;
   mgrData->connection = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelIsChannelActive --
 *
 *      Is the channel active (initialized) for processing requests.
 *
 * Results:
 *      TRUE if a channel is active.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelIsChannelActive(HgfsChannelData *channel) // IN/OUT: channel object
{
   return (Bool)((channel->state & HGFS_CHANNEL_STATE_INIT) &&
                 (channel->state & HGFS_CHANNEL_STATE_CBINIT));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelReceive --
 *
 *      Received a request on a channel pass on to the channel callback.
 *
 * Results:
 *      TRUE if a channel ws deactivated.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelReceive(HgfsChannelData *channel,   // IN/OUT: channel object
                   char const *packetIn,       // IN: incoming packet
                   size_t packetInSize,        // IN: incoming packet size
                   char *packetOut,            // OUT: outgoing packet
                   size_t *packetOutSize)      // IN/OUT: outgoing packet size
{
   return channel->ops->receive(channel->connection,
                                packetIn,
                                packetInSize,
                                packetOut,
                                packetOutSize);
}


/*
 *----------------------------------------------------------------------------
 *
 * CHANNEL PUBLIC FUNCTIONS
 *
 *----------------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuest_Init --
 *
 *      Sets up the channel for HGFS.
 *
 *      Initialize all the defined channels.
 *      At least one channel should succeed it's initialization
 *      completely, else we fail.
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
HgfsChannelGuest_Init(HgfsServerMgrData *mgrData) // IN/OUT: connection manager object
{
   HgfsServerSessionCallbacks *serverCBTable = NULL; // References a read-only global
   Bool success = FALSE;

   ASSERT(NULL != mgrData);
   ASSERT(NULL == mgrData->connection);

   gHgfsChannelsMgrState = 0;

   /* If we have a new connection initialize the server session. */
   if (!HgfsServer_InitState(&serverCBTable, NULL)) {
      Debug("%s: Could not init Hgfs server.\n", __FUNCTION__);
      goto exit;
   }
   gHgfsChannelsMgrState |= HGFS_CHANNELMGR_STATE_SERVERINIT;

   /* Initialize channels objects. */
   if (!HgfsChannelInitChannel(&gHgfsChannels[0])) {
      Debug("%s: Could not init channel.\n", __FUNCTION__);
      goto exit;
   }
   gHgfsChannelsMgrState |= HGFS_CHANNELMGR_STATE_CHANINIT;

   /* Call the channels initializers. */
   if (!HgfsChannelActivateChannel(&gHgfsChannels[0], serverCBTable, mgrData)) {
      Debug("%s: Could not activate channel.\n", __FUNCTION__);
      goto exit;
   }

   mgrData->connection = &gHgfsChannels[0];
   success = TRUE;

exit:
   if (!success) {
      HgfsChannelGuest_Exit(mgrData);
   }
   return success;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuest_Exit --
 *
 *      Close the channel for HGFS.
 *
 *      Close open sessions and close the channels.
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
HgfsChannelGuest_Exit(HgfsServerMgrData *mgrData) // IN/OUT: connection manager object
{
   HgfsChannelData *channel;

   ASSERT(mgrData != NULL);

   channel = mgrData->connection;

   Debug("%s: Channel Exit.\n", __FUNCTION__);

   if (NULL != channel) {
      if (HgfsChannelIsChannelActive(channel)) {
         HgfsChannelDeactivateChannel(channel, mgrData);
         mgrData->connection = NULL;
      }
   } else {
      channel = &gHgfsChannels[0];
   }

   if (gHgfsChannelsMgrState & HGFS_CHANNELMGR_STATE_CHANINIT) {
      HgfsChannelExitChannel(channel);
      gHgfsChannelsMgrState &= ~HGFS_CHANNELMGR_STATE_CHANINIT;
   }

   if (gHgfsChannelsMgrState & HGFS_CHANNELMGR_STATE_SERVERINIT) {
      HgfsServer_ExitState();
      gHgfsChannelsMgrState &= ~HGFS_CHANNELMGR_STATE_SERVERINIT;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuest_Receive --
 *
 *    Process packet not associated with an HGFS only registered callback.
 *
 *
 * Results:
 *    TRUE if successfully processed FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsChannelGuest_Receive(HgfsServerMgrData *mgrData, // IN/OUT : conn manager
                         char const *packetIn,       // IN: incoming packet
                         size_t packetInSize,        // IN: incoming packet size
                         char *packetOut,            // OUT: outgoing packet
                         size_t *packetOutSize)      // IN/OUT: outgoing packet size
{
   HgfsChannelData *channel = NULL;
   Bool result = FALSE;

   ASSERT(mgrData != NULL);
   ASSERT(mgrData->connection != NULL);

   channel = mgrData->connection;

   Debug("%s: Channel receive request.\n", __FUNCTION__);

   if (HgfsChannelIsChannelActive(channel)) {
      result = HgfsChannelReceive(channel,
                                  packetIn,
                                  packetInSize,
                                  packetOut,
                                  packetOutSize);
   }

   return result;
}

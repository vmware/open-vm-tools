/*********************************************************
 * Copyright (C) 2010-2017,2019 VMware, Inc. All rights reserved.
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
 * hgfsChannelGuest.c --
 *
 *    Channel abstraction for the HGFS server.
 *
 */

#include <stdlib.h>
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "vm_atomic.h"
#include "util.h"
#include "hgfsChannelGuestInt.h"
#include "hgfsServer.h"
#include "hgfsServerManager.h"

/*
 * HGFS server connection channel and state object usage.
 *
 * Currently, all plugins can share this same HGFS server channel and state.
 * This allows us to use a common channel so it is only initialized
 * once, by the first loaded plugin which requires an HGFS channel, and torn
 * down when the final plugin that uses the HGFS server is unloaded.
 *
 * Currently, the plugins are loaded (and unloaded) in any particular order,
 * and those operations are serialized. (For example the HGFS server plugin
 * maybe the first plugin loaded that uses this channel, but is not the final
 * plugin to be unloaded that uses the channel. This also may change in the
 * future, so no dependencies can be made on order of loading and unloading
 * of plugins.)
 * Furthermore, multiple plugins use the HGFS channel and server and some plugins
 * have multiple connections. Some plugins also create and teardown connections
 * during general mutlithreaded operation of the tools processes.
 *
 * In order to support the above, we must track how many users of the shared
 * connection there are. This allows us to tear down the shared connection
 * when the final plugin that is using it is unloaded, and when no
 * channels are in use the HGFS server state can be torn down.
 */

/*
 * The HGFS server state.
 *
 * This object is initiliazed once only and is shared across all
 * connections, shared or private.
 * Each new channel connection will reference the server and so the HGFS
 * server is initialized when the first new channel is being created. Each
 * new channel just increments the reference of server state object.
 * When the final channel is torn down the final HGFS server reference is
 * also removed and the HGFS server exit is called and this object is torn down.
 */
typedef struct HgfsChannelServerData {
   const HgfsServerCallbacks  *serverCBTable; /* HGFS server entry points. */
   Atomic_uint32              refCount;       /* Server data reference count. */
} HgfsChannelServerData;

/*
 * Transport channels context.
 *
 * Multiple callers share this same channel currently as only one
 * transport channel is required. Therefore, the channel is referenced
 * for each client that it is returned to (a usage count).
 */
typedef struct HgfsChannelData {
   const char                    *name;          /* Channel name. */
   const HgfsGuestChannelCBTable *ops;           /* Channel operations. */
   uint32                        state;          /* Channel state (see flags below). */
   struct HgfsGuestConn          *connection;    /* Opaque server connection */
   HgfsChannelServerData         *serverInfo;    /* HGFS server entry points. */
   Atomic_uint32                 refCount;       /* Channel reference count. */
} HgfsChannelData;

#define HGFS_CHANNEL_STATE_INIT         (1 << 0)
#define HGFS_CHANNEL_STATE_CBINIT       (1 << 1)

/* Static channel registration - assumes only one for now. */
static HgfsChannelData gHgfsChannels[] = {
   { "guest", &gGuestBackdoorOps, 0, NULL, NULL, {0} },
};

static HgfsServerConfig gHgfsGuestCfgSettings = {
   (HGFS_CONFIG_SHARE_ALL_HOST_DRIVES_ENABLED | HGFS_CONFIG_VOL_INFO_MIN),
   HGFS_MAX_CACHED_FILENODES
};

/* HGFS server info state. Referenced by each separate channel that uses it. */
static HgfsChannelServerData gHgfsChannelServerInfo = { NULL, {0} };

static void HgfsChannelTeardownChannel(HgfsChannelData *channel);
static void HgfsChannelTeardownServer(HgfsChannelServerData *serverInfo);
static void HgfsChannelExitChannel(HgfsChannelData *channel);


/*
 *----------------------------------------------------------------------------
 *
 * HGFS SERVER DATA FUNCTIONS
 *
 *----------------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGetServer --
 *
 *      Increment the server data reference count.
 *
 * Results:
 *      The value of the reference count before the increment.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static uint32
HgfsChannelGetServer(HgfsChannelServerData *serverInfo)   // IN/OUT: ref count
{
   ASSERT(NULL != serverInfo);
   return Atomic_ReadInc32(&serverInfo->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelPutServer --
 *
 *      Decrement server data reference count.
 *
 *      Teardown the server data object if removed the final reference.
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
HgfsChannelPutServer(HgfsChannelServerData *serverInfo)   // IN/OUT: ref count
{
   ASSERT(NULL != serverInfo);
   if (Atomic_ReadDec32(&serverInfo->refCount) == 1) {
      HgfsChannelTeardownServer(serverInfo);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelInitServer --
 *
 *      Initialize HGFS server and save the state.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsChannelInitServer(HgfsServerMgrCallbacks *mgrCb,       // IN: server manager callbacks
                      HgfsChannelServerData *serverInfo)   // IN/OUT: ref count
{
   Bool result;

   ASSERT(NULL == serverInfo->serverCBTable);

   Debug("%s: Initialize Hgfs server.\n", __FUNCTION__);

   /* If we have a new connection initialize the server session with default settings. */
   result = HgfsServer_InitState(&serverInfo->serverCBTable,
                                 &gHgfsGuestCfgSettings,
                                 mgrCb);
   if (!result) {
      Debug("%s: Could not init Hgfs server.\n", __FUNCTION__);
   }
   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelExitServer --
 *
 *      Reset the HGFS server and destroy the state.
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
HgfsChannelExitServer(HgfsChannelServerData *serverInfo)   // IN/OUT: ref count
{
   if (NULL != serverInfo->serverCBTable) {
      Debug("%s: Teardown Hgfs server.\n", __FUNCTION__);
      HgfsServer_ExitState();
      serverInfo->serverCBTable = NULL;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelTeardownServer --
 *
 *      Teardown the HGFS server state for all connections.
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
HgfsChannelTeardownServer(HgfsChannelServerData *serverInfo) // IN/OUT: connection manager object
{
   HgfsChannelExitServer(serverInfo);
}


/*
 *----------------------------------------------------------------------------
 *
 * CHANNEL DATA FUNCTIONS
 *
 *----------------------------------------------------------------------------
 */


 /*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGetChannel --
 *
 *      Increment channel data reference count.
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
HgfsChannelGetChannel(HgfsChannelData *channel)   // IN/OUT: ref count
{
   ASSERT(NULL != channel);
   return Atomic_ReadInc32(&channel->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelPutChannel --
 *
 *      Decrement channel reference count.
 *
 *      Teardown channel object if removed the final reference.
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
HgfsChannelPutChannel(HgfsChannelData *channel)   // IN/OUT: ref count
{
   ASSERT(NULL != channel);
   if (Atomic_ReadDec32(&channel->refCount) == 1) {
      HgfsChannelTeardownChannel(channel);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelInitChannel --
 *
 *      Initializes a channel by initializing the HGFS server state.
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
HgfsChannelInitChannel(HgfsChannelData *channel,          // IN/OUT: channel object
                       HgfsServerMgrCallbacks *mgrCb,     // IN: server manager callbacks
                       HgfsChannelServerData *serverInfo) // IN/OUT: server info
{
   Bool result = TRUE;
   uint32 serverInfoCount;

   channel->state = 0;
   /*
    * Reference the HGFS server as it will be used by the new channel.
    * The HGFS server should only be initialized once, i.e. on the first
    * caller instance, otherwise only reference the server info for
    * the new channel.
    */
   serverInfoCount = HgfsChannelGetServer(serverInfo);
   /* Referenced the server, save it for dereferencing. */
   channel->serverInfo = serverInfo;
   if (0 == serverInfoCount) {
      /* The HGFS server has not been initialized, do it now. */
      result = HgfsChannelInitServer(mgrCb, channel->serverInfo);
      if (!result) {
         Debug("%s: Could not init Hgfs server.\n", __FUNCTION__);
         goto exit;
      }
   }

   channel->state |= HGFS_CHANNEL_STATE_INIT;

exit:
   if (!result) {
      HgfsChannelExitChannel(channel);
   }
   Debug("%s: Init channel return %d.\n", __FUNCTION__, result);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelExitChannel --
 *
 *      Teardown the channel and teardown the HGFS server.
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
   if (NULL != channel->serverInfo) {
      /* Remove the reference for the HGFS server info. */
      HgfsChannelPutServer(channel->serverInfo);
      channel->serverInfo = NULL;
    }
   channel->state = 0;
   Debug("%s: Exit channel returns.\n", __FUNCTION__);
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
HgfsChannelActivateChannel(HgfsChannelData *channel,   // IN/OUT: channel object
                           void *rpc,                  // IN: Rpc channel
                           void *rpcCallback)          // IN: Rpc callback
{
   Bool success = FALSE;
   struct HgfsGuestConn *connData = NULL;

   if (channel->ops->init(&channel->serverInfo->serverCBTable->session,
                          rpc,
                          rpcCallback,
                          &connData)) {
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
HgfsChannelDeactivateChannel(HgfsChannelData *channel)   // IN/OUT: channel object
{
   channel->ops->exit(channel->connection);
   channel->state &= ~HGFS_CHANNEL_STATE_CBINIT;
   channel->connection = NULL;
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
   return (0 != (channel->state & HGFS_CHANNEL_STATE_INIT) &&
           0 != (channel->state & HGFS_CHANNEL_STATE_CBINIT));
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
 * HgfsChannelTeardownChannel --
 *
 *      Teardown the channel for HGFS.
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
HgfsChannelTeardownChannel(HgfsChannelData *channel) // IN/OUT: connection manager object
{
   if (HgfsChannelIsChannelActive(channel)) {
      HgfsChannelDeactivateChannel(channel);
   }
   HgfsChannelExitChannel(channel);
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
HgfsChannelGuest_Init(HgfsServerMgrData *mgrData,    // IN/OUT: server manager data
                      HgfsServerMgrCallbacks *mgrCb) // IN: server manager callbacks
{
   Bool success = FALSE;
   HgfsChannelData *channel = &gHgfsChannels[0]; // Shared channel (internal RPC)
   uint32 channelRefCount;

   ASSERT(NULL != mgrData);
   ASSERT(NULL == mgrData->connection);
   /* Currently, the RPC override is not implemented. */
   ASSERT(NULL == mgrData->rpc);
   ASSERT(NULL == mgrData->rpcCallback);
   ASSERT(NULL != mgrData->appName);

   Debug("%s: app %s rpc = %p rpc cb = %p.\n", __FUNCTION__,
         mgrData->appName, mgrData->rpc, mgrData->rpcCallback);

   if (NULL != mgrData->rpc || NULL != mgrData->rpcCallback) {
      /*
       * XXX - Would malloc a new channel here and activate
       * with the required RPC.
       */

      Debug("%s: Guest channel RPC override not supported.\n", __FUNCTION__);
      goto exit;
   }

   /*
    * Reference the channel. Initialize only for the first
    * caller instance, otherwise only reference the channel for
    * return to the caller.
    */
   channelRefCount = HgfsChannelGetChannel(channel);
   /* We have referenced the channel, save it for later dereference. */
   mgrData->connection = channel;
   if (0 == channelRefCount) {

      /* Initialize channels objects. */
      if (!HgfsChannelInitChannel(channel, mgrCb, &gHgfsChannelServerInfo)) {
         Debug("%s: Could not init channel.\n", __FUNCTION__);
         goto exit;
      }

      /* Call the channels initializers. */
      if (!HgfsChannelActivateChannel(channel,
                                      mgrData->rpc,
                                      mgrData->rpcCallback)) {
         Debug("%s: Could not activate channel.\n", __FUNCTION__);
         goto exit;
      }
   }

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
 *      Dereference the channel which for the final reference will
 *      close the channel for HGFS.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsChannelGuest_Exit(HgfsServerMgrData *mgrData) // IN/OUT: connection manager object
{
   HgfsChannelData *channel;

   ASSERT(NULL != mgrData);
   ASSERT(NULL != mgrData->appName);

   channel = mgrData->connection;

   Debug("%s: app %s rpc = %p rpc cb = %p chn = %p.\n", __FUNCTION__,
         mgrData->appName, mgrData->rpc, mgrData->rpcCallback, channel);

   if (NULL != channel) {
      HgfsChannelPutChannel(channel);
      mgrData->connection = NULL;
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
   HgfsChannelData *channel;
   Bool result = FALSE;

   ASSERT(NULL != mgrData);
   ASSERT(NULL != mgrData->connection);
   ASSERT(NULL != mgrData->appName);

   channel = mgrData->connection;

   Debug("%s: %s Channel receive request.\n", __FUNCTION__, mgrData->appName);

   if (HgfsChannelIsChannelActive(channel)) {
      result = HgfsChannelReceive(channel,
                                  packetIn,
                                  packetInSize,
                                  packetOut,
                                  packetOutSize);
   }

   Debug("%s: Channel receive returns %#x.\n", __FUNCTION__, result);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuest_InvalidateInactiveSessions -
 *
 *    Sends a request to invalidate all the inactive HGFS server sessions.
 *
 * Results:
 *    Number of active sessions remaining inside the HGFS server.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

uint32
HgfsChannelGuest_InvalidateInactiveSessions(HgfsServerMgrData *mgrData) // IN: conn manager
{
   HgfsChannelData *channel;
   uint32 result = 0;

   ASSERT(NULL != mgrData);
   ASSERT(NULL != mgrData->connection);
   ASSERT(NULL != mgrData->appName);

   channel = mgrData->connection;

   Debug("%s: %s Channel. Invalidating inactive sessions.\n",
         __FUNCTION__, mgrData->appName);

   if (HgfsChannelIsChannelActive(channel)) {
      result = channel->ops->invalidateInactiveSessions(channel->connection);
   }

   return result;
}

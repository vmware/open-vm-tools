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
 * hgfsChannel.c --
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

typedef enum {
   HGFS_GST_CONN_UNINITIALIZED,
   HGFS_GST_CONN_NOTCONNECTED,
   HGFS_GST_CONN_CONNECTED,
} HgfsGuestConnState;


/* Since there is only one connection we use globals. */
typedef struct HgfsGuestConn {
   Atomic_uint32 refCount;                   /* Reference count. */
   HgfsGuestConnState state;
   const HgfsServerSessionCallbacks *serverCbTable; /* Server session callbacks. */
   HgfsServerChannelCallbacks channelCbTable;
   void *serverSession;
   size_t packetOutLen;
   unsigned char *clientPacketOut;                 /* Client supplied buffer. */
   unsigned char packetOut[HGFS_LARGE_PACKET_MAX]; /* For RPC msg callbacks. */
} HgfsGuestConn;


/* Callback functions. */
static Bool HgfsChannelGuestBdInit(const HgfsServerSessionCallbacks *serverCBTable,
                                   void *rpc,
                                   void *rpcCallback,
                                   HgfsGuestConn **connection);
static void HgfsChannelGuestBdExit(HgfsGuestConn *data);
static Bool HgfsChannelGuestBdSend(void *data,
                                   HgfsPacket *packet,
                                   HgfsSendFlags flags);
static Bool HgfsChannelGuestBdReceive(HgfsGuestConn *data,
                                      char const *packetIn,
                                      size_t packetInSize,
                                      char *packetOut,
                                      size_t *packetOutSize);
static uint32 HgfsChannelGuestBdInvalidateInactiveSessions(HgfsGuestConn *data);

const HgfsGuestChannelCBTable gGuestBackdoorOps = {
   HgfsChannelGuestBdInit,
   HgfsChannelGuestBdExit,
   HgfsChannelGuestBdReceive,
   HgfsChannelGuestBdInvalidateInactiveSessions,
};

/* Private functions. */
static Bool HgfsChannelGuestConnConnect(HgfsGuestConn *connData);
static void HgfsChannelGuestConnDestroy(HgfsGuestConn *connData);
static Bool HgfsChannelGuestReceiveInternal(HgfsGuestConn *connData,
                                            char const *packetIn,
                                            size_t packetInSize,
                                            char *packetOut,
                                            size_t *packetOutSize);


/*
 *----------------------------------------------------------------------------
 *
 * CONNECTION DATA FUNCTIONS
 *
 *----------------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnGet --
 *
 *      Increment connection reference count.
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
HgfsChannelGuestConnGet(HgfsGuestConn *connData)   // IN: connection
{
   ASSERT(connData);
   Atomic_Inc(&connData->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnPut --
 *
 *      Decrement connection reference count.
 *
 *      Free connection data if this is the last reference.
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
HgfsChannelGuestConnPut(HgfsGuestConn *connData)   // IN: connection
{
   ASSERT(connData);
   if (Atomic_ReadDec32(&connData->refCount) == 1) {
      HgfsChannelGuestConnDestroy(connData);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnInit --
 *
 *      Initializes the connection.
 *
 * Results:
 *      TRUE always and the channel initialized.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestConnInit(HgfsGuestConn **connData,                        // IN/OUT: channel object
                         const HgfsServerSessionCallbacks *serverCBTable) // IN: server callbacks
{
   HgfsGuestConn *conn = NULL;

   conn = Util_SafeCalloc(1, sizeof *conn);

   /* Give ourselves a reference of one. */
   HgfsChannelGuestConnGet(conn);
   conn->serverCbTable = serverCBTable;
   conn->state = HGFS_GST_CONN_NOTCONNECTED;

   *connData = conn;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnExit --
 *
 *      Teardown the connection.
 *
 *      Removes the reference and if it is the last will cause the connection
 *      to be destroyed.
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
HgfsChannelGuestConnExit(HgfsGuestConn *connData) // IN/OUT: channel object
{
   connData->state = HGFS_GST_CONN_UNINITIALIZED;

   HgfsChannelGuestConnPut(connData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnDestroy --
 *
 *      Destroy the connection.
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
HgfsChannelGuestConnDestroy(HgfsGuestConn *connData) // IN/OUT: channel object
{
   /* Make sure the server closes it's own session data. */
   if (NULL != connData->serverSession) {
      connData->serverCbTable->close(connData->serverSession);
      connData->serverSession = NULL;
   }
   free(connData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnCreate --
 *
 *      Create's the RPC connection for the HGFS guest if asked.
 *
 *      Create the pseudo connection for the guest - state transition.
 *      (See the comment in the function where the RPC initialization
 *      is expected to be added.
 *      This entails is registering our callback to receive messages for the
 *      connection object passed. We will have the ability to receive
 *      requests until we unregister our callback.)
 *
 *      NOTE: There is only handler and connction that can be used for
 *      all HGFS guest requests.
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
HgfsChannelGuestConnCreate(HgfsGuestConn *connData,      // IN: connection
                           void *rpc,                    // IN: Rpc channel unused
                           void *rpcCallback)            // IN: Rpc callback unused
{
   ASSERT(connData->state == HGFS_GST_CONN_NOTCONNECTED);

   /*
    * Rpc may be NULL for some cases. For example, if we
    * just need to provide an HGFS server connection
    * not associated with an HGFS only RPC connection.
    */
   if (connData->state == HGFS_GST_CONN_NOTCONNECTED) {

      /* XXX - Here is where we would register an RPC callback if required. */

      connData->state = HGFS_GST_CONN_CONNECTED;
      HgfsChannelGuestConnGet(connData);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnClose --
 *
 *      Closes the connection for the HGFS guest.
 *
 *      If required unregisters the callback will prevent us from
 *      receiving any more requests closing the connection.
 *
 * Results:
 *      TRUE if closed, FALSE if was not connected.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestConnClose(HgfsGuestConn *connData,    // IN: Connection
                          void *rpc,                  // IN: Rpc channel unused
                          void *rpcCallback)          // IN: Rpc callback unused
{
   Bool result = FALSE;

   if (connData->state == HGFS_GST_CONN_CONNECTED) {
      /* XXX - Here is where we would unregister an RPC callback. */

      /* Clear the connection object since we are unregistered. */
      connData->state = HGFS_GST_CONN_NOTCONNECTED;
      HgfsChannelGuestConnPut(connData);
      result = TRUE;
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnConnect --
 *
 *      Send connection to the server.
 *
 * Results:
 *      TRUE if server returns a data object, FALSE if not.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestConnConnect(HgfsGuestConn *connData)  // IN: our connection data
{
   Bool result;
   static HgfsServerChannelData HgfsBdCapData = {
      0,
      HGFS_LARGE_PACKET_MAX
   };

   connData->channelCbTable.getWriteVa = NULL;
   connData->channelCbTable.getReadVa = NULL;
   connData->channelCbTable.putVa = NULL;
   connData->channelCbTable.send = HgfsChannelGuestBdSend;
   result = connData->serverCbTable->connect(connData,
                                             &connData->channelCbTable,
                                             &HgfsBdCapData,
                                             &connData->serverSession);
   if (result) {
      HgfsChannelGuestConnGet(connData);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnDisconnect --
 *
 *      Send disconnect to the server.
 *
 *      NOTE: The server data will be maintained until
 *      the connection is totally closed (last reference is gone).
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
HgfsChannelGuestConnDisconnect(HgfsGuestConn *connData)  // IN: connection
{
   if (connData->serverSession != NULL) {
      /* Tell the server to to disconnect the session. */
      connData->serverCbTable->disconnect(connData->serverSession);
      HgfsChannelGuestConnPut(connData);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestConnCloseInternal --
 *
 *      Close the client and send a disconnect to the server for the session.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Closes the client connection and empties the queues.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsChannelGuestConnCloseInternal(HgfsGuestConn *connData, // IN: Connection data
                                  void *rpc,               // IN: Rpc channel unused
                                  void *rpcCallback)       // IN: Rpc callback unused
{
   /* Close (unregister the backdoor RPC) connection. */
   if (HgfsChannelGuestConnClose(connData, rpc, rpcCallback)) {
      /* Disconnect the connection from the server. */
      HgfsChannelGuestConnDisconnect(connData);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestReceiveInternal --
 *
 *    Process packet not associated with any session.
 *
 *    This function is used in the HGFS server inside Tools.
 *
 *    Create an internal session if not already created, and process the packet.
 *
 * Results:
 *    TRUE if received packet ok and processed, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestReceiveInternal(HgfsGuestConn *connData,  // IN: connection
                                char const *packetIn,     // IN: incoming packet
                                size_t packetInSize,      // IN: incoming packet size
                                char *packetOut,          // OUT: outgoing packet
                                size_t *packetOutSize)    // IN/OUT: outgoing packet size
{
   HgfsPacket packet;

   ASSERT(packetIn);
   ASSERT(packetOut);
   ASSERT(packetOutSize);

   if (connData->state == HGFS_GST_CONN_UNINITIALIZED) {
      /* The connection was closed as we are exiting, so bail. */
      *packetOutSize = 0;
      return FALSE;
   }

   /* This is just a ping, return nothing. */
   if (*packetOutSize == 0) {
      return TRUE;
   }

   /*
    * Create the session if not already created.
    * This session is destroyed in HgfsServer_ExitState.
    */
   if (connData->serverSession == NULL) {
      /* Do our guest connect now which will inform the server. */
      if (!HgfsChannelGuestConnConnect(connData)) {
         *packetOutSize = 0;
         return FALSE;
      }
   }

   memset(&packet, 0, sizeof packet);
   /* For backdoor there is only one iov */
   packet.iov[0].va = (void *)packetIn;
   packet.iov[0].len = packetInSize;
   packet.iovCount = 1;
   packet.metaPacket = (void *)packetIn;
   packet.metaPacketDataSize = packetInSize;
   packet.metaPacketSize = packetInSize;
   packet.replyPacket = packetOut;
   packet.replyPacketSize = *packetOutSize;
   packet.state |= HGFS_STATE_CLIENT_REQUEST;

   /* The server will perform a synchronous processing of requests. */
   connData->serverCbTable->receive(&packet, connData->serverSession);

   *packetOutSize = connData->packetOutLen;

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * REGISTERED CALLBACK FUNCTIONS
 *
 * XXX - Where we would have any internally registered callback routines.
 * This routine would call HgfsChannelGuestReceiveInternal to process the
 * request.
 *
 *----------------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------------
 *
 * GUEST CHANNEL CALLBACKS
 *
 *----------------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestBdReceive --
 *
 *    Process packet not associated with our registered callback.
 *
 *
 * Results:
 *    TRUE if received packet ok and processed, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsChannelGuestBdReceive(HgfsGuestConn *connData,    // IN: connection
                          char const *packetIn,       // IN: incoming packet
                          size_t packetInSize,        // IN: incoming packet size
                          char *packetOut,            // OUT: outgoing packet
                          size_t *packetOutSize)      // IN/OUT: outgoing packet size
{
   Bool result = TRUE;

   ASSERT(NULL != packetIn);
   ASSERT(NULL != packetOut);
   ASSERT(NULL != packetOutSize);
   ASSERT(NULL != connData);

   if (NULL == connData) {
      result = FALSE;
      goto exit;
   }

   connData->packetOutLen = *packetOutSize;
   connData->clientPacketOut = packetOut;

   result = HgfsChannelGuestReceiveInternal(connData,
                                            packetIn,
                                            packetInSize,
                                            connData->clientPacketOut,
                                            packetOutSize);

   connData->clientPacketOut = NULL;
   connData->packetOutLen = sizeof connData->packetOut;

exit:
   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestBdInvalidateInactiveSessions --
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
HgfsChannelGuestBdInvalidateInactiveSessions(HgfsGuestConn *connData)  // IN: connection
{
   ASSERT(NULL != connData);

   if (NULL == connData) {
      return 0;
   }

   if (connData->state == HGFS_GST_CONN_UNINITIALIZED) {
      /* The connection was closed as we are exiting, so bail. */
      return 0;
   }

   /* The server will perform a synchronous processing of requests. */
   if (connData->serverSession) {
      return connData->serverCbTable->invalidateInactiveSessions(connData->serverSession);
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsChannelGuestBdSend --
 *
 *      Send reply to the request
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestBdSend(void *conn,              // IN: our connection data
                       HgfsPacket *packet,      // IN/OUT: Hgfs Packet
                       HgfsSendFlags flags)     // IN: Flags to say how to process
{
   HgfsGuestConn *connData = conn;

   ASSERT(NULL != connData);
   ASSERT(NULL != packet);
   ASSERT(NULL != packet->replyPacket);
   ASSERT(packet->replyPacketDataSize <= connData->packetOutLen);
   ASSERT(packet->replyPacketSize == connData->packetOutLen);

   if (packet->replyPacketDataSize > connData->packetOutLen) {
      packet->replyPacketDataSize = connData->packetOutLen;
   }
   connData->packetOutLen = (uint32)packet->replyPacketDataSize;

   if (!(flags & HGFS_SEND_NO_COMPLETE)) {
      connData->serverCbTable->sendComplete(packet,
                                            connData->serverSession);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestBdInit --
 *
 *      Called from channel manager.
 *
 *      Initializes our channel connections.
 *
 * Results:
 *      Always TRUE.
 *
 * Side effects:
 *      Registers RPC call.
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsChannelGuestBdInit(const HgfsServerSessionCallbacks *serverCBTable, // IN: server callbacks
                       void *rpc,                                       // IN: Rpc channel unused
                       void *rpcCallback,                               // IN: Rpc callback unused
                       HgfsGuestConn **connection)                      // OUT: connection object
{
   HgfsGuestConn *connData = NULL;
   Bool result;

   ASSERT(NULL != connection);

   /* Create our connection object. */
   result = HgfsChannelGuestConnInit(&connData,
                                     serverCBTable);
   if (!result) {
      Debug("%s: Error: guest connection initialized.\n", __FUNCTION__);
      goto exit;
   }

   /*
    * Create our connection now with any rpc handle and callback.
    */
   HgfsChannelGuestConnCreate(connData,
                              rpc,
                              rpcCallback);

exit:
   if (!result) {
      if (NULL != connData) {
         HgfsChannelGuestBdExit(connData);
         connData = NULL;
      }
   }
   *connection = connData;
   Debug("%s: guest initialized.\n", __FUNCTION__);
   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsChannelGuestBdExit --
 *
 *      Tearsdown our channel connections.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters RPC call.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsChannelGuestBdExit(HgfsGuestConn *connData)
{
   ASSERT(NULL != connData);

   if (NULL != connData) {
      /* Currently no rpc to unregister. */
      HgfsChannelGuestConnCloseInternal(connData, NULL, NULL);
      HgfsChannelGuestConnExit(connData);
   }
}

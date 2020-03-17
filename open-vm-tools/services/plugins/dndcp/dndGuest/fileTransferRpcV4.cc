/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * @fileTransferRpcV4.cc --
 *
 * Implementation of common layer file transfer rpc version 4 object.
 */

#include "fileTransferRpcV4.hh"
#include "dndCPTransport.h"

#include "dndMsg.h"
#include "hgfsServer.h"
#include "str.h"
#include "util.h"

extern "C" {
   #include "dndCPMsgV4.h"

#if defined VMX86_TOOLS
   #include "debug.h"

   #define LOG(level, ...) Debug(__VA_ARGS__)
#else
   #define LOGLEVEL_MODULE dnd
   #include "loglevel_user.h"
#endif
}


/**
 * Create transport object and register callback.
 *
 * @param[in] transport for sending/receiving messages.
 */

FileTransferRpcV4::FileTransferRpcV4(DnDCPTransport *transport)
   : mTransport(transport),
#ifdef VMX86_TOOLS
     mTransportInterface(TRANSPORT_GUEST_CONTROLLER_FT)
#else
     mTransportInterface(TRANSPORT_HOST_CONTROLLER_FT)
#endif
{
   ASSERT(mTransport);

#ifdef VMX86_TOOLS
   mUtil.Init(this, DND_CP_MSG_SRC_GUEST, DND_CP_MSG_TYPE_FT);
#else
   mUtil.Init(this, DND_CP_MSG_SRC_HOST, DND_CP_MSG_TYPE_FT);
#endif
}


/**
 * Init. Register the rpc with transport. Send a ping message to controller to
 * to let it know our version and capability.
 *
 * XXX Capability is not implemented yet.
 */

void
FileTransferRpcV4::Init(void)
{
   ASSERT(mTransport);
   mTransport->RegisterRpc(this, mTransportInterface);
}


/**
 * Sends hgfs packet to peer.
 *
 * @param[in] sessionId DnD/CopyPaste session id.
 * @param[in] packet packet data.
 * @param[in] packetSize packet size.
 *
 * @return true on success, false otherwise.
 */

bool
FileTransferRpcV4::SendHgfsPacket(uint32 sessionId,
                                  const uint8 *packet,
                                  uint32 packetSize)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = FT_CMD_HGFS_REQUEST;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, packet, packetSize);
}


/**
 * Sends hgfs reply back to peer.
 *
 * @param[in] sessionId DnD/CopyPaste session id.
 * @param[in] packet reply packet data.
 * @param[in] packetSize reply packet size.
 *
 * @return true on success, false otherwise.
 */

bool
FileTransferRpcV4::SendHgfsReply(uint32 sessionId,
                                 const uint8 *packet,
                                 uint32 packetSize)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = FT_CMD_HGFS_REPLY;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, packet, packetSize);
}


/**
 * Send a packet.
 *
 * @param[in] destId destination address id.
 * @param[in] packet
 * @param[in] length packet length
 *
 * @return true on success, false otherwise.
 */

bool
FileTransferRpcV4::SendPacket(uint32 destId,
                              const uint8 *packet,
                              size_t length)
{
   return mTransport->SendPacket(destId, mTransportInterface, packet, length);
}


/**
 * Handle a received message.
 *
 * @param[in] params parameter list for received message.
 * @param[in] binary attached binary data for received message.
 * @param[in] binarySize
 */

void
FileTransferRpcV4::HandleMsg(RpcParams *params,
                             const uint8 *binary,
                             uint32 binarySize)
{
   ASSERT(params);

   LOG(4, "%s: Got %s[%d], sessionId %d, srcId %d, binary size %d.\n",
       __FUNCTION__, DnDCPMsgV4_LookupCmd(params->cmd), params->cmd,
       params->sessionId, params->addrId, binarySize);

   switch (params->cmd) {
   case FT_CMD_HGFS_REQUEST:
      HgfsPacketReceived.emit(params->sessionId, binary, binarySize);
      break;
   case FT_CMD_HGFS_REPLY:
      HgfsReplyReceived.emit(params->sessionId, binary, binarySize);
      break;
   case DNDCP_CMD_PING_REPLY:
      break;
   default:
      LOG(0, "%s: Got unknown command %d.\n", __FUNCTION__, params->cmd);
      break;
   }
}


/**
 * Callback from transport layer after received a packet from srcId.
 *
 * @param[in] srcId addressId where the packet is from
 * @param[in] packet
 * @param[in] packetSize
 */

void
FileTransferRpcV4::OnRecvPacket(uint32 srcId,
                                const uint8 *packet,
                                size_t packetSize)
{
   mUtil.OnRecvPacket(srcId, packet, packetSize);
}

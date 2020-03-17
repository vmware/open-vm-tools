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
 * @copyPasteRpcV4.cc --
 *
 * Rpc layer object for CopyPaste version 4.
 */

#include "copyPasteRpcV4.hh"

extern "C" {
#if defined VMX86_TOOLS
   #include "debug.h"

   #define LOG(level, ...) Debug(__VA_ARGS__)
#else
   #define LOGLEVEL_MODULE dnd
   #include "loglevel_user.h"
#endif

   #include "dndClipboard.h"
}

#include "util.h"


/**
 * Constructor.
 *
 * @param[in] transport for sending packets.
 */

CopyPasteRpcV4::CopyPasteRpcV4(DnDCPTransport *transport)
   : mTransport(transport),
#ifdef VMX86_TOOLS
     mTransportInterface(TRANSPORT_GUEST_CONTROLLER_CP)
#else
     mTransportInterface(TRANSPORT_HOST_CONTROLLER_CP)
#endif
{
   ASSERT(mTransport);

#ifdef VMX86_TOOLS
   mUtil.Init(this, DND_CP_MSG_SRC_GUEST, DND_CP_MSG_TYPE_CP);
#else
   mUtil.Init(this, DND_CP_MSG_SRC_HOST, DND_CP_MSG_TYPE_CP);
#endif
}


/**
 * Init.
 */

void
CopyPasteRpcV4::Init()
{
   ASSERT(mTransport);
   mTransport->RegisterRpc(this, mTransportInterface);
}


/**
 * Send Ping message to controller.
 *
 * @param[in] caps capabilities value.
 */

void
CopyPasteRpcV4::SendPing(uint32 caps)
{
   mUtil.SendPingMsg(DEFAULT_CONNECTION_ID, caps);
}


/**
 * Send cmd CP_CMD_REQUEST_CLIPBOARD to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] isActive active or passive CopyPaste
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::SrcRequestClip(uint32 sessionId,
                               bool isActive)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = CP_CMD_REQUEST_CLIPBOARD;
   params.sessionId = sessionId;
   params.optional.cpInfo.major = mUtil.GetVersionMajor();
   params.optional.cpInfo.minor = mUtil.GetVersionMinor();
   params.optional.cpInfo.isActive = isActive;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd CP_CMD_SEND_CLIPBOARD to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] isActive active or passive CopyPaste
 * @param[in] clip cross-platform clipboard data.
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::DestSendClip(uint32 sessionId,
                             bool isActive,
                             const CPClipboard* clip)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = CP_CMD_SEND_CLIPBOARD;
   params.sessionId = sessionId;
   params.optional.cpInfo.major = mUtil.GetVersionMajor();
   params.optional.cpInfo.minor = mUtil.GetVersionMinor();
   params.optional.cpInfo.isActive = isActive;

   return mUtil.SendMsg(&params, clip);
}


/**
 * Send cmd CP_CMD_REQUEST_FILE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::RequestFiles(uint32 sessionId,
                             const uint8 *stagingDirCP,
                             uint32 sz)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = CP_CMD_REQUEST_FILES;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, stagingDirCP, sz);
}


/**
 * Send cmd CP_CMD_SEND_FILES_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] success the file transfer operation was successful or not
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::SendFilesDone(uint32 sessionId,
                              bool success,
                              const uint8 *stagingDirCP,
                              uint32 sz)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = CP_CMD_SEND_FILES_DONE;
   params.status = success ?
      DND_CP_MSG_STATUS_SUCCESS :
      DND_CP_MSG_STATUS_ERROR;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, stagingDirCP, sz);
}


/**
 * Send cmd CP_CMD_GET_FILES_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] success the file transfer operation was successful or not
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::GetFilesDone(uint32 sessionId,
                             bool success)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = CP_CMD_GET_FILES_DONE;
   params.status = success ?
      DND_CP_MSG_STATUS_SUCCESS :
      DND_CP_MSG_STATUS_ERROR;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send a packet.
 *
 * @param[in] destId destination address id.
 * @param[in] packet
 * @param[in] length packet length in bytes
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV4::SendPacket(uint32 destId,
                           const uint8 *packet,
                           size_t length)
{
   ASSERT(mTransport);
   return mTransport->SendPacket(destId, mTransportInterface, packet, length);
}


/**
 * Handle a received message.
 *
 * @param[in] params parameter list for received message.
 * @param[in] binary attached binary data for received message.
 * @param[in] binarySize in bytes
 */

void
CopyPasteRpcV4::HandleMsg(RpcParams *params,
                          const uint8 *binary,
                          uint32 binarySize)
{
   ASSERT(params);

   LOG(4, "%s: Got %s[%d], sessionId %d, srcId %d, binary size %d.\n",
       __FUNCTION__, DnDCPMsgV4_LookupCmd(params->cmd), params->cmd,
       params->sessionId, params->addrId, binarySize);

   switch (params->cmd) {
   case CP_CMD_RECV_CLIPBOARD:
      CPClipboard clip;
      if (!binary || binarySize == 0) {
         LOG(0, "%s: invalid clipboard data.\n", __FUNCTION__);
         break;
      }
      CPClipboard_Init(&clip);
      if (!CPClipboard_Unserialize(&clip, (void *)binary, binarySize)) {
         LOG(0, "%s: CPClipboard_Unserialize failed.\n", __FUNCTION__);
         break;
      }
      srcRecvClipChanged.emit(params->sessionId,
                              1 == params->optional.cpInfo.isActive,
                              &clip);
      CPClipboard_Destroy(&clip);
      break;
   case CP_CMD_REQUEST_CLIPBOARD:
      destRequestClipChanged.emit(params->sessionId,
                                  1 == params->optional.cpInfo.isActive);
      break;
   case CP_CMD_REQUEST_FILES:
      requestFilesChanged.emit(params->sessionId, binary, binarySize);
      break;
   case CP_CMD_GET_FILES_DONE:
      getFilesDoneChanged.emit(params->sessionId,
                               DND_CP_MSG_STATUS_SUCCESS == params->status,
                               binary,
                               binarySize);
      break;
   case DNDCP_CMD_PING_REPLY:
      pingReplyChanged.emit(params->optional.version.capability);
      break;
   case DNDCP_CMP_REPLY:
      LOG(0, "%s: Got cmp reply command %d.\n", __FUNCTION__, params->cmd);
      cmdReplyChanged.emit(params->cmd, params->status);
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
CopyPasteRpcV4::OnRecvPacket(uint32 srcId,
                             const uint8 *packet,
                             size_t packetSize)
{
   mUtil.OnRecvPacket(srcId, packet, packetSize);
}

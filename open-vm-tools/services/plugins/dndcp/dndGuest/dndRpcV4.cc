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
 * @DnDRpcV4.cc --
 *
 * Implementation of the DnDRpcV4 object.
 */

#include "dndRpcV4.hh"

extern "C" {
#if defined VMX86_TOOLS
   #include "debug.h"
   #define LOG(level, msg) (Debug msg)
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

DnDRpcV4::DnDRpcV4(DnDCPTransport *transport)
   : mTransport(transport),
#ifdef VMX86_TOOLS
     mTransportInterface(TRANSPORT_GUEST_CONTROLLER_DND)
#else
     mTransportInterface(TRANSPORT_HOST_CONTROLLER_DND)
#endif
{
   ASSERT(mTransport);

#ifdef VMX86_TOOLS
   mUtil.Init(this, DND_CP_MSG_SRC_GUEST, DND_CP_MSG_TYPE_DND);
#else
   mUtil.Init(this, DND_CP_MSG_SRC_HOST, DND_CP_MSG_TYPE_DND);
#endif
}


/**
 * Init.
 */

void
DnDRpcV4::Init()
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
DnDRpcV4::SendPing(uint32 caps)
{
   mUtil.SendPingMsg(DEFAULT_CONNECTION_ID, caps);
}


/**
 * Send cmd DND_CMD_SRC_DRAG_BEGIN_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcDragBeginDone(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_SRC_DRAG_BEGIN_DONE;
   params.sessionId = sessionId;
   params.optional.version.major = 4;
   params.optional.version.minor = 0;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_UPDATE_FEEDBACK to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] feedback current dnd operation feedback.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::UpdateFeedback(uint32 sessionId,
                         DND_DROPEFFECT feedback)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_UPDATE_FEEDBACK;
   params.sessionId = sessionId;
   params.optional.feedback.feedback = feedback;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_PRIV_DRAG_ENTER to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcPrivDragEnter(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_PRIV_DRAG_ENTER;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_PRIV_DRAG_LEAVE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcPrivDragLeave(uint32 sessionId,
                           int32 x,
                           int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_PRIV_DRAG_LEAVE;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_PRIV_DROP to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcPrivDrop(uint32 sessionId,
                      int32 x,
                      int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_PRIV_DROP;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_SRC_DROP to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcDrop(uint32 sessionId,
                  int32 x,
                  int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_SRC_DROP;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_SRC_DROP_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcDropDone(uint32 sessionId,
                      const uint8 *stagingDirCP,
                      uint32 sz)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_SRC_DROP_DONE;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, stagingDirCP, sz);
}


/**
 * Send cmd DND_CMD_SRC_CANCEL to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SrcCancel(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_SRC_CANCEL;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_DEST_DRAG_ENTER to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] clip cross-platform clipboard data.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DestDragEnter(uint32 sessionId,
                        const CPClipboard *clip)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DEST_DRAG_ENTER;
   params.sessionId = sessionId;
   params.optional.version.major = mUtil.GetVersionMajor();
   params.optional.version.minor = mUtil.GetVersionMinor();

   if (clip) {
      return mUtil.SendMsg(&params, clip);
   } else {
      return mUtil.SendMsg(&params);
   }
}


/**
 * Send cmd DND_CMD_DEST_SEND_CLIPBOARD to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] clip cross-platform clipboard data.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DestSendClip(uint32 sessionId,
                       const CPClipboard *clip)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DEST_SEND_CLIPBOARD;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, clip);
}


/**
 * Send cmd DND_CMD_DRAG_NOT_PENDING to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DragNotPending(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DRAG_NOT_PENDING;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_DEST_DRAG_LEAVE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DestDragLeave(uint32 sessionId,
                        int32 x,
                        int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DEST_DRAG_LEAVE;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_DEST_DROP to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DestDrop(uint32 sessionId,
                   int32 x,
                   int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DEST_DROP;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_DEST_CANCEL to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::DestCancel(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_DEST_CANCEL;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_QUERY_EXITING to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::QueryExiting(uint32 sessionId,
                       int32 x,
                       int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_QUERY_EXITING;
   params.sessionId = sessionId;
   params.optional.queryExiting.major = mUtil.GetVersionMajor();
   params.optional.queryExiting.minor = mUtil.GetVersionMinor();
   params.optional.queryExiting.x = x;
   params.optional.queryExiting.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_UPDATE_UNITY_DET_WND to controller.
 *
 * @param[in] sessionId  Active session id the controller assigned earlier.
 * @param[in] show       Show or hide unity DnD detection window.
 * @param[in] unityWndId The unity window id.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::UpdateUnityDetWnd(uint32 sessionId,
                            bool show,
                            uint32 unityWndId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_UPDATE_UNITY_DET_WND;
   params.sessionId = sessionId;
   params.optional.updateUnityDetWnd.major = mUtil.GetVersionMajor();
   params.optional.updateUnityDetWnd.minor = mUtil.GetVersionMinor();
   params.optional.updateUnityDetWnd.show = show ? 1 : 0;
   params.optional.updateUnityDetWnd.unityWndId = unityWndId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_MOVE_MOUSE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::MoveMouse(uint32 sessionId,
                    int32 x,
                    int32 y)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_MOVE_MOUSE;
   params.sessionId = sessionId;
   params.optional.mouseInfo.x = x;
   params.optional.mouseInfo.y = y;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_REQUEST_FILES to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::RequestFiles(uint32 sessionId)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_REQUEST_FILES;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params);
}


/**
 * Send cmd DND_CMD_SEND_FILES_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] success the file transfer operation was successful or not
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SendFilesDone(uint32 sessionId,
                        bool success,
                        const uint8 *stagingDirCP,
                        uint32 sz)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_SEND_FILES_DONE;
   params.status = success ?
      DND_CP_MSG_STATUS_SUCCESS :
      DND_CP_MSG_STATUS_ERROR;
   params.sessionId = sessionId;

   return mUtil.SendMsg(&params, stagingDirCP, sz);
}


/**
 * Send cmd DND_CMD_GET_FILES_DONE to controller.
 *
 * @param[in] sessionId active session id the controller assigned earlier.
 * @param[in] success the file transfer operation was successful or not
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::GetFilesDone(uint32 sessionId,
                       bool success)
{
   RpcParams params;

   memset(&params, 0, sizeof params);
   params.addrId = DEFAULT_CONNECTION_ID;
   params.cmd = DND_CMD_GET_FILES_DONE;
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
 * @param[in] length packet length
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV4::SendPacket(uint32 destId,
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
DnDRpcV4::HandleMsg(RpcParams *params,
                    const uint8 *binary,
                    uint32 binarySize)
{
   ASSERT(params);

   LOG(4, ("%s: Got %s[%d], sessionId %d, srcId %d, binary size %d.\n",
           __FUNCTION__, DnDCPMsgV4_LookupCmd(params->cmd), params->cmd,
           params->sessionId, params->addrId, binarySize));

   switch (params->cmd) {
   case DND_CMD_SRC_DRAG_BEGIN:
      CPClipboard clip;
      if (!binary || binarySize == 0) {
         LOG(0, ("%s: invalid clipboard data.\n", __FUNCTION__));
         break;
      }
      CPClipboard_Init(&clip);
      if (!CPClipboard_Unserialize(&clip, (void *)binary, binarySize)) {
         LOG(0, ("%s: CPClipboard_Unserialize failed.\n", __FUNCTION__));
         break;
      }
      srcDragBeginChanged.emit(params->sessionId, &clip);
      CPClipboard_Destroy(&clip);
      break;
   case DND_CMD_SRC_CANCEL:
      srcCancelChanged.emit(params->sessionId);
      break;
   case DND_CMD_SRC_DROP:
      srcDropChanged.emit(params->sessionId,
                          params->optional.mouseInfo.x,
                          params->optional.mouseInfo.y);
      break;
   case DND_CMD_DEST_DRAG_ENTER_REPLY:
      destDragEnterReplyChanged.emit(params->sessionId,
                                     params->status);
      break;
   case DND_CMD_DEST_DROP:
      destDropChanged.emit(params->sessionId,
                           params->optional.mouseInfo.x,
                           params->optional.mouseInfo.y);
      break;
   case DND_CMD_DEST_CANCEL:
      destCancelChanged.emit(params->sessionId);
      break;
   case DND_CMD_PRIV_DRAG_ENTER:
      destPrivDragEnterChanged.emit(params->sessionId);
      break;
   case DND_CMD_PRIV_DRAG_LEAVE:
      destPrivDragLeaveChanged.emit(params->sessionId,
                                    params->optional.mouseInfo.x,
                                    params->optional.mouseInfo.y);
      break;
   case DND_CMD_PRIV_DROP:
      destPrivDropChanged.emit(params->sessionId,
                               params->optional.mouseInfo.x,
                               params->optional.mouseInfo.y);
      break;
   case DND_CMD_QUERY_EXITING:
      queryExitingChanged.emit(params->sessionId,
                               params->optional.queryExiting.x,
                               params->optional.queryExiting.y);
      break;
   case DND_CMD_DRAG_NOT_PENDING:
      dragNotPendingChanged.emit(params->sessionId);
      break;
   case DND_CMD_UPDATE_UNITY_DET_WND:
      updateUnityDetWndChanged.emit(params->sessionId,
                                    1 == params->optional.updateUnityDetWnd.show,
                                    params->optional.updateUnityDetWnd.unityWndId);
      break;
   case DND_CMD_MOVE_MOUSE:
      moveMouseChanged.emit(params->sessionId,
                            params->optional.mouseInfo.x,
                            params->optional.mouseInfo.y);
      break;
   case DND_CMD_UPDATE_FEEDBACK:
      updateFeedbackChanged.emit(params->sessionId,
                                 params->optional.feedback.feedback);
      break;
   case DND_CMD_REQUEST_FILES:
      requestFileChanged.emit(params->sessionId, binary, binarySize);
      break;
   case DND_CMD_GET_FILES_DONE:
      getFilesDoneChanged.emit(params->sessionId,
                               DND_CP_MSG_STATUS_SUCCESS == params->status,
                               binary,
                               binarySize);
      break;
   case DNDCP_CMD_PING_REPLY:
      pingReplyChanged.emit(params->optional.version.capability);
      break;
   case DNDCP_CMD_TEST_BIG_BINARY:
   {
      if (binarySize != DND_CP_MSG_MAX_BINARY_SIZE_V4) {
         LOG(0, ("%s: msg size is not right, should be %u.\n",
                 __FUNCTION__, DND_CP_MSG_MAX_BINARY_SIZE_V4));
      }

      uint32 *testBinary = (uint32 *)binary;
      for (uint32 i = 0; i < DND_CP_MSG_MAX_BINARY_SIZE_V4 / sizeof *testBinary; i++) {
         if (testBinary[i] != i) {
            LOG(0, ("%s: msg wrong in position %u. Expect %u, but got %u.\n",
                    __FUNCTION__, i, i, testBinary[i]));
            return;
         }
      }
      LOG(4, ("%s: successfully got big binary, sending back.\n",
              __FUNCTION__));
      RpcParams outParams;
      memset(&outParams, 0, sizeof outParams);
      outParams.addrId = params->addrId;
      outParams.cmd = DNDCP_CMD_TEST_BIG_BINARY_REPLY;
      mUtil.SendMsg(&outParams, binary, DND_CP_MSG_MAX_BINARY_SIZE_V4);

      break;
   }
   case DNDCP_CMP_REPLY:
      LOG(0, ("%s: Got cmp reply command %d.\n", __FUNCTION__, params->cmd));
      cmdReplyChanged.emit(params->cmd, params->status);
      break;
   default:
      LOG(0, ("%s: Got unknown command %d.\n", __FUNCTION__, params->cmd));
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
DnDRpcV4::OnRecvPacket(uint32 srcId,
                       const uint8 *packet,
                       size_t packetSize)
{
   mUtil.OnRecvPacket(srcId, packet, packetSize);
}


/**
 * Register a listener that will fire when RPCs are received.
 *
 * @param[in] obj an instance of a class that derives DnDRpcListener.
 */

void
DnDRpcV4::AddRpcReceivedListener(DnDRpcListener *obj)
{
   mUtil.AddRpcReceivedListener(obj);
}


/**
 * Remove a listener that will fire when RPCs are received.
 *
 * @param[in] obj an instance of a class that derives DnDRpcListener.
 */

void
DnDRpcV4::RemoveRpcReceivedListener(DnDRpcListener *obj)
{
   mUtil.RemoveRpcReceivedListener(obj);
}


/**
 * Add a listener that will fire when RPCs are sent.
 *
 * @param[in] obj an instance of a class that derives DnDRpcListener.
 */

void
DnDRpcV4::AddRpcSentListener(DnDRpcListener *obj)
{
   mUtil.AddRpcSentListener(obj);
}


/**
 * Remove a listener that will fire when RPCs are sent.
 *
 * @param[in] obj an instance of a class that derives DnDRpcListener.
 */

void
DnDRpcV4::RemoveRpcSentListener(DnDRpcListener *obj)
{
   mUtil.RemoveRpcSentListener(obj);
}


/**
 * Set the max transport packet size of RPC messages.
 *
 * @param[in] size the new max packet size.
 */

void
DnDRpcV4::SetMaxTransportPacketSize(const uint32 size)
{
   mUtil.SetMaxTransportPacketSize(size);
}

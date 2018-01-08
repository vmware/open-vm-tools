/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * @DnDRpcV3.cc --
 *
 * Implementation of the DnDRpcV3 class.
 */

#if defined (_WIN32)
/*
 * When compile this file for Windows dnd plugin dll, there may be a conflict
 * between CRT and MFC libraries.  From
 * http://support.microsoft.com/default.aspx?scid=kb;en-us;q148652: The CRT
 * libraries use weak external linkage for the DllMain function. The MFC
 * libraries also contain this function. The function requires the MFC
 * libraries to be linked before the CRT library. The Afx.h include file
 * forces the correct order of the libraries.
 */
#include <afx.h>
#endif

#include "dndRpcV3.hh"
#include "tracer.hh"

extern "C" {
   #include "debug.h"
   #include "dndClipboard.h"
}

#include "util.h"
#include "dndMsg.h"


/**
 * Constructor.
 *
 * @param[in] transport for sending packets.
 */

DnDRpcV3::DnDRpcV3(DnDCPTransport *transport)
   : mTransport(transport),
     mTransportInterface(TRANSPORT_GUEST_CONTROLLER_DND)
{
   ASSERT(mTransport);

   mUtil.Init(this);
   CPClipboard_Init(&mClipboard);
}


/**
 * Destructor.
 */

DnDRpcV3::~DnDRpcV3(void)
{
   CPClipboard_Destroy(&mClipboard);
}


/**
 * Init.
 */

void
DnDRpcV3::Init(void)
{
   TRACE_CALL();
   ASSERT(mTransport);
   mTransport->RegisterRpc(this, mTransportInterface);
}


/**
 * Send DND_HG_DRAG_ENTER_DONE message.
 *
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::SrcDragEnterDone(int32 x,
                           int32 y)
{
   TRACE_CALL();
   return mUtil.SendMsg(DND_HG_DRAG_ENTER_DONE, x, y);

}


/**
 * Send DND_HG_DRAG_READY message.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::SrcDragBeginDone(uint32 sessionId)
{
   TRACE_CALL();
   return mUtil.SendMsg(DND_HG_DRAG_READY);
}


/**
 * Send DND_HG_UPDATE_FEEDBACK message.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[in] feedback current dnd operation feedback.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::UpdateFeedback(uint32 sessionId,
                         DND_DROPEFFECT feedback)
{
   DnDMsg msg;
   bool ret = false;

   DnDMsg_Init(&msg);

   DnDMsg_SetCmd(&msg, DND_HG_UPDATE_FEEDBACK);

   if (!DnDMsg_AppendArg(&msg, &feedback, sizeof feedback)) {
      g_debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mUtil.SendMsg(&msg);

exit:
   DnDMsg_Destroy(&msg);
   return ret;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 *
 * @return always true.
 */

bool
DnDRpcV3::SrcPrivDragEnter(uint32 sessionId)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::SrcPrivDragLeave(uint32 sessionId,
                           int32 x,
                           int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::SrcPrivDrop(uint32 sessionId,
                      int32 x,
                      int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::SrcDrop(uint32 sessionId,
                  int32 x,
                  int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Send DND_HG_DROP_DONE message.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::SrcDropDone(uint32 sessionId,
                      const uint8 *stagingDirCP,
                      uint32 sz)
{
   DnDMsg msg;
   bool ret = false;

   TRACE_CALL();

   DnDMsg_Init(&msg);

   /* Construct msg with both cmd CP_HG_START_FILE_COPY and stagingDirCP. */
   DnDMsg_SetCmd(&msg, DND_HG_DROP_DONE);
   if (!DnDMsg_AppendArg(&msg, (void *)stagingDirCP, sz)) {
      g_debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mUtil.SendMsg(&msg);

exit:
   DnDMsg_Destroy(&msg);
   return ret;
}


/**
 * Send DND_GH_DRAG_ENTER message.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[in] clip cross-platform clipboard data.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::DestDragEnter(uint32 sessionId,
                        const CPClipboard *clip)
{
   TRACE_CALL();
   return mUtil.SendMsg(DND_GH_DRAG_ENTER, clip);
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] clip cross-platform clipboard data.
 *
 * @return always true.
 */

bool
DnDRpcV3::DestSendClip(uint32 sessionId,
                       const CPClipboard *clip)
{
   TRACE_CALL();
   return true;
}


/**
 * Send DND_GH_NOT_PENDING message.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 *
 * @return true on success, false otherwise.
 */

bool
DnDRpcV3::DragNotPending(uint32 sessionId)
{
   TRACE_CALL();
   return mUtil.SendMsg(DND_GH_NOT_PENDING);
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::DestDragLeave(uint32 sessionId,
                        int32 x,
                        int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::DestDrop(uint32 sessionId,
                   int32 x,
                   int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::QueryExiting(uint32 sessionId,
                       int32 x,
                       int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId  Active session id the controller assigned earlier.
 * @param[ignored] show       Show or hide unity DnD detection window.
 * @param[ignored] unityWndId The unity windows id.
 *
 * @return always true.
 */

bool
DnDRpcV3::UpdateUnityDetWnd(uint32 sessionId,
                            bool show,
                            uint32 unityWndId)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] x mouse position x.
 * @param[ignored] y mouse position y.
 *
 * @return always true.
 */

bool
DnDRpcV3::MoveMouse(uint32 sessionId,
                    int32 x,
                    int32 y)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 *
 * @return always true.
 */

bool
DnDRpcV3::RequestFiles(uint32 sessionId)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] success the file transfer operation was successful or not
 * @param[ignored] stagingDirCP staging dir name in cross-platform format
 * @param[ignored] sz the staging dir name size in bytes
 *
 * @return always true.
 */

bool
DnDRpcV3::SendFilesDone(uint32 sessionId,
                        bool success,
                        const uint8 *stagingDirCP,
                        uint32 sz)
{
   TRACE_CALL();
   return true;
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] success the file transfer operation was successful or not
 *
 * @return always true.
 */

bool
DnDRpcV3::GetFilesDone(uint32 sessionId,
                       bool success)
{
   TRACE_CALL();
   return true;
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
DnDRpcV3::SendPacket(uint32 destId,
                     const uint8 *packet,
                     size_t length)
{
   TRACE_CALL();
   return mTransport->SendPacket(destId, mTransportInterface, packet, length);
}


/**
 * Handle a received version 3 message.
 *
 * @param[in] params parameter list for received message.
 * @param[in] binary attached binary data for received message.
 * @param[in] binarySize
 */

void
DnDRpcV3::HandleMsg(RpcParams *params,
                    const uint8 *binary,
                    uint32 binarySize)
{
   DnDMsg msg;
   DnDMsgErr ret;
   DynBuf *buf = NULL;

   DnDMsg_Init(&msg);

   ret = DnDMsg_UnserializeHeader(&msg, (void *)binary, binarySize);
   if (DNDMSG_SUCCESS != ret) {
      g_debug("%s: DnDMsg_UnserializeHeader failed %d\n", __FUNCTION__, ret);
      goto exit;
   }

   ret = DnDMsg_UnserializeArgs(&msg,
                                (void *)(binary + DNDMSG_HEADERSIZE_V3),
                                binarySize - DNDMSG_HEADERSIZE_V3);
   if (DNDMSG_SUCCESS != ret) {
      g_debug("%s: DnDMsg_UnserializeArgs failed with %d\n", __FUNCTION__, ret);
      goto exit;
   }

   g_debug("%s: Got %d, binary size %d.\n", __FUNCTION__, DnDMsg_GetCmd(&msg),
           binarySize);

   /*
    * Translate command and emit signal. Session Id 1 is used because version
    * 3 command does not provide session Id.
    */
   switch (DnDMsg_GetCmd(&msg)) {
   case DND_HG_DRAG_ENTER:
   {
      CPClipboard_Clear(&mClipboard);

      /* Unserialize clipboard data for the command. */
      buf = DnDMsg_GetArg(&msg, 0);
      if (!CPClipboard_Unserialize(&mClipboard, DynBuf_Get(buf), DynBuf_GetSize(buf))) {
         g_debug("%s: CPClipboard_Unserialize failed.\n", __FUNCTION__);
         break;
      }
      SrcDragEnterDone(DRAG_DET_WINDOW_WIDTH / 2,
                       DRAG_DET_WINDOW_WIDTH / 2);
      break;
   }
   case DND_HG_DRAG_START:
      srcDragBeginChanged.emit(1, &mClipboard);
      CPClipboard_Clear(&mClipboard);
      break;
   case DND_HG_CANCEL:
      srcCancelChanged.emit(1);
      break;
   case DND_HG_DROP:
      srcDropChanged.emit(1, 0, 0);
      break;
   case DND_GH_CANCEL:
      destCancelChanged.emit(1);
      break;
   case DND_GH_PRIVATE_DROP:
   {
      int32 x = 0;
      int32 y = 0;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(int32)) {
         memcpy(&x, (const char *)DynBuf_Get(buf), sizeof(int32));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) == sizeof(int32)) {
         memcpy(&y, (const char *)DynBuf_Get(buf), sizeof(int32));
      } else {
         break;
      }

      destPrivDropChanged.emit(1, x, y);
      break;
   }
   case DND_GH_UPDATE_UNITY_DET_WND:
   {
      bool show = false;
      uint32 unityWndId;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(show)) {
         memcpy(&show, (const char *)DynBuf_Get(buf), sizeof(show));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) == sizeof(unityWndId)) {
         memcpy(&unityWndId, (const char *)DynBuf_Get(buf), sizeof(unityWndId));
      } else {
         break;
      }
      updateUnityDetWndChanged.emit(1, show, unityWndId);

      break;
   }
   case DND_GH_QUERY_PENDING_DRAG:
   {
      int32 x = 0;
      int32 y = 0;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(int32)) {
         memcpy(&x, (const char *)DynBuf_Get(buf), sizeof(int32));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) == sizeof(int32)) {
         memcpy(&y, (const char *)DynBuf_Get(buf), sizeof(int32));
      } else {
         break;
      }
      queryExitingChanged.emit(1, x, y);
      break;
   }
   case DND_UPDATE_MOUSE:
   {
      int32 x = 0;
      int32 y = 0;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(x)) {
         memcpy(&x, (const char *)DynBuf_Get(buf), sizeof(x));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) == sizeof(y)) {
         memcpy(&y, (const char *)DynBuf_Get(buf), sizeof(y));
      } else {
         break;
      }

      moveMouseChanged.emit(1, x, y);
      break;
   }
   case DND_HG_FILE_COPY_DONE:
   {
      bool success;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(success)) {
         memcpy(&success, (const char *)DynBuf_Get(buf), sizeof(success));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      getFilesDoneChanged.emit(1, success, (const uint8 *)DynBuf_Get(buf), DynBuf_GetSize(buf));
      break;
   }
   default:
      g_debug("%s: got unsupported new command %d.\n", __FUNCTION__,
              DnDMsg_GetCmd(&msg));
   }
exit:
   DnDMsg_Destroy(&msg);
}


/**
 * Callback from transport layer after received a packet from srcId.
 *
 * @param[in] srcId addressId where the packet is from
 * @param[in] packet
 * @param[in] packetSize
 */

void
DnDRpcV3::OnRecvPacket(uint32 srcId,
                       const uint8 *packet,
                       size_t packetSize)
{
   TRACE_CALL();
   mUtil.OnRecvPacket(srcId, packet, packetSize);
}

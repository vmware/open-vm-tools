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

/*
 * CopyPasteRpcV3.cc --
 *
 *     Implementation of the CopyPasteRpcV3 interface.
 */


#include "copyPasteRpcV3.hh"
#include "tracer.hh"
#include "dndMsg.h"

extern "C" {
   #include "debug.h"
   #include "dndClipboard.h"
}

/**
 * Constructor.
 *
 * @param[in] transport for sending packets.
 */

CopyPasteRpcV3::CopyPasteRpcV3(DnDCPTransport *transport)
   : mTransport(transport),
     mTransportInterface(TRANSPORT_GUEST_CONTROLLER_CP)
{
   ASSERT(mTransport);

   mUtil.Init(this);
}


/**
 * Destructor.
 */

CopyPasteRpcV3::~CopyPasteRpcV3(void)
{
}


/**
 * Init.
 */

void
CopyPasteRpcV3::Init(void)
{
   TRACE_CALL();
   ASSERT(mTransport);
   mTransport->RegisterRpc(this, mTransportInterface);
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] caps capabilities mask
 */

void
CopyPasteRpcV3::SendPing(uint32 caps)
{
   TRACE_CALL();
}


/**
 * Not needed for version 3.
 *
 * @param[ignored] sessionId active session id the controller assigned
 * @param[ignored] isActive active or passive CopyPaste
 *
 * @return always true.
 */

bool
CopyPasteRpcV3::SrcRequestClip(uint32 sessionId,
                               bool isActive)
{
   TRACE_CALL();
   return true;
}


/**
 * Send cmd CP_GH_GET_CLIPBOARD_DONE to controller.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[ignored] isActive active or passive CopyPaste
 * @param[in] clip cross-platform clipboard data.
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV3::DestSendClip(uint32 sessionId,
                             bool isActive,
                             const CPClipboard* clip)
{
   TRACE_CALL();
   return mUtil.SendMsg(CP_GH_GET_CLIPBOARD_DONE, clip);
}


/**
 * Send cmd CP_HG_START_FILE_COPY to controller.
 *
 * @param[ignored] sessionId active session id the controller assigned earlier.
 * @param[in] stagingDirCP staging dir name in cross-platform format
 * @param[in] sz the staging dir name size in bytes
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV3::RequestFiles(uint32 sessionId,
                             const uint8 *stagingDirCP,
                             uint32 sz)
{
   DnDMsg msg;
   bool ret = false;

   TRACE_CALL();

   DnDMsg_Init(&msg);

   /* Construct msg with both cmd CP_HG_START_FILE_COPY and stagingDirCP. */
   DnDMsg_SetCmd(&msg, CP_HG_START_FILE_COPY);
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
CopyPasteRpcV3::SendFilesDone(uint32 sessionId,
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
CopyPasteRpcV3::GetFilesDone(uint32 sessionId,
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
 * @param[in] length packet length in byte
 *
 * @return true on success, false otherwise.
 */

bool
CopyPasteRpcV3::SendPacket(uint32 destId,
                           const uint8 *packet,
                           size_t length)
{
   TRACE_CALL();
   return mTransport->SendPacket(destId,
                                 mTransportInterface,
                                 packet,
                                 length);
}


/**
 * Handle a received message.
 *
 * @param[in] params parameter list for received message.
 * @param[in] binary attached binary data for received message.
 * @param[in] binarySize in byte
 */

void
CopyPasteRpcV3::HandleMsg(RpcParams *params,
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
   case CP_HG_SET_CLIPBOARD:
   {
      CPClipboard clip;

      /* Unserialize clipboard data for the command. */
      buf = DnDMsg_GetArg(&msg, 0);
      CPClipboard_Init(&clip);
      if (!CPClipboard_Unserialize(&clip, DynBuf_Get(buf), DynBuf_GetSize(buf))) {
         g_debug("%s: CPClipboard_Unserialize failed.\n", __FUNCTION__);
         goto exit;
      }
      srcRecvClipChanged.emit(1, false, &clip);
      CPClipboard_Destroy(&clip);
      break;
   }
   case CP_HG_FILE_COPY_DONE:
   {
      bool success = false;
      buf = DnDMsg_GetArg(&msg, 0);
      if (sizeof success == DynBuf_GetSize(buf)) {
         memcpy(&success, DynBuf_Get(buf), DynBuf_GetSize(buf));
      }
      getFilesDoneChanged.emit(1, success, NULL, 0);
      break;
   }
   case CP_GH_GET_CLIPBOARD:
   {
      destRequestClipChanged.emit(1, false);
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
CopyPasteRpcV3::OnRecvPacket(uint32 srcId,
                             const uint8 *packet,
                             size_t packetSize)
{
   TRACE_CALL();
   mUtil.OnRecvPacket(srcId, packet, packetSize);
}

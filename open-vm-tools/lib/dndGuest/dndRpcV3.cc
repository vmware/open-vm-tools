/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * DnDRpcV3.cc --
 *
 *     Implementation of the DnDRpcV3 interface.
 */


#include "dndRpcV3.hh"
#include "dndTransportGuestRpc.hh"

extern "C" {
   #include "dndMsg.h"
   #include "debug.h"
   #include "dndClipboard.h"
}

/*
 *-----------------------------------------------------------------------------
 *
 * DnDRpcV3 --
 *
 *      Constructor of DnDRpcV3 class.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

DnDRpcV3::DnDRpcV3(struct RpcIn *rpcIn) // IN
{
   mTransport = new DnDTransportGuestRpc(rpcIn, "dnd.transport");
   mTransport->recvMsgChanged.connect(sigc::mem_fun(this, &DnDRpcV3::OnRecvMsg));
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::~DnDRpcV3 --
 *
 *      Destructor of DnDRpcV3.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

DnDRpcV3::~DnDRpcV3(void)
{
   delete mTransport;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::HGDragEnterDone --
 *
 *      Serialize DND_HG_DRAG_ENTER_DONE message and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::HGDragEnterDone(int32 x, // IN
                          int32 y) // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = false;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   DnDMsg_SetCmd(&msg, DND_HG_DRAG_ENTER_DONE);

   if (!DnDMsg_AppendArg(&msg, &x, sizeof x) ||
       !DnDMsg_AppendArg(&msg, &y, sizeof y)) {
      Debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   if (!DnDMsg_Serialize(&msg, &buf)) {
      Debug("%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mTransport->SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::HGDragStartDone --
 *
 *      Serialize DND_HG_DRAG_READY message and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::HGDragStartDone(void)
{
   return SendSingleCmd(DND_HG_DRAG_READY);
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::HGUpdateFeedback --
 *
 *      Serialize DND_HG_UPDATE_FEEDBACK message and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::HGUpdateFeedback(DND_DROPEFFECT effect) // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = false;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   DnDMsg_SetCmd(&msg, DND_HG_UPDATE_FEEDBACK);

   if (!DnDMsg_AppendArg(&msg, &effect, sizeof effect)) {
      Debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   if (!DnDMsg_Serialize(&msg, &buf)) {
      Debug("%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mTransport->SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::HGDropDone --
 *
 *      Serialize DND_HG_DROP_DONE message and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::HGDropDone(const char *stagingDirCP, // IN
                     size_t sz)                // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = FALSE;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   /* Construct msg with both cmd CP_HG_START_FILE_COPY and stagingDirCP. */
   DnDMsg_SetCmd(&msg, DND_HG_DROP_DONE);
   if (!DnDMsg_AppendArg(&msg, (void *)stagingDirCP, sz)) {
      Debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   /* Serialize msg and output to buf. */
   if (!DnDMsg_Serialize(&msg, &buf)) {
      Debug("%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mTransport->SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::GHDragEnter --
 *
 *      Serialize all parameters and pass to transport layer for
 *      DND_GH_DRAG_ENTER signal.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::GHDragEnter(const CPClipboard *clip) // IN
{
   return SendCmdWithClip(DND_GH_DRAG_ENTER, clip);
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::GHUngrabTimeout --
 *
 *      Serialize DND_GH_NOT_PENDING message and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::GHUngrabTimeout(void)
{
   return SendSingleCmd(DND_GH_NOT_PENDING);
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::SendSingleCmd --
 *
 *      Serialize message without any argument and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::SendSingleCmd(DnDCommand cmd) // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = false;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   DnDMsg_SetCmd(&msg, cmd);

   if (!DnDMsg_Serialize(&msg, &buf)) {
      Debug("%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mTransport->SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * cui::dnd::DnDRpcV3::SendCmdWithClip --
 *
 *      Serialize message with only clip data and forward to
 *      transport layer.
 *
 * Results:
 *      true if success, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
DnDRpcV3::SendCmdWithClip(DnDCommand cmd,          // IN
                          const CPClipboard *clip) // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = false;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   /* Serialize clip and output into buf. */
   if (!CPClipboard_Serialize(clip, &buf)) {
      Debug("%s: CPClipboard_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   /* Construct msg with both cmd CP_HG_SET_CLIPBOARD and buf. */
   DnDMsg_SetCmd(&msg, cmd);
   if (!DnDMsg_AppendArg(&msg, DynBuf_Get(&buf), DynBuf_GetSize(&buf))) {
      Debug("%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   /* Reset buf. */
   DynBuf_Destroy(&buf);
   DynBuf_Init(&buf);

   /* Serialize msg and output to buf. */
   if (!DnDMsg_Serialize(&msg, &buf)) {
      Debug("%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = mTransport->SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDRpcV3::OnRecvMsg --
 *
 *      Received a new message from transport layer. Unserialize and
 *      translate it and emit corresponding signal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
DnDRpcV3::OnRecvMsg(const uint8 *data, // IN
                    size_t dataSize)   // IN
{
   DnDMsg msg;
   DnDMsgErr ret;
   DynBuf *buf = NULL;

   DnDMsg_Init(&msg);

   ret = DnDMsg_UnserializeHeader(&msg, (void *)data, dataSize);
   if (DNDMSG_SUCCESS != ret) {
      Debug("%s: DnDMsg_UnserializeHeader failed with %d\n",
            __FUNCTION__, ret);
      goto exit;
   }

   ret = DnDMsg_UnserializeArgs(&msg,
                                (void *)&data[DNDMSG_HEADERSIZE_V3],
                                dataSize - DNDMSG_HEADERSIZE_V3);
   if (DNDMSG_SUCCESS != ret) {
      Debug("%s: DnDMsg_UnserializeArgs failed with %d\n",
            __FUNCTION__, ret);
      goto exit;
   }

   /* Translate command and emit signal. */
   switch (DnDMsg_GetCmd(&msg)) {
   case DND_GH_UPDATE_UNITY_DET_WND:
   {
      bool bShow = false;
      uint32 unityWndId;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(bool)) {
         memcpy(&bShow, (const char *)DynBuf_Get(buf), sizeof(bool));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) == sizeof(uint32)) {
         memcpy(&unityWndId, (const char *)DynBuf_Get(buf), sizeof(uint32));
      } else {
         break;
      }
      ghUpdateUnityDetWndChanged.emit(bShow, unityWndId);

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
      ghQueryPendingDragChanged.emit(x, y);
      break;
   }
   case DND_GH_CANCEL:
      ghCancelChanged.emit();
      break;
   case DND_HG_DRAG_ENTER:
   {
      CPClipboard clip;

      /* Unserialize clipboard data for the command. */
      buf = DnDMsg_GetArg(&msg, 0);
      if (!CPClipboard_Unserialize(&clip, DynBuf_Get(buf), DynBuf_GetSize(buf))) {
         Debug("%s: CPClipboard_Unserialize failed.\n", __FUNCTION__);
         break;
      }
      hgDragEnterChanged.emit(&clip);
      break;
   }
   case DND_HG_DRAG_START:
   {
      hgDragStartChanged.emit();
      break;
   }
   case DND_HG_DROP:
      hgDropChanged.emit();
      break;
   case DND_HG_CANCEL:
      hgCancelChanged.emit();
      break;
   case DND_HG_FILE_COPY_DONE:
   {
      bool success;
      std::vector<uint8> stagingDir;

      buf = DnDMsg_GetArg(&msg, 0);
      if (DynBuf_GetSize(buf) == sizeof(bool)) {
         memcpy(&success, (const char *)DynBuf_Get(buf), sizeof(bool));
      } else {
         break;
      }

      buf = DnDMsg_GetArg(&msg, 1);
      if (DynBuf_GetSize(buf) > 0) {
         stagingDir.resize(DynBuf_GetSize(buf));
         memcpy(&stagingDir[0],
                (const char *)DynBuf_Get(buf),
                stagingDir.size());
      }
      hgFileCopyDoneChanged.emit(success, stagingDir);
      break;
   }
   default:
      Debug("%s: got unsupported new command %d.\n",
            __FUNCTION__, DnDMsg_GetCmd(&msg));
   }
exit:
   DnDMsg_Destroy(&msg);
}

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
 * CopyPasteRpcV3.cc --
 *
 *     Implementation of the CopyPasteRpcV3 interface.
 */


#include "copyPasteRpcV3.hh"
#include "dndTransportGuestRpc.hh"

extern "C" {
   #include "dndMsg.h"
   #include "debug.h"
   #include "dndClipboard.h"
}

/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteRpcV3 --
 *
 *      Constructor of CopyPasteRpcV3 class.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

CopyPasteRpcV3::CopyPasteRpcV3(struct RpcIn *rpcIn) // IN
{
   mTransport = new DnDTransportGuestRpc(rpcIn, "copypaste.transport");
   mTransport->recvMsgChanged.connect(sigc::mem_fun(this, &CopyPasteRpcV3::OnRecvMsg));
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteRpcV3::~CopyPasteRpcV3 --
 *
 *      Destructor of CopyPasteRpcV3.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

CopyPasteRpcV3::~CopyPasteRpcV3(void)
{
   delete mTransport;
}


/*
 *----------------------------------------------------------------------
 *
 * CopyPasteRpcV3::GHGetClipboardDone --
 *
 *      Serialize all parameters and pass to transport layer for
 *      CP_GH_GET_CLIPBOARD_DONE signal.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
CopyPasteRpcV3::GHGetClipboardDone(const CPClipboard* clip) // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = FALSE;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   /* Serialize clip and output into buf. */
   if (!CPClipboard_Serialize(clip, &buf)) {
      Debug("%s: CPClipboard_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   /* Construct msg with both cmd CP_GH_GET_CLIPBOARD_DONE and buf. */
   DnDMsg_SetCmd(&msg, CP_GH_GET_CLIPBOARD_DONE);
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
 * CopyPasteRpcV3::HGStartFileCopy --
 *
 *      Serialize all parameters and pass to transport layer for
 *      CP_HG_START_FILE_COPY signal.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
CopyPasteRpcV3::HGStartFileCopy(const char *stagingDirCP, // IN
                                size_t sz)                // IN
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = FALSE;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   /* Construct msg with both cmd CP_HG_START_FILE_COPY and stagingDirCP. */
   DnDMsg_SetCmd(&msg, CP_HG_START_FILE_COPY);
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
 * CopyPasteRpcV3::OnRecvMsg --
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
CopyPasteRpcV3::OnRecvMsg(const uint8 *data, // IN
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
   case CP_GH_GET_CLIPBOARD:
      ghGetClipboardChanged.emit();
      break;
   case CP_HG_SET_CLIPBOARD:
   {
      CPClipboard clip;

      /* Unserialize clipboard data for the command. */
      buf = DnDMsg_GetArg(&msg, 0);
      if (!CPClipboard_Unserialize(&clip, DynBuf_Get(buf), DynBuf_GetSize(buf))) {
         Debug("%s: CPClipboard_Unserialize failed.\n", __FUNCTION__);
         goto exit;
      }
      hgSetClipboardChanged.emit(&clip);
      break;
   }
   case CP_HG_FILE_COPY_DONE:
   {
      bool success = false;
      buf = DnDMsg_GetArg(&msg, 0);
      if (sizeof success == DynBuf_GetSize(buf)) {
         memcpy(&success, DynBuf_Get(buf), DynBuf_GetSize(buf));
      }
      hgFileCopyDoneChanged.emit(success);
      break;
   }
   default:
      Debug("%s: got unsupported new command %d.\n",
            __FUNCTION__, DnDMsg_GetCmd(&msg));
   }
exit:
   DnDMsg_Destroy(&msg);
}

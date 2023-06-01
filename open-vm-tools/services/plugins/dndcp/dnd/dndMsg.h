/*********************************************************
 * Copyright (c) 2007-2017, 2023 VMware, Inc. All rights reserved.
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
 * dndMsg.h --
 *
 *      DnDMsg represents an rpc message which is sent across the
 *      wire. Any args that it holds will be written out exactly as stored.
 */

#ifndef _DNDMSG_H_
#define _DNDMSG_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#include "dynbuf.h"
#include "dynarray.h"
#include "dnd.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Various return types serialization/unserialization functions can return. */

typedef enum {
   DNDMSG_SUCCESS = 0,
   DNDMSG_ERR,
   DNDMSG_NOMEM,
   DNDMSG_INPUT_TOO_SMALL, /* Input buffer needs to be bigger. */
   DNDMSG_INPUT_ERR, /* Serialize/unserialized failed confidence checks. */
} DnDMsgErr;

/*
 * DnD Commands.
 */

typedef enum {
   DND_INVALID = 0,

   /*
    * We need to send mouse packets for old protocols because old guest tools
    * manipulate the mouse directly, but in DnD Version 3+, the host controls the
    * mouse pointer via foundry.
    */
   /* DnD Ver 1/2 Commands */
   DND_HG_SEND_MOUSE_PACKET,

   /* DnD version 3 commands */
   // GHDnD (h->g)
   DND_GH_QUERY_PENDING_DRAG,
   DND_GH_CANCEL,
   DND_GH_COPY_DONE,
   // GHDnD (g->h)
   DND_GH_DRAG_ENTER,
   DND_GH_NOT_PENDING,

   // HGDnD (h->g)
   DND_HG_DRAG_ENTER,
   DND_HG_DRAG_START,
   DND_HG_CANCEL,
   DND_HG_DROP,
   DND_HG_FILE_COPY_DONE,
   // HGDnD (g->h)
   DND_HG_DRAG_ENTER_DONE,
   DND_HG_DRAG_READY,
   DND_HG_UPDATE_FEEDBACK,
   DND_HG_DROP_DONE,
   DND_HG_START_FILE_COPY,

   // Add future commands here.
   DND_GH_UPDATE_UNITY_DET_WND,

   // New command after DnD version 3.1
   DND_UPDATE_HOST_VERSION,
   DND_UPDATE_GUEST_VERSION,
   DND_UPDATE_MOUSE,
   DND_GH_PRIVATE_DROP,
   DND_GH_TRANSPORT_TEST,
   DND_MOVE_DET_WND_TO_MOUSE_POS,
   DND_GH_SET_CLIPBOARD,
   DND_GH_GET_NEXT_NAME,
   DND_HG_SET_GUEST_FILE_ROOT,
   DND_MAX,
} DnDCommand;

/*
 * Copy/Paste commands.
 */

typedef enum {
   CP_INVALID = 0,

   /* DnD version 3 commands. */
   // GHCopyPaste (h->g)
   CP_GH_GET_CLIPBOARD,
   // GHCopyPaste (g->h)
   CP_GH_GET_CLIPBOARD_DONE, // FORMAT DATA(property list?)

   // HGCopyPaste (h->g)
   CP_HG_SET_CLIPBOARD,
   CP_HG_FILE_COPY_DONE,
   // HGCopyPaste(g->h)
   CP_HG_START_FILE_COPY,

   // Add future commands here.
   CP_GH_TRANSPORT_TEST,
   CP_MAX,
} CopyPasteCommand;

/*
 * Opaque data structure.
 * Members are listed in order of unserialization.
 */

typedef struct DnDMsg {
   /* Header */
   uint8 ver; // Must be first across all versions.
   uint32 cmd;
   uint32 nargs;
   /* The expected size of the buffer needed to unserialize the arguments. */
   uint32 expectedArgsSz;

   /* Body */
   DynBufArray args;
} DnDMsg;

#define DnDMsg_Success(x) ((x) == DNDMSG_SUCCESS)

void DnDMsg_Init(DnDMsg *msg);
void DnDMsg_Destroy(DnDMsg *msg);

/* Header information. */
uint32 DnDMsg_GetCmd(DnDMsg *msg);
void DnDMsg_SetCmd(DnDMsg *msg, uint32 cmd);
uint32 DnDMsg_NumArgs(DnDMsg *msg);

DynBuf *DnDMsg_GetArg(DnDMsg *msg, uint32 arg);

Bool DnDMsg_AppendArg(DnDMsg *msg, void *buf, size_t len);
Bool DnDMsg_Serialize(DnDMsg *msg, DynBuf *buf);

DnDMsgErr DnDMsg_UnserializeHeader(DnDMsg *msg, void *buf, size_t len);
DnDMsgErr DnDMsg_UnserializeArgs(DnDMsg *msg, void *buf, size_t len);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _DNDMSG_H_ */

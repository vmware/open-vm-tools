/*********************************************************
 * Copyright (C) 2010-2017 VMware, Inc. All rights reserved.
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
 * @file dndRpcV3.hh --
 *
 * Rpc layer object for DnD version 3.
 */

#ifndef DND_RPC_V3_HH
#define DND_RPC_V3_HH

#include <sigc++/trackable.h>
#include "dndRpc.hh"
#include "dndCPTransport.h"
#include "rpcV3Util.hpp"

#include "dnd.h"
#include "dndMsg.h"

extern "C" {
   #include "vmware/tools/guestrpc.h"
}

class LIB_EXPORT DnDRpcV3
   : public DnDRpc,
     public sigc::trackable
{
public:
   DnDRpcV3(DnDCPTransport *transport);
   virtual ~DnDRpcV3(void);

   virtual void Init(void);
   virtual void SendPing(uint32 caps) {};

   /* DnD source. */
   virtual bool SrcDragBeginDone(uint32 sessionId);
   virtual bool SrcDrop(uint32 sessionId, int32 x, int32 y);
   virtual bool SrcDropDone(uint32 sessionId, const uint8 *stagingDirCP, uint32 sz);

   virtual bool SrcPrivDragEnter(uint32 sessionId);
   virtual bool SrcPrivDragLeave(uint32 sessionId, int32 x, int32 y);
   virtual bool SrcPrivDrop(uint32 sessionId, int32 x, int32 y);
   virtual bool SrcCancel(uint32 sessionId) { return true; };

   /* DnD destination. */
   virtual bool DestDragEnter(uint32 sessionId,
                              const CPClipboard *clip);
   virtual bool DestSendClip(uint32 sessionId,
                             const CPClipboard *clip);
   virtual bool DestDragLeave(uint32 sessionId,
                              int32 x,
                              int32 y);
   virtual bool DestDrop(uint32 sessionId,
                         int32 x,
                         int32 y);
   virtual bool DestCancel(uint32 sessionId) { return true; };

   /* Common. */
   virtual bool UpdateFeedback(uint32 sessionId, DND_DROPEFFECT feedback);
   virtual bool MoveMouse(uint32 sessionId,
                          int32 x,
                          int32 y);
   virtual bool QueryExiting(uint32 sessionId, int32 x, int32 y);
   virtual bool DragNotPending(uint32 sessionId);
   virtual bool UpdateUnityDetWnd(uint32 sessionId,
                                  bool show,
                                  uint32 unityWndId);
   virtual bool RequestFiles(uint32 sessionId);
   virtual bool SendFilesDone(uint32 sessionId,
                              bool success,
                              const uint8 *stagingDirCP,
                              uint32 sz);
   virtual bool GetFilesDone(uint32 sessionId,
                             bool success);

   virtual void HandleMsg(RpcParams *params,
                          const uint8 *binary,
                          uint32 binarySize);
   virtual bool SendPacket(uint32 destId,
                           const uint8 *packet,
                           size_t length);
   virtual void OnRecvPacket(uint32 srcId,
                             const uint8 *packet,
                             size_t packetSize);

private:
   bool SrcDragEnterDone(int32 x, int32 y);
   DnDCPTransport *mTransport;
   TransportInterfaceType mTransportInterface;
   CPClipboard mClipboard;
   RpcV3Util mUtil;
};

#endif // DND_RPC_V3_HH

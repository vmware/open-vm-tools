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
 * @file copyPasteRpcV3.hh --
 *
 * Rpc layer object for CopyPaste version 3.
 */

#ifndef COPY_PASTE_RPC_V3_HH
#define COPY_PASTE_RPC_V3_HH

#include <sigc++/trackable.h>
#include "copyPasteRpc.hh"
#include "dndCPTransport.h"
#include "rpcV3Util.hpp"

extern "C" {
   #include "vmware/tools/guestrpc.h"
}

#include "dnd.h"
#include "dndMsg.h"

class CopyPasteRpcV3
   : public CopyPasteRpc,
     public sigc::trackable
{
public:
   CopyPasteRpcV3(DnDCPTransport *transport);
   virtual ~CopyPasteRpcV3(void);

   virtual void Init(void);
   virtual void SendPing(uint32 caps);

   /* CopyPaste Rpc functions. */
   virtual bool SrcRequestClip(uint32 sessionId,
                               bool isActive);
   virtual bool DestSendClip(uint32 sessionId,
                             bool isActive,
                             const CPClipboard* clip);
   virtual bool RequestFiles(uint32 sessionId,
                             const uint8 *stagingDirCP,
                             uint32 sz);
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
   DnDCPTransport *mTransport;
   TransportInterfaceType mTransportInterface;
   RpcV3Util mUtil;
};

#endif // COPY_PASTE_RPC_V3_HH

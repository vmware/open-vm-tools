/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @fileTransferRpcV4.hh --
 *
 * File transfer rpc version 4 object for DnD/CopyPaste.
 */

#ifndef FILE_TRANSFER_RPC_V4_HH
#define FILE_TRANSFER_RPC_V4_HH

#include "fileTransferRpc.hh"
#include "dndCPTransport.h"
#include "rpcV4Util.hpp"

extern "C" {
   #include "dndCPMsgV4.h"
}

class LIB_EXPORT FileTransferRpcV4
   : public FileTransferRpc,
     public sigc::trackable
{
public:
   FileTransferRpcV4(DnDCPTransport *transport);

   virtual void Init(void);

   virtual bool SendHgfsPacket(uint32 sessionId,
                               const uint8 *packet,
                               uint32 packetSize);
   virtual bool SendHgfsReply(uint32 sessionId,
                              const uint8 *packet,
                              uint32 packetSize);
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
   RpcV4Util mUtil;
};

#endif // FILE_TRANSFER_RPC_V4_HH

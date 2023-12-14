/*********************************************************
 * Copyright (C) 2010-2017,2022 VMware, Inc. All rights reserved.
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
 * @fileTransferRpc.hh --
 *
 * File transfer roc object for DnD/CopyPaste.
 */

#ifndef FILE_TRANSFER_RPC_HH
#define FILE_TRANSFER_RPC_HH

#include <sigc++/sigc++.h>
#include <sigc++2to3.h>
#include "dndCPLibExport.hh"
#include "rpcBase.h"

#include "vm_basic_types.h"

class LIB_EXPORT FileTransferRpc
   : public RpcBase
{
public:
   virtual ~FileTransferRpc(void) {};

   sigc::signal<void, uint32, const uint8 *, size_t> HgfsPacketReceived;
   sigc::signal<void, uint32, const uint8 *, size_t> HgfsReplyReceived;

   virtual void Init(void) = 0;
   virtual bool SendHgfsPacket(uint32 sessionId,
                               const uint8 *packet,
                               uint32 packetSize) = 0;
   virtual bool SendHgfsReply(uint32 sessionId,
                              const uint8 *packet,
                              uint32 packetSize) = 0;
};

#endif // FILE_TRANSFER_RPC_HH

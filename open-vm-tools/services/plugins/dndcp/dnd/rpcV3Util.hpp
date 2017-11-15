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
 * @rpcV3Util.hpp --
 *
 * Rpc layer object for DnD version 4.
 */

#ifndef RPC_V3_UTIL_HPP
#define RPC_V3_UTIL_HPP

#ifndef LIB_EXPORT
#define LIB_EXPORT
#endif

#include "rpcBase.h"
#include "dnd.h"

struct DnDMsg;

class LIB_EXPORT RpcV3Util
{
public:
   RpcV3Util(void);
   virtual ~RpcV3Util(void);

   void Init(RpcBase *rpc);

   void OnRecvPacket(uint32 srcId,
                     const uint8 *packet,
                     size_t packetSize);
   bool SendMsg(uint32 cmd);
   bool SendMsg(uint32 cmd,
                const CPClipboard *clip);
   bool SendMsg(uint32 cmd, int32 x, int32 y); // For cmd with mouse info.
   bool SendMsg(const DnDMsg *msg);
   uint32 GetVersionMajor(void) { return mVersionMajor; }
   uint32 GetVersionMinor(void) { return mVersionMinor; }

private:
   bool SendMsg(const uint8 *binary,
                uint32 binarySize);
   RpcBase *mRpc;
   uint32 mVersionMajor;
   uint32 mVersionMinor;
   DnDTransportBuffer mSendBuf;
   DnDTransportBuffer mRecvBuf;
   uint32 mSeqNum;
};

#endif // RPC_V3_UTIL_HPP

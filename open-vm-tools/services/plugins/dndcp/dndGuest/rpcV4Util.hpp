/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * @rpcV4Util.hpp --
 *
 * Rpc layer object for DnD version 4.
 */

#ifndef RPC_V4_UTIL_HPP
#define RPC_V4_UTIL_HPP

#ifndef LIB_EXPORT
#define LIB_EXPORT
#endif

#include "rpcBase.h"
#include "dndRpcListener.hpp"
#include "dbllnklst.h"
#include "dnd.h"

extern "C" {
   #include "dndCPMsgV4.h"
}


typedef struct DnDRpcReceivedListenerNode
{
   DblLnkLst_Links l;
   const DnDRpcListener *listener;
} DnDRpcReceivedListenerNode;


typedef struct DnDRpcSentListenerNode
{
   DblLnkLst_Links l;
   const DnDRpcListener *listener;
} DnDRpcSentListenerNode;


class LIB_EXPORT RpcV4Util
{
public:
   RpcV4Util(void);
   virtual ~RpcV4Util(void);

   void Init(RpcBase *rpc, uint32 msgType, uint32 msgSrc);

   void OnRecvPacket(uint32 srcId,
                     const uint8 *packet,
                     size_t packetSize);
   bool SendPingMsg(uint32 destId,
                    uint32 capability);
   bool SendPingReplyMsg(uint32 destId,
                         uint32 capability);
   bool SendCmdReplyMsg(uint32 destId, uint32 cmd, uint32 status);
   bool SendMsg(RpcParams *params,
                const uint8 *binary,
                uint32 binarySize);
   bool SendMsg(RpcParams *params,
                const CPClipboard *clip);
   bool SendMsg(RpcParams *params)
      { return SendMsg(params, NULL, 0); }
   uint32 GetVersionMajor(void) { return mVersionMajor; }
   uint32 GetVersionMinor(void) { return mVersionMinor; }

   bool AddRpcReceivedListener(const DnDRpcListener *obj);
   bool RemoveRpcReceivedListener(const DnDRpcListener *obj);
   bool AddRpcSentListener(const DnDRpcListener *obj);
   bool RemoveRpcSentListener(const DnDRpcListener *obj);

   void SetMaxTransportPacketSize(const uint32 size);

private:
   void FireRpcReceivedCallbacks(uint32 cmd, uint32 src, uint32 session);
   void FireRpcSentCallbacks(uint32 cmd, uint32 dest, uint32 session);
   bool SendMsg(DnDCPMsgV4 *msg);
   bool RequestNextPacket(void);
   void HandlePacket(uint32 srcId,
                     const uint8 *packet,
                     size_t packetSize);
   void HandlePacket(uint32 srcId,
                     const uint8 *packet,
                     size_t packetSize,
                     DnDCPMsgPacketType packetType);
   void HandleMsg(DnDCPMsgV4 *msg);

   RpcBase *mRpc;
   uint32 mVersionMajor;
   uint32 mVersionMinor;
   DnDCPMsgV4 mBigMsgIn;
   DnDCPMsgV4 mBigMsgOut;
   uint32 mMsgType;
   uint32 mMsgSrc;
   DblLnkLst_Links mRpcSentListeners;
   DblLnkLst_Links mRpcReceivedListeners;
   uint32 mMaxTransportPacketPayloadSize;
};

#endif // RPC_V4_UTIL_HPP

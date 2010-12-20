/* **************************************************************************
 * Copyright (C) 2010 VMware, Inc. All Rights Reserved -- VMware Confidential
 * **************************************************************************/

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


extern "C" {
   #include "dnd.h"
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
};

#endif // RPC_V4_UTIL_HPP

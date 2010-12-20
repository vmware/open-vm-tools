/* **************************************************************************
 * Copyright (C) 2010 VMware, Inc. All Rights Reserved -- VMware Confidential
 * **************************************************************************/

/**
 * @dndCPTransportGuestRpc.hpp --
 *
 * GuestRpc implementation of the dndCPTransport interface. Both vmx and guest
 * tools use this class for DnD version 4.
 */


#ifndef DND_CP_TRANSPORT_GUEST_RPC_HPP
#define DND_CP_TRANSPORT_GUEST_RPC_HPP

#include "dndCPTransport.h"

extern "C" {
   #include "dnd.h"
#ifdef VMX86_TOOLS
   #include "vmware/tools/guestrpc.h"
#else
   #include "guest_rpc.h"
#endif
}

#define GUEST_RPC_CMD_STR_DND "dnd.transport"
#define GUEST_RPC_CMD_STR_CP  "copypaste.transport"
#define GUEST_RPC_DND_DISABLE "dndDisable"
#define GUEST_RPC_CP_DISABLE  "copyDisable"

class DnDCPTransportGuestRpc;
typedef struct GuestRpcCBCtx {
   DnDCPTransportGuestRpc *transport;
   TransportInterfaceType type;
} GuestRpcCBCtx;

class TransportGuestRpcTables
{
public:
   TransportGuestRpcTables(void);
   ~TransportGuestRpcTables(void);

   RpcBase *GetRpc(TransportInterfaceType type);
   void SetRpc(TransportInterfaceType type, RpcBase *rpc);
#ifndef VMX86_TOOLS
   GuestRpcCmd GetCmd(TransportInterfaceType type);
#endif
   const char *GetCmdStr(TransportInterfaceType type);
   const char *GetDisableStr(TransportInterfaceType type);

private:
   RpcBase *mRpcList[TRANSPORT_INTERFACE_MAX];
#ifndef VMX86_TOOLS
   GuestRpcCmd mCmdTable[TRANSPORT_INTERFACE_MAX];
#endif
   const char *mCmdStrTable[TRANSPORT_INTERFACE_MAX];
   const char *mDisableStrTable[TRANSPORT_INTERFACE_MAX];
};


class DnDCPTransportGuestRpc
   : public DnDCPTransport
{
public:
#ifdef VMX86_TOOLS
   DnDCPTransportGuestRpc(RpcChannel *chan);
#else
   DnDCPTransportGuestRpc(void);
#endif

   bool Init(void);
   virtual bool RegisterRpc(RpcBase *rpc,
                            TransportInterfaceType type);
   virtual bool UnregisterRpc(TransportInterfaceType type);

   virtual bool SendPacket(uint32 destId,
                           TransportInterfaceType type,
                           const uint8 *msg,
                           size_t length);
   void OnRecvPacket(TransportInterfaceType type,
                     const uint8 *packet,
                     size_t packetSize);
private:
   TransportGuestRpcTables mTables;
   GuestRpcCBCtx mCBCtx[TRANSPORT_INTERFACE_MAX];
#ifdef VMX86_TOOLS
   RpcChannel *mRpcChannel;
   RpcChannelCallback mRpcChanCBList[TRANSPORT_INTERFACE_MAX];
#endif
};

#endif // DND_CP_TRANSPORT_GUEST_RPC_HPP

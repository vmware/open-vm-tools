/*********************************************************
 * Copyright (c) 2010-2017,2021 VMware, Inc. All rights reserved.
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
 * @dndCPTransportGuestRpc.hpp --
 *
 * GuestRpc implementation of the dndCPTransport interface. Both vmx and guest
 * tools use this class for DnD version 4.
 */


#ifndef DND_CP_TRANSPORT_GUEST_RPC_HPP
#define DND_CP_TRANSPORT_GUEST_RPC_HPP

#include "dndCPTransport.h"

#include "dnd.h"

#ifdef VMX86_TOOLS
   #include "vmware/tools/guestrpc.h"
#else
extern "C" {
   #include "guest_rpc.h"
}
#endif

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

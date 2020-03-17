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
 * @dndCPTransportGuestRpc.cpp --
 *
 * GuestRpc implementation of the dndCPTransport interface.
 *
 * XXX Move this to a common lib or source location.
 */

#include "dndCPTransportGuestRpc.hpp"
#include "rpcBase.h"

#include "str.h"
#include "hostinfo.h"
#include "dndMsg.h"
#include "util.h"

extern "C" {
#ifdef VMX86_TOOLS
   #include "debug.h"
   #include "rpcout.h"
   #include "rpcin.h"
   #define LOG(level, ...) Debug(__VA_ARGS__)
#else
   #include "dndCPInt.h"
   #include "guest_rpc.h"
   #include "tclodefs.h"

   #define LOGLEVEL_MODULE dnd
   #include "loglevel_user.h"
#endif
}

#ifdef VMX86_TOOLS
/**
 * Received a message from guestRpc. Forward the message to transport class.
 *
 * @param[out] result reply string.
 * @param[out] resultLen reply string length in byte.
 * @param[in] name guestRpc cmd name.
 * @param[in] args argument list.
 * @param[in] argsSize argument list size in byte.
 * @param[in] clientData callback client data.
 *
 * @return TRUE if success, FALSE otherwise.
 */

static gboolean
RecvMsgCB(RpcInData *data) // IN/OUT
{
   LOG(4, "%s: receiving\n", __FUNCTION__);

   const uint8 *packet = (const uint8 *)(data->args + 1);
   size_t packetSize = data->argsSize - 1;

   /* '- 1' is to ignore empty space between command and args. */
   if ((data->argsSize - 1) <= 0) {
      Debug("%s: invalid argsSize\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "invalid arg size", FALSE);
   }
   GuestRpcCBCtx *ctx = (GuestRpcCBCtx *)data->clientData;
#else
/**
 * Received a message from guestRpc. Forward the message to transport class.
 *
 * @param[in] clientData callback client data.
 * @param[ignored] channelId
 * @param[in] args argument list.
 * @param[in] argsSize argument list size in byte.
 * @param[out] result reply string.
 * @param[out] resultSize reply string length in byte.
 *
 * @return TRUE if success, FALSE otherwise.
 */

static Bool
RecvMsgCB(void *clientData,
          GuestRpcChannel *chan,
          const unsigned char *args,
          uint32 argsSize,
          unsigned char **result,
          uint32 *resultSize)
{
   const uint8 *packet = args;
   size_t packetSize = argsSize;
   GuestRpcCBCtx *ctx = (GuestRpcCBCtx *)clientData;
#endif

   ASSERT(ctx);
   ASSERT(ctx->transport);
   ctx->transport->OnRecvPacket(ctx->type, packet, packetSize);

#ifdef VMX86_TOOLS
   return RPCIN_SETRETVALS(data, "", TRUE);
#else
   return GuestRpc_SetRetVals(result, resultSize, (char *)"", TRUE);
#endif
}


/**
 * Constructor.
 */

TransportGuestRpcTables::TransportGuestRpcTables(void)
{
   for (int i = 0; i < TRANSPORT_INTERFACE_MAX; i++) {
      mRpcList[i] = NULL;
      mCmdStrTable[i] = NULL;
      mDisableStrTable[i] = NULL;
#ifdef VMX86_TOOLS
   }
#else
      mCmdTable[i] = GUESTRPC_CMD_MAX;
   }
   mCmdTable[TRANSPORT_GUEST_CONTROLLER_DND] = GUESTRPC_CMD_DND_TRANSPORT;
   mCmdTable[TRANSPORT_GUEST_CONTROLLER_CP] = GUESTRPC_CMD_COPYPASTE_TRANSPORT;
#endif

   mCmdStrTable[TRANSPORT_GUEST_CONTROLLER_DND] = (char *)GUEST_RPC_CMD_STR_DND;
   mCmdStrTable[TRANSPORT_GUEST_CONTROLLER_CP] = (char *)GUEST_RPC_CMD_STR_CP;

   mDisableStrTable[TRANSPORT_GUEST_CONTROLLER_DND] = (char *)GUEST_RPC_DND_DISABLE;
   mDisableStrTable[TRANSPORT_GUEST_CONTROLLER_CP] = (char *)GUEST_RPC_CP_DISABLE;
}


/**
 * Destructor.
 */

TransportGuestRpcTables::~TransportGuestRpcTables(void)
{
}


/**
 * Get an rpc object by interface type.
 *
 * @param[in] type transport interface type
 *
 * @return a registered rpc, or NULL if the rpc for the type is not registered.
 */

RpcBase *
TransportGuestRpcTables::GetRpc(TransportInterfaceType type)
{
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);
   return mRpcList[type];
}


/**
 * Add a rpc into rpc table.
 *
 * @param[in] type transport interface type
 * @param[in] rpc rpc which is added into the table.
 */

void
TransportGuestRpcTables::SetRpc(TransportInterfaceType type,
                                RpcBase *rpc)
{
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);
   mRpcList[type] = rpc;
}


#ifndef VMX86_TOOLS
/**
 * Get a guestRpc cmd by interface type.
 *
 * @param[in] type transport interface type
 *
 * @return a guestRpc cmd.
 */

GuestRpcCmd
TransportGuestRpcTables::GetCmd(TransportInterfaceType type)
{
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);

   return mCmdTable[type];
}
#endif


/**
 * Get a guestRpc cmd string by interface type.
 *
 * @param[in] type transport interface type
 *
 * @return a guestRpc cmd string.
 */

const char *
TransportGuestRpcTables::GetCmdStr(TransportInterfaceType type)
{
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);

   return mCmdStrTable[type];
}


/**
 * Get a guestRpc cmd disable string by interface type.
 *
 * @param[in] type transport interface type
 *
 * @return a guestRpc cmd disable string.
 */

const char *
TransportGuestRpcTables::GetDisableStr(TransportInterfaceType type)
{
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);

   return mDisableStrTable[type];
}


/**
 * Constructor.
 *
 * @param[in] rpcIn
 */

#ifdef VMX86_TOOLS
DnDCPTransportGuestRpc::DnDCPTransportGuestRpc(RpcChannel *chan)
   : mRpcChannel(chan)
#else
DnDCPTransportGuestRpc::DnDCPTransportGuestRpc(void)
#endif
{
   for (int i = 0; i < TRANSPORT_INTERFACE_MAX; i++) {
      mCBCtx[i].transport = this;
      mCBCtx[i].type = (TransportInterfaceType)i;
#ifdef VMX86_TOOLS
      mRpcChanCBList[i].xdrInSize = 0;
#endif
   }
}


/**
 * Register a rpc callback to an interface.
 *
 * @param[in] rpc rpc which is listening to the message.
 * @param[in] type the interface type rpc is listening to.
 *
 * @return true on success, false otherwise.
 */

bool
DnDCPTransportGuestRpc::RegisterRpc(RpcBase *rpc,
                                    TransportInterfaceType type)
{
   if (mTables.GetRpc(type)) {
      LOG(0, "%s: the type %d is already registered\n", __FUNCTION__, type);
      UnregisterRpc(type);
   }
   const char *cmdStr = (const char *)mTables.GetCmdStr(type);
   const char *disableStr = mTables.GetDisableStr(type);

   if (!cmdStr || !disableStr) {
      LOG(0, "%s: can not find valid cmd for %d, cmdStr %s disableStr %s\n",
          __FUNCTION__, type, (cmdStr ? cmdStr : "NULL"),
          (disableStr ? disableStr : "NULL"));
      return false;
   }

   ASSERT(mCBCtx);
   ASSERT(type == TRANSPORT_GUEST_CONTROLLER_DND ||
          type == TRANSPORT_GUEST_CONTROLLER_CP ||
          type == TRANSPORT_GUEST_CONTROLLER_FT);
   LOG(4, "%s: for %s\n", __FUNCTION__, cmdStr);

#ifdef VMX86_TOOLS
   ASSERT(mRpcChannel);
   mRpcChanCBList[type].name = cmdStr;
   mRpcChanCBList[type].callback = RecvMsgCB;
   mRpcChanCBList[type].clientData = &mCBCtx[type];
   mRpcChanCBList[type].xdrIn = NULL;
   mRpcChanCBList[type].xdrOut = NULL;
   mRpcChanCBList[type].xdrInSize = 0;
   RpcChannel_RegisterCallback(mRpcChannel, &mRpcChanCBList[type]);
#else
   GuestRpc_RegisterCommand(mTables.GetCmd(type), disableStr,
                            (const unsigned char *)cmdStr, RecvMsgCB, &mCBCtx[type]);
#endif
   mTables.SetRpc(type, rpc);
   return true;
}


/**
 * Unregister a rpc callback.
 *
 * @param[in] type the interface type rpc is listening to.
 *
 * @return true on success, false otherwise.
 */

bool
DnDCPTransportGuestRpc::UnregisterRpc(TransportInterfaceType type)
{
   if (!mTables.GetRpc(type)) {
      LOG(0, "%s: the type %d is not registered\n", __FUNCTION__, type);
      return false;
   }
#ifdef VMX86_TOOLS
   ASSERT(mRpcChannel);
   RpcChannel_UnregisterCallback(mRpcChannel, &mRpcChanCBList[type]);
#else
   GuestRpc_UnregisterCommand(mTables.GetCmd(type));
#endif
   mTables.SetRpc(type, NULL);
   return true;
}


/**
 * Wrap the buffer into an rpc and send it to the peer.
 *
 * @param[ignored] destId destination address id
 * @param[in] type transport interface type
 * @param[in] data Payload buffer
 * @param[in] dataSize Payload buffer size
 *
 * @return true on success, false otherwise.
 */

bool
DnDCPTransportGuestRpc::SendPacket(uint32 destId,
                                   TransportInterfaceType type,
                                   const uint8 *msg,
                                   size_t length)
{
   char *rpc = NULL;
   size_t rpcSize = 0;
   size_t nrWritten = 0;
   const char *cmd = mTables.GetCmdStr(type);
   bool ret = true;

   if (!cmd) {
      LOG(0, "%s: can not find valid cmd for %d\n", __FUNCTION__, type);
      return false;
   }
   rpcSize = strlen(cmd) + 1 + length;
   rpc = (char *)Util_SafeMalloc(rpcSize);
   nrWritten = Str_Sprintf(rpc, rpcSize, "%s ", cmd);

   if (length > 0) {
      ASSERT(nrWritten + length <= rpcSize);
      memcpy(rpc + nrWritten, msg, length);
   }

#ifdef VMX86_TOOLS
   ret = (TRUE == RpcChannel_Send(mRpcChannel, rpc, rpcSize, NULL, NULL));

   if (!ret) {
      LOG(0, "%s: failed to send msg to host\n", __FUNCTION__);
   }

   free(rpc);
#else
   GuestRpc_SendWithTimeOut((const unsigned char *)TOOLS_DND_NAME,
                            (const unsigned char *)rpc, rpcSize,
                            GuestRpc_GenericCompletionRoutine, rpc,
                            DND_TIMEOUT);
#endif
   return ret;
}


/**
 * Callback after receiving a guestRpc message.
 *
 * @param[in] type transport interface type
 * @param[in] packet Payload buffer
 * @param[in] packetSize Payload buffer size
 */

void
DnDCPTransportGuestRpc::OnRecvPacket(TransportInterfaceType type,
                                     const uint8 *packet,
                                     size_t packetSize)
{
   RpcBase *rpc = mTables.GetRpc(type);
   if (!rpc) {
      LOG(0, "%s: can not find valid rpc for %d\n", __FUNCTION__, type);
      return;
   }
   rpc->OnRecvPacket(DEFAULT_CONNECTION_ID, packet, packetSize);
}

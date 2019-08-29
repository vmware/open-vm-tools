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
 * @rpcV4Util.cpp --
 *
 * Implementation of common utility object for DnD/CP version 4 rpc object.
 * It is shared by host, guest and UI implementation. Some common utilities
 * including
 * *packet marshalling/un-marshalling
 * *common rpc (ping, pingReply, etc)
 * *big buffer support
 * are implemented here.
 */


#include "rpcV4Util.hpp"

#ifdef VMX86_TOOLS
extern "C" {
   #include "debug.h"
   #define LOG(level, msg) (Debug msg)
}
#else
   #define LOGLEVEL_MODULE dnd
   #include "loglevel_user.h"
#endif

extern "C" {
#include "dndClipboard.h"
}

#include "util.h"

/**
 * Constructor.
 */

RpcV4Util::RpcV4Util(void)
   : mVersionMajor(4),
     mVersionMinor(0),
     mMaxTransportPacketPayloadSize(DND_CP_PACKET_MAX_PAYLOAD_SIZE_V4)
{
   DnDCPMsgV4_Init(&mBigMsgIn);
   DnDCPMsgV4_Init(&mBigMsgOut);
   DblLnkLst_Init(&mRpcSentListeners);
   DblLnkLst_Init(&mRpcReceivedListeners);
}


/**
 * Destructor.
 */

RpcV4Util::~RpcV4Util(void)
{
   DnDCPMsgV4_Destroy(&mBigMsgIn);
   DnDCPMsgV4_Destroy(&mBigMsgOut);

   while (DblLnkLst_IsLinked(&mRpcSentListeners)) {
      DnDRpcSentListenerNode *node =
         DblLnkLst_Container(mRpcSentListeners.next,
                             DnDRpcSentListenerNode, l);

      ASSERT(node);
      DblLnkLst_Unlink1(&node->l);
      free(node);
   }

   while (DblLnkLst_IsLinked(&mRpcReceivedListeners)) {
      DnDRpcReceivedListenerNode *node =
         DblLnkLst_Container(mRpcReceivedListeners.next,
                             DnDRpcReceivedListenerNode, l);

      ASSERT(node);
      DblLnkLst_Unlink1(&node->l);
      free(node);
   }
}


/**
 * Initialize the RpcV4Util object. All owner should call this first before
 * calling any other utility function.
 *
 * @param[in] rpc the owner of this utility object
 * @param[in] msgType the type of message (DnD/CP/FT)
 * @param[in] msgSrc source of the message (host/guest/controller)
 */

void
RpcV4Util::Init(RpcBase *rpc,
                uint32 msgType,
                uint32 msgSrc)
{
   ASSERT(rpc);
   mRpc = rpc;
   mMsgType = msgType;
   mMsgSrc = msgSrc;
}


/**
 * Serialize the clipboard item if there is one, then send the message to
 * destId.
 *
 * @param[in] params parameter list for the message
 * @param[in] clip the clipboard item.
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendMsg(RpcParams *params,
                   const CPClipboard *clip)
{
   DynBuf buf;
   bool ret = false;

   ASSERT(params);

   if (!clip) {
      return SendMsg(params);
   }

   DynBuf_Init(&buf);

   if (!CPClipboard_Serialize(clip, &buf)) {
      LOG(0, ("%s: CPClipboard_Serialize failed.\n", __FUNCTION__));
      goto exit;
   }

   ret = SendMsg(params,
                 (const uint8 *)DynBuf_Get(&buf),
                 (uint32)DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   return ret;
}


/**
 * Serialize the message and send it to destId.
 *
 * @param[in] params parameter list for the message
 * @param[in] binary
 * @param[in] binarySize
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendMsg(RpcParams *params,
                   const uint8 *binary,
                   uint32 binarySize)
{
   bool ret = false;
   DnDCPMsgV4 *msgOut = NULL;
   DnDCPMsgV4 shortMsg;

   ASSERT(params);

   DnDCPMsgV4_Init(&shortMsg);

   if (binarySize > mMaxTransportPacketPayloadSize) {
      /*
       * For big message, all information should be cached in mBigMsgOut
       * because multiple packets and sends are needed.
       */
      DnDCPMsgV4_Destroy(&mBigMsgOut);
      msgOut = &mBigMsgOut;
   } else {
      /* For short message, the temporary shortMsg is enough. */
      msgOut = &shortMsg;
   }

   msgOut->addrId = params->addrId;
   msgOut->hdr.cmd = params->cmd;
   msgOut->hdr.type = mMsgType;
   msgOut->hdr.src = mMsgSrc;
   msgOut->hdr.sessionId = params->sessionId;
   msgOut->hdr.status = params->status;
   msgOut->hdr.param1 = params->optional.genericParams.param1;
   msgOut->hdr.param2 = params->optional.genericParams.param2;
   msgOut->hdr.param3 = params->optional.genericParams.param3;
   msgOut->hdr.param4 = params->optional.genericParams.param4;
   msgOut->hdr.param5 = params->optional.genericParams.param5;
   msgOut->hdr.param6 = params->optional.genericParams.param6;
   msgOut->hdr.binarySize = binarySize;
   msgOut->hdr.payloadOffset = 0;
   msgOut->hdr.payloadSize = 0;
   msgOut->binary = NULL;
   if (binarySize > 0) {
      msgOut->binary = (uint8 *)(Util_SafeMalloc(binarySize));
      memcpy(msgOut->binary, binary,binarySize);
   }

   ret = SendMsg(msgOut);
   /* The mBigMsgOut is destroyed when the message sending was failed. */
   if (!ret && msgOut == &mBigMsgOut) {
      DnDCPMsgV4_Destroy(&mBigMsgOut);
   }
   DnDCPMsgV4_Destroy(&shortMsg);
   return ret;
}


/**
 * Construct a DNDCP_CMD_PING message and send it to destId.
 *
 * @param[in] destId destination address id
 * @param[in] capability
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendPingMsg(uint32 destId,
                       uint32 capability)
{
   RpcParams params;
   memset(&params, 0, sizeof params);
   params.addrId = destId;
   params.cmd = DNDCP_CMD_PING;
   params.optional.version.major = mVersionMajor;
   params.optional.version.minor = mVersionMinor;
   params.optional.version.capability = capability;

   return SendMsg(&params);
}


/**
 * Construct a DNDCP_CMD_PING_REPLY message and send it to destId.
 *
 * @param[in] destId destination address id
 * @param[in] capability
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendPingReplyMsg(uint32 destId,
                            uint32 capability)
{
   RpcParams params;
   memset(&params, 0, sizeof params);
   params.addrId = destId;
   params.cmd = DNDCP_CMD_PING_REPLY;
   params.optional.version.major = mVersionMajor;
   params.optional.version.minor = mVersionMinor;
   params.optional.version.capability = capability;

   return SendMsg(&params);
}


/**
 * Construct a DNDCP_CMP_REPLY message and send it to destId.
 *
 * @param[in] destId destination address id
 * @param[in] cmd the command to be replied
 * @param[in] status
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendCmdReplyMsg(uint32 destId,
                            uint32 cmd,
                            uint32 status)
{
   RpcParams params;
   memset(&params, 0, sizeof params);
   params.addrId = destId;
   params.cmd = DNDCP_CMP_REPLY;
   params.status = status;
   params.optional.replyToCmd.cmd = cmd;

   return SendMsg(&params);
}


/**
 * Construct a DNDCP_CMD_REQUEST_NEXT message and send it to mBigMsgIn.addrId.
 * This is used for big message receiving. After received a packet, receiver
 * side should send this message to ask for next piece of binary.
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::RequestNextPacket(void)
{
   RpcParams params;
   memset(&params, 0, sizeof params);
   params.addrId = mBigMsgIn.addrId;
   params.cmd = DNDCP_CMD_REQUEST_NEXT;
   params.sessionId = mBigMsgIn.hdr.sessionId;
   params.optional.requestNextCmd.cmd = mBigMsgIn.hdr.cmd;
   params.optional.requestNextCmd.binarySize = mBigMsgIn.hdr.binarySize;
   params.optional.requestNextCmd.payloadOffset = mBigMsgIn.hdr.payloadOffset;

   return SendMsg(&params);
}


/**
 * Serialize a message and send it to msg->addrId.
 *
 * @param[in] msg the message to be serialized
 *
 * @return true on success, false otherwise.
 */

bool
RpcV4Util::SendMsg(DnDCPMsgV4 *msg)
{
   uint8 *packet = NULL;
   size_t packetSize = 0;
   bool ret = false;

   if (!DnDCPMsgV4_SerializeWithInputPayloadSizeCheck(msg, &packet,
      &packetSize, mMaxTransportPacketPayloadSize)) {
      LOG(1, ("%s: DnDCPMsgV4_Serialize failed. \n", __FUNCTION__));
      return false;
   }

   ret = mRpc->SendPacket(msg->addrId, packet, packetSize);
   if (ret == true) {
      FireRpcSentCallbacks(msg->hdr.cmd,
                           msg->addrId,
                           msg->hdr.sessionId);
   }
   free(packet);
   return ret;
}


/**
 * Callback from transport layer after received a packet from srcId.
 *
 * @param[in] srcId addressId where the packet is from
 * @param[in] packet
 * @param[in] packetSize
 */

void
RpcV4Util::OnRecvPacket(uint32 srcId,
                        const uint8 *packet,
                        size_t packetSize)
{
   DnDCPMsgPacketType packetType = DND_CP_MSG_PACKET_TYPE_INVALID;

   if (packetSize <= mMaxTransportPacketPayloadSize + DND_CP_MSG_HEADERSIZE_V4) {
      packetType = DnDCPMsgV4_GetPacketType(packet, packetSize, mMaxTransportPacketPayloadSize);
   }

   switch (packetType) {
   case DND_CP_MSG_PACKET_TYPE_SINGLE:
      HandlePacket(srcId, packet, packetSize);
      break;
   case DND_CP_MSG_PACKET_TYPE_MULTIPLE_NEW:
   case DND_CP_MSG_PACKET_TYPE_MULTIPLE_CONTINUE:
   case DND_CP_MSG_PACKET_TYPE_MULTIPLE_END:
      HandlePacket(srcId, packet, packetSize, packetType);
      break;
   default:
      LOG(1, ("%s: invalid packet. \n", __FUNCTION__));
      SendCmdReplyMsg(srcId, DNDCP_CMD_INVALID, DND_CP_MSG_STATUS_INVALID_PACKET);
      break;
   }
}


/**
 * Handle a packet for short message.
 *
 * @param[in] srcId addressId where the packet is from
 * @param[in] packet
 * @param[in] packetSize
 */

void
RpcV4Util::HandlePacket(uint32 srcId,
                    const uint8 *packet,
                    size_t packetSize)
{
   DnDCPMsgV4 msgIn;

   DnDCPMsgV4_Init(&msgIn);

   if (!DnDCPMsgV4_UnserializeSingle(&msgIn, packet, packetSize)) {
      LOG(1, ("%s: invalid packet. \n", __FUNCTION__));
      SendCmdReplyMsg(srcId, DNDCP_CMD_INVALID, DND_CP_MSG_STATUS_INVALID_PACKET);
      return;
   }

   msgIn.addrId = srcId;
   HandleMsg(&msgIn);

   DnDCPMsgV4_Destroy(&msgIn);
}


/**
 * Handle a packet for long message.
 *
 * @param[in] srcId addressId where the packet is from
 * @param[in] packet
 * @param[in] packetSize
 * @param[in] packetType
 */

void
RpcV4Util::HandlePacket(uint32 srcId,
                    const uint8 *packet,
                    size_t packetSize,
                    DnDCPMsgPacketType packetType)
{
   if (!DnDCPMsgV4_UnserializeMultiple(&mBigMsgIn, packet, packetSize)) {
      LOG(1, ("%s: invalid packet. \n", __FUNCTION__));
      SendCmdReplyMsg(srcId, DNDCP_CMD_INVALID, DND_CP_MSG_STATUS_INVALID_PACKET);
      goto cleanup;
   }

   mBigMsgIn.addrId = srcId;

   /*
    * If there are multiple packets for the message, sends DNDCP_REQUEST_NEXT
    * back to sender to ask for next packet.
    */
   if (DND_CP_MSG_PACKET_TYPE_MULTIPLE_END != packetType) {
      if (!RequestNextPacket()) {
         LOG(1, ("%s: RequestNextPacket failed.\n", __FUNCTION__));
         goto cleanup;
      }
      /*
       * Do not destroy mBigMsgIn here because more packets are expected for
       * this message.
       */
      return;
   }

   HandleMsg(&mBigMsgIn);

cleanup:
   DnDCPMsgV4_Destroy(&mBigMsgIn);
}


/**
 * Handle a received message.
 *
 * @param[in] msgIn received message.
 */

void
RpcV4Util::HandleMsg(DnDCPMsgV4 *msgIn)
{
   RpcParams params;

   ASSERT(msgIn);

   if (DNDCP_CMD_REQUEST_NEXT == msgIn->hdr.cmd) {
      /*
       * This is for big buffer support. The receiver is asking for next piece
       * of data. For details about big buffer support, please refer to
       * https://wiki.eng.vmware.com/DnDVersion4Message#Binary_Buffer
       */
      bool ret = SendMsg(&mBigMsgOut);

      if (!ret) {
         LOG(1, ("%s: SendMsg failed. \n", __FUNCTION__));
      }

      /*
       * mBigMsgOut will be destroyed if SendMsg failed or whole message has
       * been sent.
       */
      if (!ret || mBigMsgOut.hdr.payloadOffset == mBigMsgOut.hdr.binarySize) {
         DnDCPMsgV4_Destroy(&mBigMsgOut);
      }
      return;
   }

   params.addrId = msgIn->addrId;
   params.cmd = msgIn->hdr.cmd;
   params.sessionId = msgIn->hdr.sessionId;
   params.status = msgIn->hdr.status;
   params.optional.genericParams.param1 = msgIn->hdr.param1;
   params.optional.genericParams.param2 = msgIn->hdr.param2;
   params.optional.genericParams.param3 = msgIn->hdr.param3;
   params.optional.genericParams.param4 = msgIn->hdr.param4;
   params.optional.genericParams.param5 = msgIn->hdr.param5;
   params.optional.genericParams.param6 = msgIn->hdr.param6;

   mRpc->HandleMsg(&params, msgIn->binary, msgIn->hdr.binarySize);
   FireRpcReceivedCallbacks(msgIn->hdr.cmd, msgIn->addrId, msgIn->hdr.sessionId);
}


/**
 * Add an rpc received listener to the list.
 *
 * @param[in] listener class instance that implements DnDRpcListener
 *
 * @note generally, needs to be matched with a call to
 * RemoveRpcReceivedListener, but if not, cleanup happens in destructor.
 */

bool
RpcV4Util::AddRpcReceivedListener(const DnDRpcListener *listener)
{
   ASSERT(listener);

   DnDRpcReceivedListenerNode *node =
      (DnDRpcReceivedListenerNode *) Util_SafeMalloc(sizeof(DnDRpcReceivedListenerNode));
   DblLnkLst_Init(&node->l);
   node->listener = listener;
   DblLnkLst_LinkLast(&mRpcReceivedListeners, &node->l);
   return true;
}


/**
 * Remove an rpc received listener from the list.
 *
 * @param[in] listener class instance that implements DnDRpcReceivedListener
 *
 * @note only the first instance of the listener will be removed.
 */

bool
RpcV4Util::RemoveRpcReceivedListener(const DnDRpcListener *listener)
{
   ASSERT(listener);

   DblLnkLst_Links *curr;

   DblLnkLst_ForEach(curr, &mRpcReceivedListeners) {
      DnDRpcReceivedListenerNode *p =
         DblLnkLst_Container(curr, DnDRpcReceivedListenerNode, l);
      if (p && p->listener == listener) {
         DblLnkLst_Unlink1(&p->l);
         free(p);
         return true;
      }
   }
   return false;
}


/**
 * Fire all registered rpc received callbacks.
 *
 * @param[in] cmd rpc command
 * @param[in] src src ID
 * @param[in] session session ID
 */

void
RpcV4Util::FireRpcReceivedCallbacks(uint32 cmd,
                                    uint32 src,
                                    uint32 session)
{
   DblLnkLst_Links *curr = NULL;
   DnDRpcReceivedListenerNode *p = NULL;
   DnDRpcListener *listener = NULL;

   DblLnkLst_ForEach(curr, &mRpcReceivedListeners) {
      p = DblLnkLst_Container(curr, DnDRpcReceivedListenerNode, l);
      if (p) {
         listener = const_cast<DnDRpcListener *>(p->listener);
         listener->OnRpcReceived(cmd, src, session);
      }
   }
}


/**
 * Add an rpc sent listener to the list.
 *
 * @param[in] listener class instance that implements DnDRpcListener
 *
 * @note generally, needs to be matched with a call to
 * RemoveRpcSentListener, but if not, cleanup happens in destructor.
 */

bool
RpcV4Util::AddRpcSentListener(const DnDRpcListener *listener)
{
   ASSERT(listener);

   DnDRpcSentListenerNode *node =
      (DnDRpcSentListenerNode *) Util_SafeMalloc(sizeof *node);
   DblLnkLst_Init(&node->l);
   node->listener = listener;
   DblLnkLst_LinkLast(&mRpcSentListeners, &node->l);
   return true;
}


/**
 * Remove an rpc sent listener from the list.
 *
 * @param[in] listener class instance that implements DnDRpcSentListener
 *
 * @note only the first instance of the listener will be removed.
 */

bool
RpcV4Util::RemoveRpcSentListener(const DnDRpcListener *listener)
{
   ASSERT(listener);

   DblLnkLst_Links *curr;

   DblLnkLst_ForEach(curr, &mRpcSentListeners) {
      DnDRpcSentListenerNode *p =
         DblLnkLst_Container(curr, DnDRpcSentListenerNode, l);
      if (p && p->listener == listener) {
         DblLnkLst_Unlink1(&p->l);
         free(p);
         return true;
      }
   }
   return false;
}


/**
 * Fire all registered rpc sent callbacks.
 *
 * @param[in] cmd rpc command
 * @param[in] dest destination ID
 * @param[in] session session ID
 */

void
RpcV4Util::FireRpcSentCallbacks(uint32 cmd,
                                uint32 dest,
                                uint32 session)
{
   DblLnkLst_Links *curr = NULL;

   DblLnkLst_ForEach(curr, &mRpcSentListeners) {
      DnDRpcSentListenerNode *p =
         DblLnkLst_Container(curr, DnDRpcSentListenerNode, l);
      if (p && p->listener) {
         const_cast<DnDRpcListener *>(p->listener)->OnRpcSent(cmd,
                                                              dest,
                                                              session);
      }
   }
}


/**
 * Set the max transport packet size of RPC messages.
 *
 * @param[in] size the new max packet size.
 */

void
RpcV4Util::SetMaxTransportPacketSize(const uint32 size)
{
   ASSERT(size > DND_CP_MSG_HEADERSIZE_V4);

   uint32 newProposedPayloadSize = size - DND_CP_MSG_HEADERSIZE_V4;
   if (newProposedPayloadSize < DND_CP_PACKET_MAX_PAYLOAD_SIZE_V4) {
      /*
       * Reset the max transport packet payload size
       * if the new size is stricter than the default one.
       */
      mMaxTransportPacketPayloadSize = newProposedPayloadSize;
      LOG(1, ("%s: The packet size is set to %u. \n", __FUNCTION__,
              mMaxTransportPacketPayloadSize));
   }
}



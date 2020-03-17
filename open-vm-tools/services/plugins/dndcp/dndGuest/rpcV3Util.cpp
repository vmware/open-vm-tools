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
 * @rpcV3Util.cpp --
 *
 * Implementation of common utility object for DnD/CP version 3 rpc object.
 * It is shared by vmx and guest implementation. Some common utilities
 * including
 * *packet marshalling/un-marshalling
 * *big buffer support
 * are implemented here.
 */


#include "rpcV3Util.hpp"

#ifdef VMX86_TOOLS
extern "C" {      
   #include "debug.h"
   #define LOG(level, ...) Debug(__VA_ARGS__)
}
#else
   #define LOGLEVEL_MODULE dnd
   #include "loglevel_user.h"
#endif

extern "C" {      
#include "dndClipboard.h"
}

#include "util.h"
#include "dndMsg.h"
#include "hostinfo.h"

/**
 * Constructor.
 */

RpcV3Util::RpcV3Util(void)
   : mRpc(NULL),
     mVersionMajor(3),
     mVersionMinor(0),
     mSeqNum(1)
{
   mSendBuf.buffer = NULL;
   mRecvBuf.buffer = NULL;
   DnD_TransportBufReset(&mSendBuf);
   DnD_TransportBufReset(&mRecvBuf);
}


/**
 * Destructor.
 */

RpcV3Util::~RpcV3Util(void)
{
   free(mSendBuf.buffer);
   free(mRecvBuf.buffer);
}


/**
 * Initialize the RpcV3Util object. All owner should call this first before
 * calling any other utility function.
 *
 * @param[in] rpc the owner of this utility object
 * @param[in] msgType the type of message (DnD/CP/FT)
 * @param[in] msgSrc source of the message (host/guest/controller)
 */

void
RpcV3Util::Init(RpcBase *rpc)
{
   ASSERT(rpc);
   mRpc = rpc;
}


/**
 * Serialize command, then send the message.
 *
 * @param[in] cmd version 3 command
 *
 * @return true on success, false otherwise.
 */

bool
RpcV3Util::SendMsg(uint32 cmd)
{
   DnDMsg msg;
   bool ret;

   DnDMsg_Init(&msg);
   DnDMsg_SetCmd(&msg, cmd);
   ret = SendMsg(&msg);
   DnDMsg_Destroy(&msg);

   return ret;
}


/**
 * Serialize the clipboard item if there is one, then send the message to
 * destId.
 *
 * @param[in] cmd version 3 command
 * @param[in] clip the clipboard item.
 *
 * @return true on success, false otherwise.
 */

bool
RpcV3Util::SendMsg(uint32 cmd,
                   const CPClipboard *clip)
{
   DnDMsg msg;
   DynBuf buf;
   bool ret = false;

   DnDMsg_Init(&msg);
   DynBuf_Init(&buf);

   /* Serialize clip and output into buf. */
   if (!CPClipboard_Serialize(clip, &buf)) {
      LOG(0, "%s: CPClipboard_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   /* Construct msg with both cmd CP_HG_SET_CLIPBOARD and buf. */
   DnDMsg_SetCmd(&msg, cmd);
   if (!DnDMsg_AppendArg(&msg, DynBuf_Get(&buf), DynBuf_GetSize(&buf))) {
      LOG(0, "%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = SendMsg(&msg);

exit:
   DynBuf_Destroy(&buf);
   DnDMsg_Destroy(&msg);
   return ret;
}


/**
 * Serialize command with mouse position, and send the message.
 *
 * @param[in] cmd version 3 command
 * @param[in] x mouse position x.
 * @param[in] y mouse position y.
 *
 * @return true on success, false otherwise.
 */

bool
RpcV3Util::SendMsg(uint32 cmd,
                   int32 x,
                   int32 y)
{
   bool ret = false;

   DnDMsg msg;

   DnDMsg_Init(&msg);

   DnDMsg_SetCmd(&msg, cmd);

   if (!DnDMsg_AppendArg(&msg, &x, sizeof x) ||
       !DnDMsg_AppendArg(&msg, &y, sizeof y)) {
      LOG(0, "%s: DnDMsg_AppendData failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = SendMsg(&msg);

exit:
   DnDMsg_Destroy(&msg);
   return ret;
}


/**
 * Serialize and send the message.
 *
 * @param[in] msg the message to be serialized and sent.
 *
 * @return true on success, false otherwise.
 */

bool
RpcV3Util::SendMsg(const DnDMsg *msg)
{
   DynBuf buf;
   bool ret = false;

   DynBuf_Init(&buf);

   /* Serialize msg and output to buf. */
   if (!DnDMsg_Serialize((DnDMsg *)msg, &buf)) {
      LOG(0, "%s: DnDMsg_Serialize failed.\n", __FUNCTION__);
      goto exit;
   }

   ret = SendMsg((uint8 *)DynBuf_Get(&buf), DynBuf_GetSize(&buf));

exit:
   DynBuf_Destroy(&buf);
   return ret;
}


/**
 * Serialize the message and send it to destId.
 *
 * @param[in] binary
 * @param[in] binarySize
 *
 * @return true on success, false otherwise.
 */

bool
RpcV3Util::SendMsg(const uint8 *binary,
                   uint32 binarySize)
{
   DnDTransportPacketHeader *packet = NULL;
   size_t packetSize;
   bool ret = FALSE;

   if (binarySize > DNDMSG_MAX_ARGSZ) {
      LOG(1, "%s: message is too big, quit.\n", __FUNCTION__);
      return false;
   }

   LOG(4, "%s: got message, size %d.\n", __FUNCTION__, binarySize);

   if (binarySize <= DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE) {
      /*
       * It is a small size message, so it is not needed to buffer it. Just
       * put message into a packet and send it.
       */
      packetSize = DnD_TransportMsgToPacket((uint8 *)binary, binarySize,
                                            mSeqNum, &packet);
   } else {
      /*
       * It is a big size message. First buffer it and send it with multiple
       * packets.
       */
      if (mSendBuf.buffer) {
         /*
          * There is a pending big size message. If there is no update time
          * more than DND_MAX_TRANSPORT_LATENCY_TIME, remove old message and
          * send new message. Otherwise ignore this message.
          */

         if ((Hostinfo_SystemTimerUS() - mSendBuf.lastUpdateTime) <
             DND_MAX_TRANSPORT_LATENCY_TIME) {
            LOG(1, "%s: got a big buffer, but there is another pending one, drop it\n",
                __FUNCTION__);
            return false;
         }
      }
      DnD_TransportBufInit(&mSendBuf, (uint8 *)binary, binarySize, mSeqNum);
      packetSize = DnD_TransportBufGetPacket(&mSendBuf, &packet);
   }

   /* Increase sequence number for next message. */
   mSeqNum++;
   if (packetSize) {
      ret = mRpc->SendPacket(0, (const uint8 *)packet, packetSize);
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
RpcV3Util::OnRecvPacket(uint32 srcId,
                        const uint8 *packet,
                        size_t packetSize)
{
   DnDTransportPacketHeader *packetV3 = (DnDTransportPacketHeader *)packet;
   ASSERT(packetV3);
   if (packetSize <= 0 || packetSize > DND_MAX_TRANSPORT_PACKET_SIZE ||
       packetV3->payloadSize > DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE ||
       (packetV3->payloadSize + DND_TRANSPORT_PACKET_HEADER_SIZE) != packetSize) {
      goto invalid_packet;
   }

   switch (packetV3->type) {
   case DND_TRANSPORT_PACKET_TYPE_SINGLE:
      if (packetV3->payloadSize != packetV3->totalSize) {
         goto invalid_packet;
      }
      /* This is a single packet. Forward to rpc layer for further processing. */
      mRpc->HandleMsg(NULL, packetV3->payload, packetV3->payloadSize);
      break;
   case DND_TRANSPORT_PACKET_TYPE_REQUEST:
      {
         /* Peer is asking for next packet. */
         DnDTransportPacketHeader *replyPacket = NULL;
         size_t replyPacketSize;

         if (packetV3->payloadSize ||
             packetV3->seqNum != mSendBuf.seqNum ||
             packetV3->offset != mSendBuf.offset) {
            LOG(0, "%s: received packet does not match local buffer.\n",
                __FUNCTION__);
            return;
         }

         replyPacketSize = DnD_TransportBufGetPacket(&mSendBuf, &replyPacket);

         if (!replyPacketSize) {
            /*
             * Not needed to reset mSendBuf because DnD_TransportBufGetPacket already
             * did that.
             */
            LOG(0, "%s: DnD_TransportBufGetPacket failed.\n", __FUNCTION__);
            return;
         }

         if (!mRpc->SendPacket(0, (const uint8 *)replyPacket, replyPacketSize) ||
             mSendBuf.offset == mSendBuf.totalSize) {
            /* Reset mSendBuf if whole buffer is sent or there is any error. */
            DnD_TransportBufReset(&mSendBuf);
         }

         free(replyPacket);

         break;
      }
   case DND_TRANSPORT_PACKET_TYPE_PAYLOAD:
      /*
       * If seqNum does not match, it means either this is the first packet, or there
       * is a timeout in another side. The buffer will be reset in all cases later.
       * For the first packet, the totalSize should not larger than DNDMSG_MAX_ARGSZ.
       * For the rest packets, the totalSize should be the same as the first packet.
       */
      if (mRecvBuf.seqNum != packetV3->seqNum) {
         if (packetV3->totalSize > DNDMSG_MAX_ARGSZ) {
            goto invalid_packet;
         }
      } else {
         if (packetV3->totalSize != mRecvBuf.totalSize) {
            goto invalid_packet;
         }
      }

      /*
       * The totalSize has been validated.
       * We need to make sure the  payloadSize and offset are in right range.
       */
      if (packetV3->payloadSize > packetV3->totalSize ||
          packetV3->offset > packetV3->totalSize ||
          (packetV3->payloadSize + packetV3->offset) > packetV3->totalSize) {
         goto invalid_packet;
      }

      /* Received next packet for big binary buffer. */
      if (!DnD_TransportBufAppendPacket(&mRecvBuf, packetV3, packetSize)) {
         LOG(0, "%s: DnD_TransportBufAppendPacket failed.\n", __FUNCTION__);
         return;
      }

      if (mRecvBuf.offset == mRecvBuf.totalSize) {
         /*
          * Received all packets for the messge, forward it to rpc layer for
          * further processing.
          */
         mRpc->HandleMsg(NULL, mRecvBuf.buffer, mRecvBuf.totalSize);
         DnD_TransportBufReset(&mRecvBuf);
      } else {
         /* Send request for next packet. */
         DnDTransportPacketHeader *replyPacket = NULL;
         size_t replyPacketSize;

         replyPacketSize = DnD_TransportReqPacket(&mRecvBuf, &replyPacket);

         if (!replyPacketSize) {
            LOG(0, "%s: DnD_TransportReqPacket failed.\n", __FUNCTION__);
            return;
         }

         if (!mRpc->SendPacket(0, (const uint8 *)replyPacket, replyPacketSize)) {
            DnD_TransportBufReset(&mRecvBuf);
         }
         free(replyPacket);
      }
      break;
   default:
      LOG(0, "%s: unknown packet.\n", __FUNCTION__);
      break;
   }

invalid_packet:
   LOG(0, "%s: received invalid data.\n", __FUNCTION__);
}



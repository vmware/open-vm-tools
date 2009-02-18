/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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

/*
 * dndTransportGuestRpc.cc --
 *
 *     GuestRpc implementation of the dndTransport interface.
 *
 *     A multiple packet transport protocol is implemented in this layer to
 *     support big buffer transfer. With the support, message size limit is
 *     improved from 64K to 4M (theoratically can be 4G).
 *
 *     There are 3 different types of packet. If a message is smaller than
 *     64K, it will be sent with type DND_TRANSPORT_PACKET_TYPE_SINGLE. If
 *     it is bigger than 64K, initial 64K will be first sent with
 *     DND_TRANSPORT_PACKET_TYPE_PAYLOAD. After received the packet, the 
 *     receiver should reply with DND_TRANSPORT_PACKET_TYPE_REQUEST, then
 *     next 64K will be sent.
 *
 *     This is a temporary solution before VICF is available. To simplify
 *     the implementation, there are some limitations:
 *     1. Any time for any transport class, there is only one pending
 *        big buffer for each direction. Any following big size message will
 *        be dropped before the pending transfer is done.Small message (<64K)
 *        can be sent at any time.
 *     2. Caller can not cancel any pending big buffer sending and 
 *        receiving.
 *     3. Pending big buffer will be dropped if there is any error.
 */


#include <sigc++/hide.h>

#include "dndTransportGuestRpc.hh"

extern "C" {
   #include "util.h"
   #include "rpcout.h"
   #include "rpcin.h"
   #include "debug.h"
   #include "str.h"
   #include "hostinfo.h"
   #include "dndMsg.h"
}

/*
 *-----------------------------------------------------------------------------
 *
 * RecvMsgCB --
 *
 *      Reads msg from VMX.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RecvMsgCB(char const **result,     // OUT
          size_t *resultLen,       // OUT
          const char *name,        // IN
          const char *args,        // IN
          size_t argsSize,         // IN: Size of args
          void *clientData)        // IN
{
   DnDTransportGuestRpc *transport = (DnDTransportGuestRpc *)clientData;
   ASSERT(transport);

   /* '- 1' is to ignore empty space between command and args. */
   if ((argsSize - 1) <= 0) {
      Debug("%s: invalid argsSize\n", __FUNCTION__);
      return RpcIn_SetRetVals(result, resultLen, "invalid arg size", FALSE);
   }
   transport->RecvMsg((DnDTransportPacketHeader *)(args + 1), argsSize - 1);
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDTransportGuestRpc::DnDTransportGuestRpc --
 *
 *      Constructor of DnDTransportGuestRpc class.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

DnDTransportGuestRpc::DnDTransportGuestRpc(struct RpcIn *rpcIn, // IN
                                           const char *rpcCmd)  // IN
   : mRpcIn(rpcIn)
{
   ASSERT(rpcIn);
   ASSERT(rpcCmd);

   RpcIn_RegisterCallback(rpcIn, rpcCmd, RecvMsgCB, this);
   mRpcCmd = Util_SafeStrdup(rpcCmd);

   mSendBuf.buffer = NULL;
   mRecvBuf.buffer = NULL;
   DnD_TransportBufReset(&mSendBuf);
   DnD_TransportBufReset(&mRecvBuf);
   mSeqNum= 0;
}


/*
 *----------------------------------------------------------------------
 *
 * DnDTransportGuestRpc::~DnDTransportGuestRpc --
 *
 *      Destructor of DnDTransportGuestRpc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

DnDTransportGuestRpc::~DnDTransportGuestRpc(void)
{
   RpcIn_UnregisterCallback(mRpcIn, mRpcCmd);
   free(mRpcCmd);
   free(mSendBuf.buffer);
   free(mRecvBuf.buffer);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDTransportGuestRpc::SendMsg --
 *
 *      Sends msg to the VMX.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDTransportGuestRpc::SendMsg(uint8 *msg,    // IN
                              size_t length) // IN
{
   DnDTransportPacketHeader *packet = NULL;
   size_t packetSize;
   bool ret = FALSE;

   if (length > DNDMSG_MAX_ARGSZ) {
      Debug("%s: message is too big, quit.\n", __FUNCTION__);
      return FALSE;
   }

   Debug("%s: got message, size %"FMTSZ"u\n", __FUNCTION__, length);

   if (length <= DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE) {
      /*
       * It is a small size message, so it is not needed to buffer it in the transport
       * layer. Just put message into a packet and send it.
       */
      packetSize = DnD_TransportMsgToPacket(msg, length, mSeqNum, &packet);
   } else {
      /*
       * It is a big size message. First buffer it in the transport layer and send
       * it with multiple packets.
       */
      if (mSendBuf.buffer) {
         /*
          * There is a pending big size message. If there is no update time more than
          * DND_MAX_TRANSPORT_LATENCY_TIME, remove old message and send new message.
          * Otherwise ignore this message.
          *
          * XXX This transport implementation is just a temporary solution before VICF
          * is available. So any time there is only one pending big buffer. Any other
          * big message during this time will be dropped. Current pending buffer will
          * also be dropped if there is any error. Later on VICF should provide a
          * better transportation.
          */
         VmTimeType curTime;

         Hostinfo_GetTimeOfDay(&curTime);

         if ((curTime - mSendBuf.lastUpdateTime) < DND_MAX_TRANSPORT_LATENCY_TIME) {
            Debug("%s: got a big buffer, but there is already a pending one, quitting\n",
                  __FUNCTION__);
            return FALSE;
         }
      }
      DnD_TransportBufInit(&mSendBuf, msg, length, mSeqNum);
      packetSize = DnD_TransportBufGetPacket(&mSendBuf, &packet);
   }

   mSeqNum++;
   if (packetSize) {
      ret = SendPacket((uint8 *)packet, packetSize);
   }
   free(packet);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDTransportGuestRpc::SendPacket --
 *
 *      Sends packet to the VMX.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

bool
DnDTransportGuestRpc::SendPacket(uint8 *packet,     // IN
                                 size_t packetSize) // IN
{
   char *rpc = NULL;
   size_t rpcSize = 0;
   size_t nrWritten = 0;
   bool ret = FALSE;

   if (packetSize == 0 || packetSize > DND_MAX_TRANSPORT_PACKET_SIZE) {
      Debug("%s: invalid packet\n", __FUNCTION__);
      return FALSE;
   }

   rpcSize = strlen(mRpcCmd) + 1 + packetSize;
   rpc = (char *)Util_SafeMalloc(rpcSize);
   nrWritten = Str_Sprintf(rpc, rpcSize, "%s ", mRpcCmd);
   ASSERT(nrWritten + packetSize <= rpcSize);
   memcpy(rpc + nrWritten, packet, packetSize);

   ret = (TRUE == RpcOut_SendOneRaw(rpc, rpcSize, NULL, NULL));
   
   if (!ret) {
      Debug("%s: failed to send msg to host\n", __FUNCTION__);
   }

   free(rpc);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnDTransportGuestRpc::RecvMsg --
 *
 *      Receives packet from VMX.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DnDTransportGuestRpc::RecvMsg(DnDTransportPacketHeader *packet, // IN
                              size_t packetSize)                // IN
{
   if (packetSize <= 0 ||
       packetSize != (packet->payloadSize + DND_TRANSPORT_PACKET_HEADER_SIZE) ||
       packetSize > DND_MAX_TRANSPORT_PACKET_SIZE) {
      Debug("%s: Received invalid data.\n", __FUNCTION__);
      return;
   }

   Debug("%s: received data, size %"FMTSZ"u.\n", __FUNCTION__, packetSize);

   switch (packet->type) {
   case DND_TRANSPORT_PACKET_TYPE_SINGLE:
      if (packet->payloadSize != packet->totalSize) {
         Debug("%s: received invalid packet.\n", __FUNCTION__);
         return;
      }
      recvMsgChanged.emit(packet->payload,
                          packet->payloadSize);
      break;
   case DND_TRANSPORT_PACKET_TYPE_REQUEST:
      {
         DnDTransportPacketHeader *replyPacket = NULL;
         size_t replyPacketSize;

         /* Validate received packet. */
         if (packet->payloadSize ||
             packet->seqNum != mSendBuf.seqNum ||
             packet->offset != mSendBuf.offset) {
            Debug("%s: received packet does not match local buffer.\n", __FUNCTION__);
            return;
         }

         replyPacketSize = DnD_TransportBufGetPacket(&mSendBuf, &replyPacket);

         if (!replyPacketSize) {
            /*
             * Not needed to reset mSendBuf because DnD_TransportBufGetPacket already
             * did that.
             */
            Debug("%s: DnD_TransportBufGetPacket failed.\n", __FUNCTION__);
            return;
         }
  
         if (!SendPacket((uint8 *)replyPacket, replyPacketSize) ||
             mSendBuf.offset == mSendBuf.totalSize) {
            /* Reset mSendBuf if whole buffer is sent or there is any error. */
            DnD_TransportBufReset(&mSendBuf);
         }

         free(replyPacket);

         break;
      }
   case DND_TRANSPORT_PACKET_TYPE_PAYLOAD:
      if (!DnD_TransportBufAppendPacket(&mRecvBuf, packet, packetSize)) {
         Debug("%s: DnD_TransportBufAppendPacket failed.\n", __FUNCTION__);
         return;
      }

      if (mRecvBuf.offset == mRecvBuf.totalSize) {
         /* Received all packets for the messge. */
         recvMsgChanged.emit(mRecvBuf.buffer, mRecvBuf.totalSize);
         DnD_TransportBufReset(&mRecvBuf);
      } else {
         /* Send request for next packet. */
         DnDTransportPacketHeader *replyPacket = NULL;
         size_t replyPacketSize;

         replyPacketSize = DnD_TransportReqPacket(&mRecvBuf, &replyPacket);

         if (!replyPacketSize) {
            Debug("%s: DnD_TransportReqPacket failed.\n", __FUNCTION__);
            return;
         }
         
         if (!SendPacket((uint8 *)replyPacket, replyPacketSize)) {
            DnD_TransportBufReset(&mRecvBuf);
         }
         free(replyPacket);      
      }
      break;
   default:
      Debug("%s: unknown packet.\n", __FUNCTION__);
      break;
   }
}

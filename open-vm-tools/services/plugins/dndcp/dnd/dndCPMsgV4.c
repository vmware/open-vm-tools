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
 * @dndCPMsgV4.c --
 *
 * Helper functions for DnDCPMsgV4.
 */

#include "vm_assert.h"
#include "dnd.h"
#include "dndClipboard.h"
#include "dndCPMsgV4.h"
#include "util.h"


/**
 * Check if received packet is valid or not.
 *
 * @param[in] packet
 * @param[in] packetSize
 *
 * @return TRUE if the packet is valid, FALSE otherwise.
 */

static Bool
DnDCPMsgV4IsPacketValid(const uint8 *packet,
                        size_t packetSize)
{
   DnDCPMsgHdrV4 *msgHdr = NULL;
   ASSERT(packet);

   if (packetSize < DND_CP_MSG_HEADERSIZE_V4 ||
       packetSize > DND_MAX_TRANSPORT_PACKET_SIZE) {
      return FALSE;
   }

   msgHdr = (DnDCPMsgHdrV4 *)packet;

   /* Payload size is not valid. */
   if (msgHdr->payloadSize > DND_CP_PACKET_MAX_PAYLOAD_SIZE_V4) {
      return FALSE;
   }

   /* Payload size plus header size should be equal to packet size. */
   if (msgHdr->payloadSize + DND_CP_MSG_HEADERSIZE_V4 != packetSize) {
      return FALSE;
   }

   /* Binary size is not valid. */
   if (msgHdr->binarySize > DND_CP_MSG_MAX_BINARY_SIZE_V4) {
      return FALSE;
   }

   /*
    * For Workstation/Fusion Payload should be smaller than the whole binary
    * and should not be put beyond the binary tail.
    *
    * binarySize should be smaller than DND_CP_MSG_MAX_BINARY_SIZE_V4, so that
    * integer overflow is not possible since DND_CP_MSG_MAX_BINARY_SIZE_V4 * 2
    * is guaranteed to be less than MAX_UINT32. Horizon removes this limitation
    */
#ifndef VMX86_HORIZON_VIEW
   ASSERT_ON_COMPILE(DND_CP_MSG_MAX_BINARY_SIZE_V4 <= MAX_UINT32 / 2);
#endif
   if (msgHdr->payloadOffset > msgHdr->binarySize ||
       msgHdr->payloadSize > msgHdr->binarySize ||
       msgHdr->payloadOffset + msgHdr->payloadSize > msgHdr->binarySize) {
      return FALSE;
   }

   return TRUE;
}


/**
 * Initialize the DnDCPMsgV4.
 *
 * @param[in/out] msg DnDCPMsgV4 to be initialized.
 */

void
DnDCPMsgV4_Init(DnDCPMsgV4 *msg)
{
   ASSERT(msg);
   memset(msg, 0, sizeof *msg);
}

/**
 * Destroy the DnDCPMsgV4.
 *
 * @param[in/out] msg DnDCPMsgV4 to be destroyed.
 */

void
DnDCPMsgV4_Destroy(DnDCPMsgV4 *msg)
{
   if (msg) {
      free(msg->binary);
      DnDCPMsgV4_Init(msg);
   }
}


/**
 * Check the packet type.
 *
 * @param[in] packet
 * @param[in] packetSize
 * @param[in] allowed max packet Size
 *
 * @return DnDCPMsgPacketType
 */

DnDCPMsgPacketType DnDCPMsgV4_GetPacketType(const uint8 *packet,
                                            size_t packetSize,
                                            const uint32 maxPacketPayloadSize)
{
   DnDCPMsgHdrV4 *msgHdr = NULL;
   ASSERT(packet);

   if (!DnDCPMsgV4IsPacketValid(packet, packetSize)) {
      return DND_CP_MSG_PACKET_TYPE_INVALID;
   }

   msgHdr = (DnDCPMsgHdrV4 *)packet;
   if (msgHdr->binarySize <= maxPacketPayloadSize) {
      return DND_CP_MSG_PACKET_TYPE_SINGLE;
   }

   if (0 == msgHdr->payloadOffset) {
      return DND_CP_MSG_PACKET_TYPE_MULTIPLE_NEW;
   }

   if (msgHdr->payloadOffset + msgHdr->payloadSize == msgHdr->binarySize) {
      return DND_CP_MSG_PACKET_TYPE_MULTIPLE_END;
   }

   return DND_CP_MSG_PACKET_TYPE_MULTIPLE_CONTINUE;
}


/**
 * Serialize the msg to packet.
 *
 * @param[in/out] msg DnDCPMsgV4 to be serialized from.
 * @param[out] packet DnDCPMsgV4 to be serialized to.
 * @param[out] packetSize serialized packet size.
 *
 * @return TRUE if succeed, FALSE otherwise.
 */

Bool
DnDCPMsgV4_Serialize(DnDCPMsgV4 *msg,
                     uint8 **packet,
                     size_t *packetSize)
{
   return DnDCPMsgV4_SerializeWithInputPayloadSizeCheck(
      msg, packet, packetSize, DND_CP_PACKET_MAX_PAYLOAD_SIZE_V4);
}


Bool
DnDCPMsgV4_SerializeWithInputPayloadSizeCheck(DnDCPMsgV4 *msg,
                                              uint8 **packet,
                                              size_t *packetSize,
                                              const uint32 maxPacketPayloadSize)
{
   size_t payloadSize = 0;

   ASSERT(msg);
   ASSERT(packet);
   ASSERT(packetSize);
   ASSERT(msg->hdr.binarySize >= msg->hdr.payloadOffset);

   if (msg->hdr.binarySize <= maxPacketPayloadSize) {
      /*
       * One single packet is enough for the message. For short message, the
       * payloadOffset should always be 0.
       */
      ASSERT(msg->hdr.payloadOffset == 0);
      payloadSize = msg->hdr.binarySize;
   } else {
      /* For big message, payloadOffset means binary size we already sent out. */
      if (msg->hdr.binarySize - msg->hdr.payloadOffset > maxPacketPayloadSize) {
         payloadSize = maxPacketPayloadSize;
      } else {
         payloadSize = msg->hdr.binarySize - msg->hdr.payloadOffset;
      }
   }

   *packetSize = DND_CP_MSG_HEADERSIZE_V4 + payloadSize;
   *packet = Util_SafeMalloc(*packetSize);
   memcpy(*packet, msg, DND_CP_MSG_HEADERSIZE_V4);
   if (payloadSize > 0) {
      memcpy(*packet + DND_CP_MSG_HEADERSIZE_V4,
             msg->binary + msg->hdr.payloadOffset,
             payloadSize);
   }
   ((DnDCPMsgHdrV4 *)(*packet))->payloadSize = payloadSize;
   /* Next DnDCPMsgV4_Serialize will use this payloadOffset to get unsent binary. */
   msg->hdr.payloadOffset += payloadSize;
   return TRUE;
}


/**
 * Unserialize the packet to DnDCPMsgV4 for short messsage.
 *
 * @param[in/out] msg DnDCPMsgV4 to be unserialized to.
 * @param[in] packet DnDCPMsgV4 to be unserialized from.
 * @param[in] packetSize
 *
 * @return TRUE if succeed, FALSE otherwise.
 */

Bool
DnDCPMsgV4_UnserializeSingle(DnDCPMsgV4 *msg,
                             const uint8 *packet,
                             size_t packetSize)
{
   DnDCPMsgHdrV4 *msgHdr = NULL;
   ASSERT(msg);
   ASSERT(packet);

   if (!DnDCPMsgV4IsPacketValid(packet, packetSize)) {
      return FALSE;
   }

   msgHdr = (DnDCPMsgHdrV4 *)packet;

   /* Offset should be 0 for short message. */
   if (msgHdr->payloadOffset != 0) {
      return FALSE;
   }

   memcpy(msg, msgHdr, DND_CP_MSG_HEADERSIZE_V4);

   if (msg->hdr.binarySize != 0) {
      msg->binary = Util_SafeMalloc(msg->hdr.binarySize);

      memcpy(msg->binary,
             packet + DND_CP_MSG_HEADERSIZE_V4,
             msg->hdr.payloadSize);
      msg->hdr.payloadOffset = msg->hdr.payloadSize;
   }
   return TRUE;
}


/**
 * Unserialize the packet to DnDCPMsgV4 for big messsage.
 *
 * @param[in/out] msg DnDCPMsgV4 to be unserialized to.
 * @param[in] packet DnDCPMsgV4 to be unserialized from.
 * @param[in] packetSize
 *
 * @return TRUE if succeed, FALSE otherwise.
 */

Bool
DnDCPMsgV4_UnserializeMultiple(DnDCPMsgV4 *msg,
                               const uint8 *packet,
                               size_t packetSize)
{
   DnDCPMsgHdrV4 *msgHdr = NULL;
   ASSERT(msg);
   ASSERT(packet);

   if (!DnDCPMsgV4IsPacketValid(packet, packetSize)) {
      return FALSE;
   }

   msgHdr = (DnDCPMsgHdrV4 *)packet;

   /*
    * For each session, there is at most 1 big packet. If the received
    * sessionId is different with buffered one, the received packet is for
    * another another new packet. Destroy old buffered packet.
    */
   if (msg->hdr.sessionId != msgHdr->sessionId) {
      DnDCPMsgV4_Destroy(msg);
   }

   if (msg->binary == NULL) {
      /* New packet */

      /* Offset should be 0 for new packet. */
      if (msgHdr->payloadOffset != 0) {
         return FALSE;
      }
      msg->hdr = *msgHdr;
      msg->hdr.payloadSize = 0; // unused; initialize to zero for safety
      msg->binary = Util_SafeMalloc(msg->hdr.binarySize);
   } else {
      /* Existing buffered packet */

      /* Binary size should match.
       * DnDCPMsgV4IsPacketValid() can only validate packets individually.
       * For multiple packets in a session, it requires that all packets
       * have the same binarySize.
       *
       * We alloc buffer only when the first packet arrives, using the
       * binarySize from the first packet. The binarySize in the following
       * packets must be the same as the first packet.
       */
      if (msg->hdr.binarySize != msgHdr->binarySize) {
         return FALSE;
      }

      /* Payload offset must match expected offset after earlier payload */
      if (msg->hdr.payloadOffset != msgHdr->payloadOffset) {
         return FALSE;
      }
   }

   /* msg->hdr.payloadOffset is used as received binary size. */
   memcpy(msg->binary + msg->hdr.payloadOffset,
          packet + DND_CP_MSG_HEADERSIZE_V4,
          msgHdr->payloadSize);
   msg->hdr.payloadOffset += msgHdr->payloadSize;
   return TRUE;
}


/**
 * Map a command to a string.
 *
 * @param[in] cmd the DnD V4 command
 *
 * @return a valid command string if the command is valid, "invalid command"
 *         otherwise.
 */

const char *
DnDCPMsgV4_LookupCmd(uint32 cmd)
{
   static const struct {
      uint32 cmd;
      const char *cmdStr;
   } cmdStringTable[] = {
      { DNDCP_CMD_PING,                "DNDCP_CMD_PING" },
      { DNDCP_CMD_PING_REPLY,          "DNDCP_CMD_PING_REPLY" },
      { DNDCP_CMD_REQUEST_NEXT,        "DNDCP_CMD_REQUEST_NEXT" },
      { DNDCP_CMP_REPLY,               "DNDCP_CMP_REPLY" },
      { DNDCP_CMD_TEST_BIG_BINARY,     "DNDCP_CMD_TEST_BIG_BINARY" },
      { DNDCP_CMD_TEST_BIG_BINARY_REPLY, "DNDCP_CMD_TEST_BIG_BINARY_REPLY" },

      { DND_CMD_DEST_DRAG_ENTER,       "DND_CMD_DEST_DRAG_ENTER" },
      { DND_CMD_DEST_DRAG_ENTER_REPLY, "DND_CMD_DEST_DRAG_ENTER_REPLY" },
      { DND_CMD_DEST_SEND_CLIPBOARD,   "DND_CMD_DEST_SEND_CLIPBOARD" },
      { DND_CMD_DEST_DRAG_LEAVE,       "DND_CMD_DEST_DRAG_LEAVE" },
      { DND_CMD_DEST_DROP,             "DND_CMD_DEST_DROP" },
      { DND_CMD_SRC_DRAG_BEGIN,        "DND_CMD_SRC_DRAG_BEGIN" },
      { DND_CMD_SRC_DRAG_BEGIN_DONE,   "DND_CMD_SRC_DRAG_BEGIN_DONE" },
      { DND_CMD_SRC_DROP,              "DND_CMD_SRC_DROP" },
      { DND_CMD_SRC_DROP_DONE,         "DND_CMD_SRC_DROP_DONE" },
      { DND_CMD_SRC_CANCEL,            "DND_CMD_SRC_CANCEL" },
      { DND_CMD_PRIV_DRAG_ENTER,       "DND_CMD_PRIV_DRAG_ENTER" },
      { DND_CMD_PRIV_DRAG_LEAVE,       "DND_CMD_PRIV_DRAG_LEAVE" },
      { DND_CMD_PRIV_DROP,             "DND_CMD_PRIV_DROP" },
      { DND_CMD_MOVE_MOUSE,            "DND_CMD_MOVE_MOUSE" },
      { DND_CMD_UPDATE_FEEDBACK,       "DND_CMD_UPDATE_FEEDBACK" },
      { DND_CMD_REQUEST_FILES,         "DND_CMD_REQUEST_FILES" },
      { DND_CMD_GET_FILES_DONE,        "DND_CMD_GET_FILES_DONE" },
      { DND_CMD_SEND_FILES_DONE,       "DND_CMD_SEND_FILES_DONE" },
      { DND_CMD_QUERY_EXITING,         "DND_CMD_QUERY_EXITING" },
      { DND_CMD_DRAG_NOT_PENDING,      "DND_CMD_DRAG_NOT_PENDING" },
      { DND_CMD_UPDATE_UNITY_DET_WND,  "DND_CMD_UPDATE_UNITY_DET_WND" },

      { CP_CMD_REQUEST_CLIPBOARD,      "CP_CMD_REQUEST_CLIPBOARD" },
      { CP_CMD_REQUEST_FILES,          "CP_CMD_REQUEST_FILES" },
      { CP_CMD_RECV_CLIPBOARD,         "CP_CMD_RECV_CLIPBOARD" },
      { CP_CMD_SEND_CLIPBOARD,         "CP_CMD_SEND_CLIPBOARD" },
      { CP_CMD_GET_FILES_DONE,         "CP_CMD_GET_FILES_DONE" },
      { CP_CMD_SEND_FILES_DONE,        "CP_CMD_SEND_FILES_DONE" },

      { FT_CMD_HGFS_REQUEST,           "FT_CMD_HGFS_REQUEST" },
      { FT_CMD_HGFS_REPLY,             "FT_CMD_HGFS_REPLY" },
      { FT_CMD_UPDATE_PROGRESS,        "FT_CMD_UPDATE_PROGRESS" },
      { FT_CMD_PROGRESS_REPLY,         "FT_CMD_PROGRESS_REPLY" },
   };
   size_t i;

   for (i = 0; i < ARRAYSIZE(cmdStringTable); i++) {
      if (cmdStringTable[i].cmd == cmd) {
         return cmdStringTable[i].cmdStr;
      }
   }
   return "invalid command";
}


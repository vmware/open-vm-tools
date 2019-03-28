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
 * @dndCPMsgV4.h --
 *
 * DnDCPMsgV4 represents version 4 rpc message/packet for DnD/Copy/Paste.
 * DnD/CP message/packet is used to pass command between 2 spots (between host
 * and controller, or between controller and guest). The message/packet is
 * cross-platform. In sender side, RPC layer should construct dnd/cp message,
 * serialize it to packet, and transport layer will send the packet to another
 * side. In receiver side, transport layer will dispatch the packet to right
 * RPC, and the RPC should unpack the packet into message and send up to common
 * state machine layer.
 */

#ifndef DND_CP_MSG_V4_H
#define DND_CP_MSG_V4_H

#define INCLUDE_ALLOW_USERLEVEL

#include "includeCheck.h"
#include "vm_basic_types.h"

/*
 * These commands will be used as cross-platform DnD/CopyPaste communication.
 * All common commands start from 0. DnD commands start from 1000. CopyPaste
 * commands start from 2000. FileTransfer commands start from 3000.
 */

/* Commands for all channels. */
typedef enum {
   DNDCP_CMD_INVALID = 0,
   /*
    * These 2 commands are used right after any communication channel is
    * established to exchange some initial information such as version,
    * capability.
    */
   DNDCP_CMD_PING,
   DNDCP_CMD_PING_REPLY,
   /* This command is used for big binary transfer. */
   DNDCP_CMD_REQUEST_NEXT,
   /*
    * This command is used for general reply for any command. The reply is
    * optional. Most commands do not need it.
    */
   DNDCP_CMP_REPLY,
   /* These 2 commands are used for testing big binary transport. */
   DNDCP_CMD_TEST_BIG_BINARY,
   DNDCP_CMD_TEST_BIG_BINARY_REPLY,
} DnDCPCmdV4;

/* DnD Commands. */
typedef enum {
   /* For DnD destination. */
   DND_CMD_DEST_DRAG_ENTER = 1000,
   DND_CMD_DEST_DRAG_ENTER_REPLY,
   DND_CMD_DEST_SEND_CLIPBOARD,
   DND_CMD_DEST_DRAG_LEAVE,
   DND_CMD_DEST_DROP,

   /* For DnD source. */
   DND_CMD_SRC_DRAG_BEGIN,
   DND_CMD_SRC_DRAG_BEGIN_DONE,
   DND_CMD_SRC_DROP,
   DND_CMD_SRC_DROP_DONE,
   DND_CMD_SRC_CANCEL,

   /* For private DnD. */
   DND_CMD_PRIV_DRAG_ENTER,
   DND_CMD_PRIV_DRAG_LEAVE,
   DND_CMD_PRIV_DROP,

   /* For some common DnD operation. */
   DND_CMD_MOVE_MOUSE,
   DND_CMD_UPDATE_FEEDBACK,
   DND_CMD_REQUEST_FILES,
   DND_CMD_GET_FILES_DONE,
   DND_CMD_SEND_FILES_DONE,
   DND_CMD_QUERY_EXITING,
   DND_CMD_DRAG_NOT_PENDING,
   DND_CMD_UPDATE_UNITY_DET_WND,
   DND_CMD_DEST_CANCEL
} DnDCmdV4;

/* Copy/Paste commands. */
typedef enum {
   CP_CMD_REQUEST_CLIPBOARD = 2000,
   CP_CMD_REQUEST_FILES,
   CP_CMD_RECV_CLIPBOARD,
   CP_CMD_SEND_CLIPBOARD,
   CP_CMD_GET_FILES_DONE,
   CP_CMD_SEND_FILES_DONE,
} CopyPasteCmdV4;

/* File transfer commands. */
typedef enum {
   FT_CMD_HGFS_REQUEST = 3000,
   FT_CMD_HGFS_REPLY,
   FT_CMD_UPDATE_PROGRESS,
   FT_CMD_PROGRESS_REPLY
} FileTransferCmdV4;

/* Message types. */
typedef enum DnDCPMsgType {
   DND_CP_MSG_TYPE_INVALID = 0,
   DND_CP_MSG_TYPE_DND,
   DND_CP_MSG_TYPE_CP,
   DND_CP_MSG_TYPE_FT
} DnDCPMsgType;

/* Message source. */
typedef enum DnDCPMsgSrc {
   DND_CP_MSG_SRC_INVALID = 0,
   DND_CP_MSG_SRC_HOST,
   DND_CP_MSG_SRC_CONTROLLER,
   DND_CP_MSG_SRC_GUEST
} DnDCPMsgSrc;

/* Command reply status. */
typedef enum DnDCPMsgStatus {
   DND_CP_MSG_STATUS_SUCCESS,
   DND_CP_MSG_STATUS_ERROR,
   DND_CP_MSG_STATUS_CANCEL,
   DND_CP_MSG_STATUS_BUSY,
   DND_CP_MSG_STATUS_ACCEPTED,
   DND_CP_MSG_STATUS_INVALID_PACKET,
   DND_CP_MSG_STATUS_INVALID_SESSION_ID,
   DND_CP_MSG_STATUS_INVALID_FORMAT,
} DnDCPMsgStatus;

/* Packet types. */
typedef enum DnDCPMsgPacketType {
   DND_CP_MSG_PACKET_TYPE_SINGLE,
   DND_CP_MSG_PACKET_TYPE_MULTIPLE_NEW,
   DND_CP_MSG_PACKET_TYPE_MULTIPLE_CONTINUE,
   DND_CP_MSG_PACKET_TYPE_MULTIPLE_END,
   DND_CP_MSG_PACKET_TYPE_INVALID
} DnDCPMsgPacketType;

/*
 * Definitions for DnD/CP capabilities. DND_CP_CAP_VALID is used to specify if
 * the capability itself is valid or not.
 */
#define DND_CP_CAP_VALID            (1 << 0)
#define DND_CP_CAP_DND              (1 << 1)
#define DND_CP_CAP_CP               (1 << 2)
#define DND_CP_CAP_PLAIN_TEXT_DND   (1 << 3)
#define DND_CP_CAP_PLAIN_TEXT_CP    (1 << 4)
#define DND_CP_CAP_RTF_DND          (1 << 5)
#define DND_CP_CAP_RTF_CP           (1 << 6)
#define DND_CP_CAP_IMAGE_DND        (1 << 7)
#define DND_CP_CAP_IMAGE_CP         (1 << 8)
#define DND_CP_CAP_FILE_DND         (1 << 9)
#define DND_CP_CAP_FILE_CP          (1 << 10)
#define DND_CP_CAP_FILE_CONTENT_DND (1 << 11)
#define DND_CP_CAP_FILE_CONTENT_CP  (1 << 12)
#define DND_CP_CAP_ACTIVE_CP        (1 << 13)
#define DND_CP_CAP_GUEST_PROGRESS   (1 << 14)
#define DND_CP_CAP_BIG_BUFFER       (1 << 15)

#define DND_CP_CAP_FORMATS_CP       (DND_CP_CAP_PLAIN_TEXT_CP   | \
                                     DND_CP_CAP_RTF_CP          | \
                                     DND_CP_CAP_IMAGE_CP        | \
                                     DND_CP_CAP_FILE_CP         | \
                                     DND_CP_CAP_FILE_CONTENT_CP)

#define DND_CP_CAP_FORMATS_DND      (DND_CP_CAP_PLAIN_TEXT_DND  | \
                                     DND_CP_CAP_RTF_DND         | \
                                     DND_CP_CAP_IMAGE_DND       | \
                                     DND_CP_CAP_FILE_DND        | \
                                     DND_CP_CAP_FILE_CONTENT_DND)

#define DND_CP_CAP_FORMATS_ALL      (DND_CP_CAP_FORMATS_CP      | \
                                     DND_CP_CAP_FORMATS_DND)

/*
 * Header definition for DnD version 4 packet. Any DnD version 4 packet has 2
 * parts: fixed header and payload. payload is optional.
 */
typedef
#include "vmware_pack_begin.h"
struct DnDCPMsgHdrV4 {
   uint32 cmd;             /* DnD/CP message command. */
   uint32 type;            /* DnD/CP message type. */
   uint32 src;             /* Message sender. */
   uint32 sessionId;       /* DnD/CP session ID. */
   uint32 status;          /* Status for last operation. */
   uint32 param1;          /* Optional parameter. Optional. */
   uint32 param2;          /* Optional parameter. Optional. */
   uint32 param3;          /* Optional parameter. Optional. */
   uint32 param4;          /* Optional parameter. Optional. */
   uint32 param5;          /* Optional parameter. Optional. */
   uint32 param6;          /* Optional parameter. Optional. */
   uint32 binarySize;      /* Binary size. */
   uint32 payloadOffset;   /* Payload offset. */
   uint32 payloadSize;     /* Payload size. */
}
#include "vmware_pack_end.h"
DnDCPMsgHdrV4;

/* Some important definitions for DnDCPMsgV4. */
#define DND_CP_MSG_HEADERSIZE_V4 (sizeof (DnDCPMsgHdrV4))
#define DND_CP_PACKET_MAX_PAYLOAD_SIZE_V4 (DND_MAX_TRANSPORT_PACKET_SIZE - \
                                           DND_CP_MSG_HEADERSIZE_V4)
#ifdef VMX86_HORIZON_VIEW
/*
 * Horizon has no hard limit, but the size field is type of uint32,
 * 4G-1(0xffffffff) is the maximum size represented.
 */
#define DND_CP_MSG_MAX_BINARY_SIZE_V4 0xffffffff
#else
// Workstation/fusion have hard limit in size(4M) of DnD Msg, refer to dnd.h
#define DND_CP_MSG_MAX_BINARY_SIZE_V4 (1 << 22)
#endif

/* DnD version 4 message. */
typedef struct DnDCPMsgV4 {
   DnDCPMsgHdrV4 hdr;
   uint32 addrId;
   uint8 *binary;
} DnDCPMsgV4;

#if !defined(SWIG)
void DnDCPMsgV4_Init(DnDCPMsgV4 *msg);
void DnDCPMsgV4_Destroy(DnDCPMsgV4 *msg);
DnDCPMsgPacketType DnDCPMsgV4_GetPacketType(const uint8 *packet,
                                            size_t packetSize,
                                            const uint32 maxPacketPayloadSize);
Bool DnDCPMsgV4_Serialize(DnDCPMsgV4 *msg,
                          uint8 **packet,
                          size_t *packetSize);
Bool DnDCPMsgV4_SerializeWithInputPayloadSizeCheck(DnDCPMsgV4 *msg,
   uint8 **packet, size_t *packetSize, const uint32 maxPacketPayloadSize);
Bool DnDCPMsgV4_UnserializeSingle(DnDCPMsgV4 *msg,
                                  const uint8 *packet,
                                  size_t packetSize);
Bool DnDCPMsgV4_UnserializeMultiple(DnDCPMsgV4 *msg,
                                    const uint8 *packet,
                                    size_t packetSize);
const char *DnDCPMsgV4_LookupCmd(uint32 cmd);
#endif
#endif // DND_CP_MSG_V4_H

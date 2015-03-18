/*********************************************************
 * Copyright (C) 2012,2014 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmci_sockets_packet.h --
 *
 *    Definition of vSockets packet format, constants, and types.
 */

#ifndef _VMCI_SOCKETS_PACKET_H_
#define _VMCI_SOCKETS_PACKET_H_

#include "vmci_defs.h"
#include "vmci_call_defs.h"

/*
 * STREAM control packets.
 */

/* If the packet format changes in a release then this should change too. */
#define VSOCK_PACKET_VERSION 1

/* The resource ID on which control packets are sent. */
#define VSOCK_PACKET_RID 1

/*
 * Assert that the given packet is valid.
 * We check that the two original reserved fields equal zero because the
 * version of the common code that shipped with ESX 4.0 and WS 6.5 did so and
 * will return a RST packet if they aren't set that way. For newer packet
 * types added after that release we don't do this.
 */
#define VSOCK_PACKET_ASSERT(_p)                      \
   do {                                              \
      ASSERT((_p));                                  \
      ASSERT((_p)->type < VSOCK_PACKET_TYPE_MAX);    \
      if ((_p)->type < VSOCK_PACKET_TYPE_REQUEST2) { \
         ASSERT(0 == (_p)->proto);                   \
         ASSERT(0 == (_p)->_reserved2);              \
      }                                              \
   } while(0)

typedef enum VSockPacketType {
   VSOCK_PACKET_TYPE_INVALID = 0,   // Invalid type.
   VSOCK_PACKET_TYPE_REQUEST,       // Connection request (WR/WW/READ/WRITE)
   VSOCK_PACKET_TYPE_NEGOTIATE,     // Connection negotiate.
   VSOCK_PACKET_TYPE_OFFER,         // Connection offer queue pair.
   VSOCK_PACKET_TYPE_ATTACH,        // Connection attach.
   VSOCK_PACKET_TYPE_WROTE,         // Wrote data to queue pair.
   VSOCK_PACKET_TYPE_READ,          // Read data from queue pair.
   VSOCK_PACKET_TYPE_RST,           // Reset.
   VSOCK_PACKET_TYPE_SHUTDOWN,      // Shutdown the connection.
   VSOCK_PACKET_TYPE_WAITING_WRITE, // Notify peer we are waiting to write.
   VSOCK_PACKET_TYPE_WAITING_READ,  // Notify peer we are waiting to read.
   VSOCK_PACKET_TYPE_REQUEST2,      // Connection request (new proto flags)
   VSOCK_PACKET_TYPE_NEGOTIATE2,    // Connection request (new proto flags)
   VSOCK_PACKET_TYPE_MAX            // Last message.
} VSockPacketType;

typedef uint16 VSockProtoVersion;
#define VSOCK_PROTO_INVALID        0        // Invalid protocol version.
#define VSOCK_PROTO_PKT_ON_NOTIFY (1 << 0)  // Queuepair inspection proto.

#define VSOCK_PROTO_ALL_SUPPORTED (VSOCK_PROTO_PKT_ON_NOTIFY)

typedef struct VSockWaitingInfo {
   uint64 generation; // Generation of the queue.
   uint64 offset;     // Offset within the queue.
} VSockWaitingInfo;

/*
 * Control packet type for STREAM sockets.  DGRAMs have no control packets
 * nor special packet header for data packets, they are just raw VMCI DGRAM
 * messages.  For STREAMs, control packets are sent over the control channel
 * while data is written and read directly from queue pairs with no packet
 * format.
 */
typedef struct VSockPacket {
   VMCIDatagram dg;          // Datagram header.
   uint8 version;            // Version.
   uint8 type;               // Type of message.
   VSockProtoVersion proto;  // Supported proto versions in CONNECT2 and
                             // NEGOTIATE2. 0 otherwise.
   uint32 srcPort;           // Source port.
   uint32 dstPort;           // Destination port.
   uint32 _reserved2;        // Reserved.
   union {
      uint64 size;           // Size of queue pair for request/negotiation.
      uint64 mode;           // Mode of shutdown for shutdown.
      VMCIHandle handle;     // Queue pair handle once size negotiated.
      VSockWaitingInfo wait; // Information provided for wait notifications.
   } u;
} VSockPacket;

/*
 * Size assertions.
 */

MY_ASSERTS(VSockSeqPacketAsserts,
   ASSERT_ON_COMPILE(sizeof (VSockPacket) == 56);
)

#endif // _VMCI_SOCKETS_PACKET_H_

/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vsockPacket.h --
 *
 *    Packet constants, types and functions.
 */

#ifndef _VSOCK_PACKET_H_
#define _VSOCK_PACKET_H_

#if defined(_WIN32) || defined(VMKERNEL) || defined(__APPLE__)
# include "vsockOSInt.h"
#else
# define VSockOS_ClearMemory(_dst, _sz)   memset(_dst, 0, _sz)
# define VSockOS_Memcpy(_dst, _src, _sz)  memcpy(_dst, _src, _sz)
#endif


/* If the packet format changes in a release then this should change too. */
#define VSOCK_PACKET_VERSION 1

/* The resource ID on which control packets are sent. */
#define VSOCK_PACKET_RID 1

/* Assert that the given packet is valid. */
#define VSOCK_PACKET_ASSERT(_p)                 \
   ASSERT((_p));                                \
   ASSERT((_p)->type < VSOCK_PACKET_TYPE_MAX);  \
   ASSERT(0 == (_p)->_reserved1);               \
   ASSERT(0 == (_p)->_reserved2)


typedef enum VSockPacketType {
   VSOCK_PACKET_TYPE_INVALID = 0,   // Invalid type.
   VSOCK_PACKET_TYPE_REQUEST,       // Connection request.
   VSOCK_PACKET_TYPE_NEGOTIATE,     // Connection negotiate.
   VSOCK_PACKET_TYPE_OFFER,         // Connection offer queue pair.
   VSOCK_PACKET_TYPE_ATTACH,        // Connection attach.
   VSOCK_PACKET_TYPE_WROTE,         // Wrote data to queue pair.
   VSOCK_PACKET_TYPE_READ,          // Read data from queue pair.
   VSOCK_PACKET_TYPE_RST,           // Reset.
   VSOCK_PACKET_TYPE_SHUTDOWN,      // Shutdown the connection.
   VSOCK_PACKET_TYPE_WAITING_WRITE, // Notify peer we are waiting to write.
   VSOCK_PACKET_TYPE_WAITING_READ,  // Notify peer we are waiting to read.
   VSOCK_PACKET_TYPE_MAX            // Last message.
} VSockPacketType;

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
   uint16 _reserved1;        // Reserved.
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


MY_ASSERTS(VSockPacketAsserts,
   ASSERT_ON_COMPILE(sizeof (VSockPacket) == 56);
)


/*
 *-----------------------------------------------------------------------------
 *
 * VSockPacket_Init --
 *
 *      Initialize the given packet.  The packet version is set and the fields
 *      are filled out.  Reserved fields are cleared.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VSockPacket_Init(VSockPacket *pkt,        // OUT
                 struct sockaddr_vm *src, // IN
                 struct sockaddr_vm *dst, // IN
                 uint8 type,              // IN
                 uint64 size,             // IN
                 uint64 mode,             // IN
                 VSockWaitingInfo *wait,  // IN
                 VMCIHandle handle)       // IN
{
   ASSERT(pkt);
   VSOCK_ADDR_NOFAMILY_ASSERT(src);
   VSOCK_ADDR_NOFAMILY_ASSERT(dst);

   pkt->dg.src = VMCI_MAKE_HANDLE(src->svm_cid, VSOCK_PACKET_RID);
   pkt->dg.dst = VMCI_MAKE_HANDLE(dst->svm_cid, VSOCK_PACKET_RID);
   pkt->dg.payloadSize = sizeof *pkt - sizeof pkt->dg;
   pkt->version = VSOCK_PACKET_VERSION;
   pkt->type = type;
   pkt->srcPort = src->svm_port;
   pkt->dstPort = dst->svm_port;
   VSockOS_ClearMemory(&pkt->_reserved1, sizeof pkt->_reserved1);
   VSockOS_ClearMemory(&pkt->_reserved2, sizeof pkt->_reserved2);

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_INVALID:
      pkt->u.size = 0;
      break;

   case VSOCK_PACKET_TYPE_REQUEST:
   case VSOCK_PACKET_TYPE_NEGOTIATE:
      pkt->u.size = size;
      break;

   case VSOCK_PACKET_TYPE_OFFER:
   case VSOCK_PACKET_TYPE_ATTACH:
      pkt->u.handle = handle;
      break;

   case VSOCK_PACKET_TYPE_WROTE:
   case VSOCK_PACKET_TYPE_READ:
   case VSOCK_PACKET_TYPE_RST:
      pkt->u.size = 0;
      break;

   case VSOCK_PACKET_TYPE_SHUTDOWN:
      pkt->u.mode = mode;
      break;

   case VSOCK_PACKET_TYPE_WAITING_READ:
   case VSOCK_PACKET_TYPE_WAITING_WRITE:
      ASSERT(wait);
      VSockOS_Memcpy(&pkt->u.wait, wait, sizeof pkt->u.wait);
      break;
   }

   VSOCK_PACKET_ASSERT(pkt);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockPacket_Validate --
 *
 *      Validate the given packet.
 *
 * Results:
 *      0 on success, EFAULT if the address is invalid, EINVAL if the packet
 *      fields are invalid.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int32
VSockPacket_Validate(VSockPacket *pkt)
{
   int32 err = EINVAL;

   if (NULL == pkt) {
      err = EFAULT;
      goto exit;
   }

   if (VMCI_HANDLE_INVALID(pkt->dg.src)) {
      goto exit;
   }

   if (VMCI_HANDLE_INVALID(pkt->dg.dst)) {
      goto exit;
   }

   if (VMCI_INVALID_ID == pkt->dstPort || VMCI_INVALID_ID == pkt->srcPort) {
      goto exit;
   }

   if (VSOCK_PACKET_VERSION != pkt->version) {
      goto exit;
   }

   if (0 != pkt->_reserved1 || 0 != pkt->_reserved2) {
      goto exit;
   }

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_INVALID:
      if (0 != pkt->u.size) {
         goto exit;
      }
      break;

   case VSOCK_PACKET_TYPE_REQUEST:
   case VSOCK_PACKET_TYPE_NEGOTIATE:
      if (0 == pkt->u.size) {
         goto exit;
      }
      break;

   case VSOCK_PACKET_TYPE_OFFER:
   case VSOCK_PACKET_TYPE_ATTACH:
      if (VMCI_HANDLE_INVALID(pkt->u.handle)) {
         goto exit;
      }
      break;

   case VSOCK_PACKET_TYPE_WROTE:
   case VSOCK_PACKET_TYPE_READ:
   case VSOCK_PACKET_TYPE_RST:
      if (0 != pkt->u.size) {
         goto exit;
      }
      break;
   }

   err = 0;
   
exit:
   return sockerr2err(err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VSockPacket_GetAddresses --
 *
 *      Get the local and remote addresses from the given packet.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VSockPacket_GetAddresses(VSockPacket *pkt,           // IN
                         struct sockaddr_vm *local,  // OUT
                         struct sockaddr_vm *remote) // OUT
{
   VSOCK_PACKET_ASSERT(pkt);
   VSockAddr_Init(local, VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst),
                  pkt->dstPort);
   VSockAddr_Init(remote, VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                  pkt->srcPort);
}


#endif // _VSOCK_PACKET_H_


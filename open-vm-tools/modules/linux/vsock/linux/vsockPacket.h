/*********************************************************
 * Copyright (C) 2007-2012 VMware, Inc. All rights reserved.
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

#include "vmci_sockets_packet.h"

#if defined(_WIN32) || defined(VMKERNEL) || defined(__APPLE__) || defined(VMX86_VMX)
# include "vsockOSInt.h"
#else
# define VSockOS_ClearMemory(_dst, _sz)   memset(_dst, 0, _sz)
# define VSockOS_Memcpy(_dst, _src, _sz)  memcpy(_dst, _src, _sz)
#endif

#include "vsockCommon.h"


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
                 VSockProtoVersion proto, // IN
                 VMCIHandle handle)       // IN
{
   ASSERT(pkt);
   VSOCK_ADDR_NOFAMILY_ASSERT(src);
   VSOCK_ADDR_NOFAMILY_ASSERT(dst);

   /*
    * We register the stream control handler as an any cid handle so we
    * must always send from a source address of VMADDR_CID_ANY
    */
   pkt->dg.src = VMCI_MAKE_HANDLE(VMADDR_CID_ANY, VSOCK_PACKET_LOCAL_RID);
   pkt->dg.dst = VMCI_MAKE_HANDLE(dst->svm_cid,
                                  dst->svm_cid == VMCI_HYPERVISOR_CONTEXT_ID ?
                                  VSOCK_PACKET_HYPERVISOR_RID :
                                  VSOCK_PACKET_RID);
   pkt->dg.payloadSize = sizeof *pkt - sizeof pkt->dg;
   pkt->version = VSOCK_PACKET_VERSION;
   pkt->type = type;
   pkt->srcPort = src->svm_port;
   pkt->dstPort = dst->svm_port;
   VSockOS_ClearMemory(&pkt->proto, sizeof pkt->proto);
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

   case VSOCK_PACKET_TYPE_REQUEST2:
   case VSOCK_PACKET_TYPE_NEGOTIATE2:
      pkt->u.size = size;
      pkt->proto = proto;
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

   /* See the comment above VSOCK_PACKET_ASSERT. */
   if (pkt->type < VSOCK_PACKET_TYPE_REQUEST2) {
      if (0 != pkt->proto || 0 != pkt->_reserved2) {
         goto exit;
      }
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
 *      None.
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

/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * vmci_page_channel.h
 *
 *    vPageChannel structure and functions.
 */

#ifndef _VMCI_PAGE_CHANNEL_H_
#define _VMCI_PAGE_CHANNEL_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"

/* Max size of a single tx buffer. */
#define VPAGECHANNEL_MAX_TX_BUF_SIZE (1 << 14)
#define VPAGECHANNEL_MAX_PAGES_PER_TX_BUFFER \
   (VPAGECHANNEL_MAX_TX_BUF_SIZE / PAGE_SIZE + 1)

#define VPAGECHANNEL_PACKET_ELEMS(packet)             \
   (VPageChannelElem *)((char *)(packet) +            \
                         sizeof(VPageChannelPacket) + \
                         packet->msgLen)
#define VPAGECHANNEL_PACKET_MESSAGE(packet) \
   (char *)((char *)(packet) + sizeof(VPageChannelPacket))


typedef
#include "vmware_pack_begin.h"
struct VPageChannelElem {
   union {
      uint64 pa;     // For guest
      uint64 ma;     // For hypervisor
   };
   uint32 le;
}
#include "vmware_pack_end.h"
VPageChannelElem;

typedef enum {
   VPCPacket_Data = 1,
   VPCPacket_Completion_Notify, // Hypervisor to guest only.
   VPCPacket_GuestConnect,      // Connect to hypervisor.  Internal use only.
   VPCPacket_HyperConnect,      // Complete connection handshake.  Internal.
   VPCPacket_RequestBuffer,     // Request buffers.  Internal use only.
   VPCPacket_SetRecvBuffer,     // Set buffers.  Internal use only.
} VPageChannelPacketType;

typedef
#include "vmware_pack_begin.h"
struct VPageChannelPacket {
   VPageChannelPacketType type;
   uint32 msgLen;
   uint32 numElems;
   /*
    * Followed by msgLen of message and numElems of VPageChannelElem.
    */
}
#include "vmware_pack_end.h"
VPageChannelPacket;

typedef
#include "vmware_pack_begin.h"
struct VPageChannelBuffer {
   uint32 numElems;
   VPageChannelElem elems[1];
   /*
    * Followed by numElems - 1 of VPageChannelElem.
    */
}
#include "vmware_pack_end.h"
VPageChannelBuffer;

typedef
#include "vmware_pack_begin.h"
struct VPageChannelGuestConnectMessage {
   VMCIHandle dgHandle;
   VMCIHandle qpHandle;
   uint64 produceQSize;
   uint64 consumeQSize;
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VPageChannelGuestConnectMessage;

typedef
#include "vmware_pack_begin.h"
struct VPageChannelHyperConnectMessage {
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VPageChannelHyperConnectMessage;

struct VPageChannel;
typedef struct VPageChannel VPageChannel;

typedef void (*VPageChannelRecvCB)(void *clientData,
                                   VPageChannelPacket *packet);


#if !defined(VMKERNEL)

typedef int (*VPageChannelAllocElemFn)(void *clientData,
                                       VPageChannelElem *elems,
                                       int numElems);

typedef void (*VPageChannelFreeElemFn)(void *clientData,
                                       VPageChannelElem *elems,
                                       int numElems);

int VPageChannel_CreateInVM(VPageChannel **channel,
                            VMCIId resourceId,
                            VMCIId peerResourceId,
                            uint64 produceQSize,
                            uint64 consumeQSize,
                            VPageChannelRecvCB recvCB,
                            void *clientRecvData,
                            Bool notifyOnly,
                            VPageChannelAllocElemFn elemAlloc,
                            void *allocClientData,
                            VPageChannelFreeElemFn elemFree,
                            void *freeClientData,
                            int defaultRecvBuffers,
                            int maxRecvBuffers);

#else // VMKERNEL

int VPageChannel_CreateInVMK(VPageChannel **channel,
                             VMCIId resourceId,
                             VPageChannelRecvCB recvCB,
                             void *clientRecvData);

int VPageChannel_ReserveBuffers(VPageChannel *channel,
                                size_t dataLen,
                                VPageChannelBuffer **buffer);
void VPageChannel_ReleaseBuffers(VPageChannel *channel,
                                 VPageChannelBuffer *buffer,
                                 Bool returnToFreePool);

/*
 * This function is called when the client is finished using the
 * scatter-gather list of a packet. This will generate a notification to the
 * guest to pass the ownership of buffers back to the guest. This can also be
 * used to read back the data from hypervisor and send it the to guest.
 */

int VPageChannel_CompletionNotify(VPageChannel *channel,
                                  char *message,
                                  int len,
                                  VPageChannelBuffer *buffer);

int VPageChannel_MapToMa(VPageChannel *channel,
                         VPageChannelElem paElem,
                         VPageChannelElem *maElems,
                         uint32 numElems);
int VPageChannel_UnmapMa(VPageChannel *channel,
                         VPageChannelBuffer *buffer,
                         int numElems);

#endif // VMKERNEL

/*
 * Common functions.
 */

void VPageChannel_Destroy(VPageChannel *channel);
int VPageChannel_Send(VPageChannel *channel,
                      VPageChannelPacketType type,
                      char *message,
                      int len,
                      VPageChannelBuffer *buffer);
int VPageChannel_SendPacket(VPageChannel *channel,
                            VPageChannelPacket *packet);
void VPageChannel_PollRecvQ(VPageChannel *channel);


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelPacket_BufferLen --
 *
 *      Calculate the length of the given packet.
 *
 * Results:
 *      The length of the given packet in bytes.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE size_t
VPageChannelPacket_BufferLen(VPageChannelPacket *packet) // IN
{
   size_t len, i;
   VPageChannelElem *elems;

   ASSERT(packet);

   len = 0;
   elems = VPAGECHANNEL_PACKET_ELEMS(packet);
   for (i = 0; i < packet->numElems; i++) {
      len += elems[i].le;
   }

   return len;
}


#if defined(linux) && !defined(VMKERNEL)
#include "compat_pci.h"
#define vmci_pci_map_page(_pg, _off, _sz, _dir) \
   pci_map_page(NULL, (_pg), (_off), (_sz), (_dir))
#define vmci_pci_unmap_page(_dma, _sz, _dir) \
   pci_unmap_page(NULL, (_dma), (_sz), (_dir))
#endif // linux && !VMKERNEL

#endif // _VMCI_PACKET_H_

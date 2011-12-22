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
 * vmci_packet.h --
 *
 *    VMCI packet structure and functions.
 */

#ifndef _VMCI_PACKET_H_
#define _VMCI_PACKET_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"

/* Max size of a single tx buffer. */
#define VMCI_PACKET_MAX_TX_BUF_SIZE (1 << 14)
#define VMCI_PACKET_MAX_PAGES_PER_TX_BUFFER \
   (VMCI_PACKET_MAX_TX_BUF_SIZE / PAGE_SIZE + 1)

#define VMCI_PACKET_SG_ELEMS(packet) \
   (VMCISgElem *)((char *)(packet) + sizeof(VMCIPacket) + packet->msgLen)
#define VMCI_PACKET_MESSAGE(packet) \
   (char *)((char *)(packet) + sizeof(VMCIPacket))


typedef
#include "vmware_pack_begin.h"
struct VMCISgElem {
   union {
      uint64 pa;     // For guest
      uint64 ma;     // For hypervisor
   };
   uint32 le;
}
#include "vmware_pack_end.h"
VMCISgElem;

typedef enum {
   VMCIPacket_Data = 1,
   VMCIPacket_Completion_Notify, // Hypervisor to guest only.
   VMCIPacket_GuestConnect,      // Connect to hypervisor.  Internal use only.
   VMCIPacket_HyperConnect,      // Complete connection handshake.  Internal.
   VMCIPacket_RequestBuffer,     // Request buffers.  Internal use only.
   VMCIPacket_SetRecvBuffer,     // Set buffers.  Internal use only.
} VMCIPacketType;

typedef
#include "vmware_pack_begin.h"
struct VMCIPacket {
   VMCIPacketType type;
   uint32 msgLen;
   uint32 numSgElems;
   /*
    * Followed by msgLen of message and numSgElems of VMCISgElem.
    */
}
#include "vmware_pack_end.h"
VMCIPacket;

typedef
#include "vmware_pack_begin.h"
struct VMCIPacketBuffer {
   uint32 numSgElems;
   VMCISgElem elems[1];
   /*
    * Followed by numSgElems - 1 of VMCISgElem.
    */
}
#include "vmware_pack_end.h"
VMCIPacketBuffer;

typedef
#include "vmware_pack_begin.h"
struct VMCIPacketGuestConnectMessage {
   VMCIHandle dgHandle;
   VMCIHandle qpHandle;
   uint64 produceQSize;
   uint64 consumeQSize;
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VMCIPacketGuestConnectMessage;

typedef
#include "vmware_pack_begin.h"
struct VMCIPacketHyperConnectMessage {
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VMCIPacketHyperConnectMessage;

struct VMCIPacketChannel;
typedef struct VMCIPacketChannel VMCIPacketChannel;

typedef void (*VMCIPacketRecvCB)(void *clientData, VMCIPacket *packet);


#if !defined(VMKERNEL)

typedef int (*VMCIPacketAllocSgElemFn)(void *clientData,
                                       VMCISgElem *sgElems,
                                       int numOfElems);

typedef void (*VMCIPacketFreeSgElemFn)(void *clientData,
                                       VMCISgElem *sgElems,
                                       int numOfElems);

int VMCIPacketChannel_CreateInVM(VMCIPacketChannel **channel,
                                 VMCIId resourceId,
                                 VMCIId peerResourceId,
                                 uint64 produceQSize,
                                 uint64 consumeQSize,
                                 VMCIPacketRecvCB recvCB,
                                 void *clientRecvData,
                                 Bool notifyOnly,
                                 VMCIPacketAllocSgElemFn elemAlloc,
                                 void *allocClientData,
                                 VMCIPacketFreeSgElemFn elemFree,
                                 void *freeClientData,
                                 int defaultRecvBuffers,
                                 int maxRecvBuffers);

/*
 * Send a packet to the hypervisor. The message is copied and the buffers
 * represented by the scatter-gather list of (pa, le) are sent to the
 * hypervisor. The buffers belong to the hypervisor until it sends a completion
 * notification using VMCIPacketChannel_CompletionNotify().
 */

int VMCIPacketChannel_SendInVM(VMCIPacketChannel *channel, VMCIPacket *packet);

#else // VMKERNEL

int VMCIPacketChannel_CreateInVMK(VMCIPacketChannel **channel,
                                  VMCIId resourceId,
                                  VMCIPacketRecvCB recvCB,
                                  void *clientRecvData);

int VMCIPacketChannel_ReserveBuffers(VMCIPacketChannel *channel,
                                     size_t dataLen,
                                     VMCIPacketBuffer **buffer);
void VMCIPacketChannel_ReleaseBuffers(VMCIPacketChannel *channel,
                                      VMCIPacketBuffer *buffer,
                                      Bool returnToFreePool);

/*
 * This function is called when the client is finished using the
 * scatter-gather list of a packet. This will generate a notification to the
 * guest to pass the ownership of buffers back to the guest. This can also be
 * used to read back the data from hypervisor and send it the to guest.
 */

int VMCIPacketChannel_CompletionNotify(VMCIPacketChannel *channel,
                                       char *message,
                                       int len,
                                       VMCIPacketBuffer *buffer);

int VMCIPacketChannel_MapToMa(VMCIPacketChannel *channel,
                              VMCISgElem paElem,
                              VMCISgElem *maElems,
                              uint32 numSgElems);
int VMCIPacketChannel_UnmapMa(VMCIPacketChannel *channel,
                              VMCIPacketBuffer *buffer,
                              int numSgElems);

#endif // VMKERNEL

/*
 * Common functions.
 */

void VMCIPacketChannel_Destroy(VMCIPacketChannel *channel);
int VMCIPacketChannel_Send(VMCIPacketChannel *channel,
                           VMCIPacketType type,
                           char *message,
                           int len,
                           VMCIPacketBuffer *buffer);
int VMCIPacketChannel_SendPacket(VMCIPacketChannel *channel,
                                 VMCIPacket *packet);
void VMCIPacketChannel_PollRecvQ(VMCIPacketChannel *channel);


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIPacket_BufferLen --
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
VMCIPacket_BufferLen(VMCIPacket *packet) // IN
{
   size_t len, i;
   VMCISgElem *elems;

   ASSERT(packet);

   len = 0;
   elems = VMCI_PACKET_SG_ELEMS(packet);
   for (i = 0; i < packet->numSgElems; i++) {
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

/*********************************************************
 * Copyright (C) 2011-2012 VMware, Inc. All rights reserved.
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

/** \cond PRIVATE */
#define VPAGECHANNEL_MAX_TX_BUF_SIZE (1 << 14)
#define VPAGECHANNEL_MAX_PAGES_PER_TX_BUFFER \
   (VPAGECHANNEL_MAX_TX_BUF_SIZE / PAGE_SIZE + 1)
/** \endcond */

/**
 * \brief Get a pointer to the elements in a packet.
 *
 * Returns a pointer to the elements at the end of a page channel packet.
 *
 * \see VPageChannelElem
 * \see VPageChannelPacket
 */

#define VPAGECHANNEL_PACKET_ELEMS(packet)             \
   (VPageChannelElem *)((char *)(packet) +            \
                         sizeof(VPageChannelPacket) + \
                         packet->msgLen)

/**
 * \brief Get a pointer to the message in a packet.
 *
 * Returns a pointer to the message embedded in a page channel packet.
 *
 * \see VPageChannelPacket
 */

#define VPAGECHANNEL_PACKET_MESSAGE(packet) \
   (char *)((char *)(packet) + sizeof(VPageChannelPacket))

/**
 * \brief Notify client directly, and do not read packets.
 *
 * This flag indicates that the channel should invoke the client's receive
 * callback directly when any packets are available.  If not specified, then
 * when a notification is received, packets are read from the channel and the
 * callback invoked for each one separately.
 *
 * \note Not applicable to VMKernel.
 *
 * \see VPageChannel_CreateInVM()
 */

#define VPAGECHANNEL_FLAGS_NOTIFY_ONLY 0x1

/**
 * \brief Invoke client's receive callback in delayed context.
 *
 * This flag indicates that all callbacks run in a delayed context, and the
 * caller and callback are allowed to block.  If not specified, then callbacks
 * run in interrupt context and the channel will not block, and the caller
 * is not allowed to block.
 *
 * \note Not applicable to VMKernel.
 *
 * \see VPageChannel_CreateInVM()
 */

#define VPAGECHANNEL_FLAGS_RECV_DELAYED 0x2

/**
 * \brief Send from an atomic context.
 *
 * This flag indicates that the client wishes to call Send() from an atomic
 * context and that the channel should not block.  If the channel is not
 * allowed to block, then the channel's pages are permanently mapped and
 * pinned.  Note that this will limit the total size of the channel to
 * VMCI_MAX_PINNED_QP_MEMORY.
 *
 * \note Not applicable to VMKernel.
 *
 * \see VPageChannel_CreateInVM()
 */

#define VPAGECHANNEL_FLAGS_SEND_WHILE_ATOMIC 0x4

/**
 * \brief An element describing a data range.
 *
 * Describes a data range, starting at a base address and for a given
 * length, i.e., an element of a scatter-gather list.  Indicates physical
 * address for the guest and machine address for hypervisor.  Can be passed
 * via packets or buffers.
 *
 * \note Structure is packed.
 *
 * \see VPageChannelPacket
 * \see VPageChanelBuffer
 */

typedef
#include "vmware_pack_begin.h"
struct VPageChannelElem {
   union {
      /** \brief Physical address for guest. */
      uint64 pa;

      /** \brief Machine address for hypervisor. */
      uint64 ma;
   };

   /** \brief Length of range. */
   uint32 le;
}
#include "vmware_pack_end.h"
VPageChannelElem;

/**
 * \brief Page channel page types.
 *
 * The various types of page channel packets.
 *
 * \see VPageChannelPacket
 */
typedef enum {
   /** \brief Data packet. */
   VPCPacket_Data = 1,

   /** \brief Completion notification, from hypervisor to guest. */
   VPCPacket_Completion_Notify,

   /** \cond PRIVATE */
   /** \brief Connect to hypervisor, internal. */
   VPCPacket_GuestConnect,

   /** \brief Complete connection handshake, internal. */
   VPCPacket_HyperConnect,

   /** \brief Request buffers, internal. */
   VPCPacket_RequestBuffer,

   /** \brief Set buffers, internal. */
   VPCPacket_SetRecvBuffer,

   /** \brief Hypervisor channel disconnect, internal. */
   VPCPacket_HyperDisconnect,

   /** \brief Guest channel ACK hypervisor disconnect, internal. */
   VPCPacket_GuestDisconnect,
   /** \endcond */
} VPageChannelPacketType;

/**
 * \brief Page channel packet structure.
 *
 * A packet structure for passing control/data between guest and hypervisor.
 * Can optionally contain a message and also a number of elements.
 *
 * \note Structure is packed.
 *
 * \see VPageChannelPacketType
 */
typedef
#include "vmware_pack_begin.h"
struct VPageChannelPacket {
   /** \brief Type of packet. */
   VPageChannelPacketType type;

   /** \brief Length of optional message. */
   uint32 msgLen;

   /** \brief Number of optional elements in packet. */
   uint32 numElems;

   /** \brief Followed by msgLen of message and numElems VPageChannelElem. */
}
#include "vmware_pack_end.h"
VPageChannelPacket;

/**
 * \brief Page channel buffer structure.
 *
 * A buffer of elements (a scatter-gather list).
 *
 * \note Structure is packed.
 *
 * \see VPageChannelElem
 */

typedef
#include "vmware_pack_begin.h"
struct VPageChannelBuffer {
   /** \brief Number of elements. */
   uint32 numElems;

   /** \brief First element. */
   VPageChannelElem elems[1];

   /** \brief Followed by numElems - 1 of VPageChannelElem. */
}
#include "vmware_pack_end.h"
VPageChannelBuffer;

/** \cond PRIVATE */
typedef
#include "vmware_pack_begin.h"
struct VPageChannelGuestConnectMessage {

   /** \brief Guest channel's datagram handle for control channel. */
   VMCIHandle dgHandle;

   /** \brief Guest channel's queuepair handle. */
   VMCIHandle qpHandle;

   /** \brief Size of producer queue in queuepair in bytes. */
   uint64 produceQSize;

   /** \brief Size of consumer queue in queuepair in bytes. */
   uint64 consumeQSize;

   /** \brief Guest channel's doorbell handle. */
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VPageChannelGuestConnectMessage;

typedef
#include "vmware_pack_begin.h"
struct VPageChannelHyperConnectMessage {
   /** \brief Hypervisor's doorbell handle. */
   VMCIHandle doorbellHandle;
}
#include "vmware_pack_end.h"
VPageChannelHyperConnectMessage;
/** \endcond PRIVATE */

/** \cond PRIVATE */
typedef enum VPageChannelState {
   VPCState_Free = 0,
   VPCState_Unconnected,
   VPCState_Connecting,
   VPCState_Connected,
   VPCState_Disconnecting,
   VPCState_Disconnected,
} VPageChannelState;
/** \endcond PRIVATE */

/**
 * \brief Opaque page channel type.
 */

struct VPageChannel;
typedef struct VPageChannel VPageChannel;

/**
 * \brief Client receive callback type.
 *
 * Type of receive callback, invoked when there are data packets in the
 * channel.  The client provides a callback with this type to
 * VPageChannel_CreateInVM().  If VPAGECHANNEL_FLAGS_NOTIFY_ONLY is specified
 * in the channel creation flags, then \c packet is \c NULL; otherwise,
 * \c packet points to a channel packet.
 *
 * \see VPageChannel_CreateInVM()
 * \see VPageChannelPacket
 */

typedef void (*VPageChannelRecvCB)(void *clientData,
                                   VPageChannelPacket *packet);


#if !defined(VMKERNEL)

/**
 * \brief Client element allocation callback type.
 *
 * Type of element allocation callback, invoked when the channel needs
 * elements.  The client provides a callback of this type to
 * VPageChannel_CreateInVM().
 *
 * \see VPageChannel_CreateInVM()
 * \see VPageChannelElem
 * \see VPageChannelFreeElemFn
 */

typedef int (*VPageChannelAllocElemFn)(void *clientData,
                                       VPageChannelElem *elems,
                                       int numElems);

/**
 * \brief Client element release callback type.
 *
 * Type of element release callback, invoked when the channel releases
 * elements.  The client provides a callback of this type to
 * VPageChannel_CreateInVM().
 *
 * \see VPageChannel_CreateInVM()
 * \see VPageChannelElem
 * \see VPageChannelAllocElemFn
 */

typedef void (*VPageChannelFreeElemFn)(void *clientData,
                                       VPageChannelElem *elems,
                                       int numElems);

/*
 ************************************************************************
 * VPageChannel_CreateInVM                                         */ /**
 *
 * \brief Create guest page channel.
 *
 * Creates a page channel in the guest.  The channel should be released
 * with VPageChannel_Destroy().
 *
 * \note Only applicable in the guest.
 *
 * \see VPageChannel_CreateInVMK()
 * \see VPageChannel_Destroy()
 *
 * \param[out] channel           Pointer to a newly constructed page
 *                               channel if successful.
 * \param[in]  resourceId        Resource ID on which the channel should
 *                               register its control channel.
 * \param[in]  peerResourceId    Resource ID of peer's control channel.
 * \param[in]  produceQSize      Size of producer queue in queuepair in
 *                               bytes.
 * \param[in]  consumeQSize      Size of consumer queue in queuepair in
 *                               bytes.
 * \param[in]  flags             Channel flags.
 * \param[in]  recvCB            Client's receive callback.
 * \param[in]  clientRecvData    Client data for client's receive
 *                               callback.
 * \param[in]  elemAlloc         Element allocation callback for
 *                               allocating page ranges (scatter-gather
 *                               elements).
 * \param[in]  allocClientData   Client data for element allocation
 *                               callback.
 * \param[in]  elemFree          Element release callback for elements.
 * \param[in]  freeClientData    Client data for element release
 *                               callback.
 * \param[in]  defRecvBufs       Default number of elements sent to
 *                               hypervisor channel.
 * \param[in]  maxRecvBufs       Maximum number of elements that can be
 *                               sent to the hypervisor channel.
 *
 * \retval     VMCI_SUCCESS      Creation succeeded, \c *channel contains
 *                               a pointer to a valid channel.
 * \retval     other             Failure.
 *
 ************************************************************************
 */

int VPageChannel_CreateInVM(VPageChannel **channel,
                            VMCIId resourceId,
                            VMCIId peerResourceId,
                            uint64 produceQSize,
                            uint64 consumeQSize,
                            uint32 flags,
                            VPageChannelRecvCB recvCB,
                            void *clientRecvData,
                            VPageChannelAllocElemFn elemAlloc,
                            void *allocClientData,
                            VPageChannelFreeElemFn elemFree,
                            void *freeClientData,
                            int defRecvBufs,
                            int maxRecvBufs);

#else // VMKERNEL

/**
 * \brief Type of VM memory access off callback.
 *
 * This callback is invoked when the memory of the VM containing the peer
 * endpoint becomes inaccessible, for example due to a crash.  When this
 * occurs, the client should unmap any guest pages and cleanup any state.
 * This callback runs in a blockable context.  The client is not allowed to
 * call VPageChannel_Destroy() from inside the callback, or it will deadlock,
 * since that function will wait for the callback to complete.
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_CreateInVMK()
 */

typedef void (*VPageChannelMemAccessOffCB)(void *clientData);

/*
 ************************************************************************
 * VPageChannel_CreateInVMK                                        */ /**
 *
 * \brief Create a page channel in VMKernel.
 *
 * Creates a page channel.  The channel should be released with
 * VPageChannel_Destroy().
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_CreateInVM()
 * \see VPageChannel_Destroy()
 *
 * \param[out] channel           Pointer to a newly constructed page
 *                               channel if successful.
 * \param[in]  resourceId        Resource ID on which to register
 *                               control channel.
 * \param[in]  recvCB            Client's receive callback.
 * \param[in]  clientRecvData    Client data for receive callback.
 * \param[in]  memAccessOffCB    Client's mem access off callback.
 * \param[in]  memAccessOffData  Client data for mem access off.
 *
 * \retval     VMCI_SUCCESS      Creation succeeded, \c *channel
 *                               contains a pointer to a valid channel.
 * \retval     other             Failure.
 *
 ***********************************************************************
 */

int VPageChannel_CreateInVMK(VPageChannel **channel,
                             VMCIId resourceId,
                             VPageChannelRecvCB recvCB,
                             void *clientRecvData,
                             VPageChannelMemAccessOffCB memAccessOffCB,
                             void *memAccessOffData);

/*
 ************************************************************************
 * VPageChannel_ReserveBuffers                                     */ /**
 *
 * \brief Reserve guest elements.
 *
 * Reserve sufficient guest elements to cover the given length.  The
 * buffers can then be posted to the guest.  This allocates both the
 * buffer and the elements within the buffer.
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_ReleaseBuffers()
 *
 * \param[in]  channel  Page channel.
 * \param[in]  dataLen  Length to reserve in bytes.
 * \param[out] buffer   Pointer to a buffer containing elements to cover
 *                      the given length if successful.
 *
 * \retval     VMCI_SUCCESS   Reservation succeeded, \c *buffer contains
 *                            a pointer to a valid buffer.
 * \retval     other          Failure.
 *
 ************************************************************************
 */

int VPageChannel_ReserveBuffers(VPageChannel *channel,
                                size_t dataLen,
                                VPageChannelBuffer **buffer);

/*
 ************************************************************************
 * VPageChannel_ReleaseBuffers                                     */ /**
 *
 * \brief Release guest elements.
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_ReserveBuffers()
 *
 * Release guest elements previous reserved with
 * VPageChannel_ReserveBuffers().  If the buffers were sent to the guest,
 * then only the buffer itself should be released, i.e.,
 * \c returnToFreePool should be \c FALSE; the guest will release the
 * buffers on completion.  Otherwise, if for some reason they are not
 * sent after reserving them, then \c returnToFreePool should be set to
 * \c TRUE.
 *
 * \param[in]  channel           Page channel.
 * \param[in]  buffer            Buffer to be released.
 * \param[in]  returnToFreePool  If \c TRUE, then release the elements
 *                               of the buffer along with the buffer
 *                               itself.  If \c FALSE, then release only
 *                               the buffer pointer itself.
 *
 ************************************************************************
 */

void VPageChannel_ReleaseBuffers(VPageChannel *channel,
                                 VPageChannelBuffer *buffer,
                                 Bool returnToFreePool);

/*
 ************************************************************************
 * VPageChannel_CompletionNotify                                   */ /**
 *
 * \brief Notify channel of completion.
 *
 * This function is called when the client is finished using the elements
 * (scatter-gather list) of a packet.  This will generated a notification
 * to the guest to pass ownership of the buffers back to the guest.  This
 * can also be used to read back the data from the hypervisor and send
 * it to the guest.
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_ReserveBuffers
 *
 * \param[in]  channel  Channel on which I/O is complete.
 * \param[in]  message  Optional message to send to guest.
 * \param[in]  len      Length of optional message.
 * \param[in]  buffer   Buffer used for I/O.
 *
 ************************************************************************
 */

int VPageChannel_CompletionNotify(VPageChannel *channel,
                                  char *message,
                                  int len,
                                  VPageChannelBuffer *buffer);

/*
 ************************************************************************
 * VPageChannel_MapToMa                                           */ /**
 *
 * \brief Map guest PA in an element to a list of MAs.
 *
 * Map a guest physical address to a list of hypervisor machine
 * addresses.
 *
 * \note Only applicable on VMKernel.
 *
 * \param[in]     channel    Channel on which to map.
 * \param[in]     paElem     Guest's physical address.
 * \param[out]    maElems    Hypervisor machine addresses.
 * \param[in]     numElems   Max number of hypervisor elements.
 *
 * \retval  elems    Number of mapped elements.
 *
 ************************************************************************
 */

int VPageChannel_MapToMa(VPageChannel *channel,
                         VPageChannelElem paElem,
                         VPageChannelElem *maElems,
                         uint32 numElems);

/*
 ************************************************************************
 * VPageChannel_UnmapMa                                            */ /**
 *
 * \brief Unmap MA for a buffer.
 *
 * Unmap hypervisor machine addresses referring to a guest physical
 * addresses.
 *
 * \note Only applicable on VMKernel.
 *
 * \see VPageChannel_MapToMa
 *
 * \param[in]  channel     Channel for which to unmap.
 * \param[in]  buffer      Buffer containing elements to unmap.
 * \param[in]  numElems    Number of elements to unmap.
 *
 * \retval  0     Unmap successful.
 * \retval  -1    World not found for channel.
 *
 ************************************************************************
 */

int VPageChannel_UnmapMa(VPageChannel *channel,
                         VPageChannelBuffer *buffer,
                         int numElems);

#endif // VMKERNEL

/*
 ************************************************************************
 * VPageChannel_Destroy                                            */ /**
 *
 * \brief Destroy the given channel.
 *
 * Destroy the given channel.  This will disconnect from the peer
 * channel (if connected) and release all resources.
 *
 * \see VPageChannel_CreateInVMK
 * \see VPageChannel_CreateInVM
 *
 * \param[in]  channel  The channel to be destroyed.
 *
 ************************************************************************
 */

void VPageChannel_Destroy(VPageChannel *channel);

/*
 ************************************************************************
 * VPageChannel_Send                                               */ /**
 *
 * \brief Send a packet to the channel's peer.
 *
 * Send a packet to the channel's peer.  A message and a number of
 * elements may optionally be sent.  If the send is successful, the
 * elements are owned by the peer and only the buffer itself should
 * be released, but not the elements within.  If the send fails, the
 * client should release the buffer and the elements.
 *
 * \see VPageChannel_SendPacket
 *
 * \param[in]  channel     Channel on which to send.
 * \param[in]  type        Type of packet to send.
 * \param[in]  message     Optional message to send.
 * \param[in]  len         Length of optional message.
 * \param[in]  buffer      Buffer (of elements) to send.
 *
 * \retval  VMCI_SUCCESS   Packet successfully sent, buffer elements
 *                         owned by peer.
 * \retval  other          Failure to send, client should release
 *                         elements.
 *
 ************************************************************************
 */

int VPageChannel_Send(VPageChannel *channel,
                      VPageChannelPacketType type,
                      char *message,
                      int len,
                      VPageChannelBuffer *buffer);

/*
 ************************************************************************
 * VPageChannel_SendPacket                                         */ /**
 *
 * \brief Send the given packet to the channel's peer.
 *
 * Send a client-constructed packet to the channel's peer.  If the
 * send is successful, any elements in the packet are owned by the
 * peer.  Otherwise, the client retains ownership.
 *
 * \see VPageChannel_Send
 *
 * \param[in]  channel  Channel on which to send.
 * \param[in]  packet   Packet to be sent.
 *
 * \retval  VMCI_SUCCESS   Packet successfully sent, buffer elements
 *                         owned by peer.
 * \retval  other          Failure to send, client should release
 *                         elements.
 *
 ************************************************************************
 */

int VPageChannel_SendPacket(VPageChannel *channel,
                            VPageChannelPacket *packet);

/*
 ************************************************************************
 * VPageChannel_PollRecvQ                                          */ /**
 *
 * \brief Poll the channel's receive queue for packets.
 *
 * Poll the channel's receive queue for packets from the peer.  If any
 * packets are available, the channel's receive callback will be invoked.
 *
 * \param[in]  channel  Channel to poll.
 *
 ************************************************************************
 */

void VPageChannel_PollRecvQ(VPageChannel *channel);

/*
 ************************************************************************
 * VPageChannel_BufferLen                                          */ /**
 *
 * \brief Determine the length of a packet.
 *
 * Determine the length of the given packet in bytes.
 *
 * \param[in]  packet   Packet for which length is to be determined.
 *
 * \retval     bytes    Size of the packet in bytes.
 *
 ************************************************************************
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

/** \cond PRIVATE */
#if defined(linux) && !defined(VMKERNEL)
#include "compat_pci.h"
#define vmci_pci_map_page(_pg, _off, _sz, _dir) \
   pci_map_page(NULL, (_pg), (_off), (_sz), (_dir))
#define vmci_pci_unmap_page(_dma, _sz, _dir) \
   pci_unmap_page(NULL, (_dma), (_sz), (_dir))
#endif // linux && !VMKERNEL
/** \endcond PRIVATE */

#endif // _VMCI_PACKET_H_

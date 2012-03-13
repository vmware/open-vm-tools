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
 * vmciPacket.c --
 *
 *     Implementation of vPageChannel for guest kernels.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_page_channel.h"
#include "vmciDriver.h"
#include "vmciKernelAPI.h"

#if !defined(linux) || defined(VMKERNEL)
#error "Wrong platform."
#endif // !linux || VMKERNEL

#define LGPFX "vPageChannel: "

/*
 * This threshold is to account for packets being in-flight.  We can't keep
 * an accurate count of receive buffers, it's just an estimate, so we allow
 * some slack.
 */

#define VMCI_PACKET_RECV_THRESHOLD 150

/*
 * All flags.  We use this to check the validity of the flags, so put it here
 * instead of in the header, otherwise people might assume we mean for them
 * to use it.
 */

#define VPAGECHANNEL_FLAGS_ALL             \
   (VPAGECHANNEL_FLAGS_NOTIFY_ONLY       | \
    VPAGECHANNEL_FLAGS_RECV_DELAYED      | \
    VPAGECHANNEL_FLAGS_SEND_WHILE_ATOMIC)


/*
 * Page channel.  This is opaque to clients.
 */

struct VPageChannel {
   VMCIHandle dgHandle;
   uint32 flags;
   VPageChannelRecvCB recvCB;
   void *clientRecvData;
   VPageChannelAllocElemFn elemAllocFn;
   void *allocClientData;
   VPageChannelFreeElemFn elemFreeFn;
   void *freeClientData;

   /*
    * QueuePair info.
    */

   VMCIQPair *qpair;
   VMCIHandle qpHandle;
   uint64 produceQSize;
   uint64 consumeQSize;
   VMCIId attachSubId;
   VMCIId detachSubId;
   Bool qpConnected;
   Bool useSpinLock;
   spinlock_t qpRecvLock;
   spinlock_t qpSendLock;
   struct semaphore qpRecvMutex;
   struct semaphore qpSendMutex;

   /*
    * Doorbell info.
    */

   VMCIHandle doorbellHandle;
   VMCIHandle peerDoorbellHandle;

   /*
    * Receiving buffer.
    */

   int curRecvBufs;
   int recvBufsTarget;
   int defaultRecvBufs;
   int maxRecvBufs;

   VMCIId resourceId;
   VMCIHandle peerDgHandle;

   Bool inPoll;
};


static int VPageChannelSendControl(VPageChannel *channel,
                                   char *message,
                                   int len,
                                   VPageChannelPacketType type,
                                   int numElems,
                                   VPageChannelElem *elems);

/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelAcquireSendLock
 *
 *      Acquire the channel's send lock.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VPageChannelAcquireSendLock(VPageChannel *channel, // IN
                            unsigned long *flags)  // OUT
{
   ASSERT(channel);

   *flags = 0; /* Make compiler happy about it being unused in some paths. */
   if (channel->useSpinLock) {
      spin_lock_irqsave(&channel->qpSendLock, *flags);
   } else {
      down(&channel->qpSendMutex);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelReleaseSendLock
 *
 *      Release the channel's send lock.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VPageChannelReleaseSendLock(VPageChannel *channel, // IN
                            unsigned long flags)   // IN
{
   ASSERT(channel);

   if (channel->useSpinLock) {
      spin_unlock_irqrestore(&channel->qpSendLock, flags);
   } else {
      up(&channel->qpSendMutex);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelAcquireRecvLock
 *
 *      Acquire the channel's receive lock.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VPageChannelAcquireRecvLock(VPageChannel *channel, // IN
                            unsigned long *flags)  // OUT
{
   ASSERT(channel);
   ASSERT(flags);

   *flags = 0; /* Make compiler happy about it being unused in some paths. */
   if (channel->useSpinLock) {
      spin_lock_irqsave(&channel->qpRecvLock, *flags);
   } else {
      down(&channel->qpRecvMutex);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelReleaseRecvLock
 *
 *      Release the channel's receive lock.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VPageChannelReleaseRecvLock(VPageChannel *channel, // IN
                            unsigned long flags)   // IN
{
   ASSERT(channel);

   if (channel->useSpinLock) {
      spin_unlock_irqrestore(&channel->qpRecvLock, flags);
   } else {
      up(&channel->qpRecvMutex);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelSetRecvBuffers --
 *
 *      Set the receiving buffers for the channel.
 *
 * Results:
 *      VMCI_SUCCESS if set, negative error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelSetRecvBuffers(VPageChannel *channel,     // IN
                           int numElems,              // IN
                           Bool byControl)            // IN
{
   int retval;
   int allocNum;
   size_t size = sizeof(VPageChannelPacket) +
      numElems * sizeof(VPageChannelElem);
   VPageChannelElem *elems;
   VPageChannelPacket *packet;

   ASSERT(channel);

   packet = (VPageChannelPacket *)VMCI_AllocKernelMem(size, VMCI_MEMORY_ATOMIC);
   if (packet == NULL) {
      VMCI_WARNING((LGPFX"Failed to allocate packet (channel=%p) "
                    "(size=%"FMTSZ"u).\n",
                    channel,
                    size));
      return VMCI_ERROR_NO_MEM;
   }

   packet->type = VPCPacket_SetRecvBuffer;
   packet->msgLen = 0;
   packet->numElems = numElems;

   elems = VPAGECHANNEL_PACKET_ELEMS(packet);
   allocNum = channel->elemAllocFn(channel->allocClientData, elems, numElems);
   if (allocNum != numElems) {
      VMCI_WARNING((LGPFX"Failed to allocate receive buffer (channel=%p) "
                    "(expected=%d) (actual=%d).\n",
                    channel,
                    numElems,
                    allocNum));
      retval = VMCI_ERROR_NO_MEM;
      goto error;
   }

   if (byControl || !channel->qpConnected) {
      retval = VPageChannelSendControl(channel, NULL, 0,
                                            VPCPacket_SetRecvBuffer,
                                            numElems, elems);
   } else {
      retval = VPageChannel_SendPacket(channel, packet);
   }
   if (retval < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to set receive buffers (channel=%p) "
                    "(err=%d).\n",
                    channel,
                    retval));
      goto error;
   }

   channel->curRecvBufs += numElems;

   VMCI_FreeKernelMem(packet, size);

   return VMCI_SUCCESS;

 error:
   if (packet != NULL) {
      if (allocNum) {
         channel->elemFreeFn(channel->freeClientData, elems, allocNum);
      }
      VMCI_FreeKernelMem(packet, size);
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelRecvPacket --
 *
 *      Process a VMCI packet.
 *
 * Results:
 *      Always VMCI_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelRecvPacket(VPageChannel *channel,         // IN
                       VPageChannelPacket *packet)    // IN
{
   int recvBufsTarget;

   ASSERT(channel);
   ASSERT(packet);

   if (packet->type != VPCPacket_Data &&
       packet->type != VPCPacket_Completion_Notify &&
       packet->type != VPCPacket_RequestBuffer &&
       packet->type != VPCPacket_HyperConnect) {
      VMCI_WARNING((LGPFX"Received invalid packet (channel=%p) "
                    "(type=%d).\n",
                    channel,
                    packet->type));
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Received packet (channel=%p) (type=%d) "
                   "(elems=%d).\n",
                   channel,
                   packet->type,
                   packet->numElems));

   if (packet->type == VPCPacket_HyperConnect) {
      VPageChannelHyperConnectMessage *message;

      if (packet->msgLen < sizeof *message) {
         VMCI_WARNING((LGPFX"Received invalid hypervisor connection message "
                       "(channel=%p) (size=%u).\n",
                       channel,
                       packet->msgLen));
         return VMCI_ERROR_INVALID_ARGS;
      }

      message = (VPageChannelHyperConnectMessage *)
         VPAGECHANNEL_PACKET_MESSAGE(packet);
      channel->peerDoorbellHandle = message->doorbellHandle;

      VMCI_DEBUG_LOG(10,
                     (LGPFX"Connected to peer (channel=%p) "
                      "(db handle=0x%x:0x%x).\n",
                      channel,
                      channel->peerDoorbellHandle.context,
                      channel->peerDoorbellHandle.resource));

      return VMCI_SUCCESS;
   }

   recvBufsTarget = channel->recvBufsTarget;

   switch (packet->type) {
   case VPCPacket_RequestBuffer:
      /*
       * Increase the number of receive buffers by channel->defaultRecvBufs
       * if the hypervisor requests it.
       */

      VMCI_DEBUG_LOG(10,
                     (LGPFX"Requested more buffers (channel=%p) "
                      "(cur=%d) (target=%d) (max=%d).\n",
                      channel,
                      channel->curRecvBufs,
                      channel->recvBufsTarget,
                      channel->maxRecvBufs));

      if (channel->recvBufsTarget < channel->maxRecvBufs) {
         recvBufsTarget = channel->recvBufsTarget + channel->defaultRecvBufs;
      }
      break;

   case VPCPacket_Data:
      channel->recvCB(channel->clientRecvData, packet);
      channel->curRecvBufs -= packet->numElems;
      break;

   case VPCPacket_Completion_Notify:
      channel->recvCB(channel->clientRecvData, packet);
      break;

   default:
      ASSERT_NOT_IMPLEMENTED(FALSE);
      break;
   }

   /*
    * Set more receive buffers if it is below the threshold.  We bump it up
    * here even when not requested to do so.  This is to account for buffers
    * being in-flight, i.e., in packets that have not yet been processed by
    * the other side.  When we increase here, we also tack on extra threshold,
    * in the hope that we won't hit this again.
    */

   if (channel->curRecvBufs < (recvBufsTarget - VMCI_PACKET_RECV_THRESHOLD)) {
      int numElems = recvBufsTarget + VMCI_PACKET_RECV_THRESHOLD -
         channel->curRecvBufs;

      if (VPageChannelSetRecvBuffers(channel, numElems, FALSE) ==
          VMCI_SUCCESS) {
         channel->recvBufsTarget = recvBufsTarget;
      }
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelDgRecvFunc --
 *
 *      Callback function to receive a VMCI packet.  This is only used until
 *      the connection is made; after that, packets are received over the
 *      queuepair.
 *
 * Results:
 *      VMCI_SUCCESS on success, negative values on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelDgRecvFunc(void *clientData,         // IN
                       VMCIDatagram *dg)         // IN
{
   VPageChannel *channel = (VPageChannel *)clientData;

   ASSERT(channel);
   ASSERT(dg);

   if (dg->src.context != VMCI_HOST_CONTEXT_ID ||
       dg->src.resource != channel->peerDgHandle.resource) {
       VMCI_WARNING((LGPFX"Received a packet from an unknown source "
                     "(channel=%p) (handle=0x%x:0x%x).\n",
                     channel,
                     dg->src.context,
                     dg->src.resource));
       return VMCI_ERROR_NO_ACCESS;
   }

   if (dg->payloadSize < sizeof (VPageChannelPacket)) {
      VMCI_WARNING((LGPFX"Received invalid packet (channel=%p) "
                    "(size=%"FMT64"u).\n",
                    channel,
                    dg->payloadSize));
      return VMCI_ERROR_INVALID_ARGS;
   }

   return VPageChannelRecvPacket(channel, VMCI_DG_PAYLOAD(dg));
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelDoDoorbellCallback
 *
 *    Process a doorbell notification.  Will read packets from the queuepair
 *    until empty.
 *
 *    XXX, this function is now identical to the one with the same name in
 *    modules/vmkernel/vmci/vmciPacketVMK.c.  We should share this code.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VPageChannelDoDoorbellCallback(VPageChannel *channel) // IN/OUT
{
   Bool inUse;
   unsigned long flags;
   VPageChannelPacket packetHeader;

   ASSERT(channel);

   if (!channel->qpConnected) {
      VMCI_WARNING((LGPFX"Not connected (channel=%p).\n",
                    channel));
      return;
   }

   VPageChannelAcquireRecvLock(channel, &flags);
   inUse = channel->inPoll;
   channel->inPoll = TRUE;
   VPageChannelReleaseRecvLock(channel, flags);

   if (inUse) {
      return;
   }

retry:
   while (VMCIQPair_ConsumeBufReady(channel->qpair) >= sizeof packetHeader) {
      ssize_t retSize, totalSize;
      VPageChannelPacket *packet;

      retSize = VMCIQPair_Peek(channel->qpair, &packetHeader,
                               sizeof packetHeader,
                               /* XXX, UTIL_VMKERNEL_BUFFER for VMKernel. */
                               0);
      if (retSize < sizeof packetHeader) {
         /*
          * XXX, deal with partial read.
          */

         VMCI_WARNING((LGPFX"Failed to peek (channel=%p) "
                       "(required=%"FMTSZ"d) (err=%"FMTSZ"d).\n",
                       channel,
                       sizeof packetHeader,
                       retSize));
         break;
      }

      totalSize = sizeof packetHeader + packetHeader.msgLen +
         packetHeader.numElems * sizeof(VPageChannelElem);

      retSize = VMCIQPair_ConsumeBufReady(channel->qpair);
      if (retSize < totalSize) {
         /*
          * XXX, deal with partial read.
          */

         VMCI_WARNING((LGPFX"Received partial packet (channel=%p) "
                       "(type=%d) (len=%d) (num elems=%d) (avail=%"FMTSZ"d) "
                       "(requested=%"FMTSZ"d).\n",
                       channel,
                       packetHeader.type,
                       packetHeader.msgLen,
                       packetHeader.numElems,
                       retSize,
                       totalSize));
         break;
      }

      packet = (VPageChannelPacket *)
         VMCI_AllocKernelMem(totalSize, VMCI_MEMORY_ATOMIC);
      if (!packet) {
         VMCI_WARNING((LGPFX"Failed to allocate packet (channel=%p) "
                       "(size=%"FMTSZ"d).\n",
                       channel,
                       totalSize));
         break;
      }

      retSize = VMCIQPair_Dequeue(channel->qpair, packet,
                                  totalSize,
                                  /* XXX, UTIL_VMKERNEL_BUFFER for VMKernel. */
                                  0);
      if (retSize < totalSize) {
         /*
          * XXX, deal with partial read.
          */

         VMCI_WARNING((LGPFX"Failed to dequeue (channel=%p) "
                       "(required=%"FMTSZ"d) (err=%"FMTSZ"d).\n",
                       channel,
                       totalSize,
                       retSize));
         VMCI_FreeKernelMem(packet, totalSize);
         break;
      }

      VPageChannelRecvPacket(channel, packet);
      VMCI_FreeKernelMem(packet, totalSize);
   }

   VPageChannelAcquireRecvLock(channel, &flags);

   /*
    * The doorbell may have been notified between when we we finished reading
    * data and when we grabbed the lock.  If that happens, then there may be
    * data, but we bailed out of that second notification because inPoll was
    * already set.  So that we don't miss anything, do a final check here under
    * the lock for any data that might have arrived.
    */

   if (VMCIQPair_ConsumeBufReady(channel->qpair) >= sizeof packetHeader) {
      VPageChannelReleaseRecvLock(channel, flags);
      goto retry;
   }

   channel->inPoll = FALSE;
   VPageChannelReleaseRecvLock(channel, flags);
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelDoorbellCallback --
 *
 *    Callback for doorbell notification.  Will invoke the channel's receive
 *    function directly or process the packets in the queuepair.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VPageChannelDoorbellCallback(void *clientData) // IN/OUT
{
   VPageChannel *channel = (VPageChannel *)clientData;

   ASSERT(channel);

   if (channel->flags & VPAGECHANNEL_FLAGS_NOTIFY_ONLY) {
      channel->recvCB(channel->clientRecvData, NULL);
   } else {
      VPageChannelDoDoorbellCallback(channel);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelSendConnectionMessage --
 *
 *    Send a connection control message to the hypervisor.
 *
 * Results:
 *    VMCI_SUCCESS if sent, negative values on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VPageChannelSendConnectionMessage(VPageChannel *channel) // IN
{
   VPageChannelGuestConnectMessage message;

   ASSERT(channel);

   memset(&message, 0, sizeof message);
   message.dgHandle = channel->dgHandle;
   message.qpHandle = channel->qpHandle;
   message.produceQSize = channel->produceQSize;
   message.consumeQSize = channel->consumeQSize;
   message.doorbellHandle = channel->doorbellHandle;

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Sending guest connect (channel=%p) "
                   "(qp handle=0x%x:0x%x).\n",
                   channel,
                   channel->qpHandle.context,
                   channel->qpHandle.resource));

   return VPageChannelSendControl(channel,
                                  (char *)&message, sizeof message,
                                  VPCPacket_GuestConnect, 0, NULL);
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelPeerAttachCB --
 *
 *    Invoked when a peer attaches to a queue pair.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify page channel state.
 *
 *----------------------------------------------------------------------------
 */

static void
VPageChannelPeerAttachCB(VMCIId subId,             // IN
                         VMCI_EventData *eData,    // IN
                         void *clientData)         // IN
{
   VPageChannel *channel;
   VMCIEventPayload_QP *ePayload;

   ASSERT(eData);
   ASSERT(clientData);

   channel = (VPageChannel *)clientData;
   ePayload = VMCIEventDataPayload(eData);

   if (VMCI_HANDLE_EQUAL(channel->qpHandle, ePayload->handle)) {
      VMCI_DEBUG_LOG(10,
                     (LGPFX"Peer attached (channel=%p) "
                      "(qp handle=0x%x:0x%x).\n",
                      channel,
                      ePayload->handle.context,
                      ePayload->handle.resource));
      channel->qpConnected = TRUE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelPeerDetachCB --
 *
 *    Invoked when a peer detaches from a queue pair.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify page channel state.
 *
 *----------------------------------------------------------------------------
 */

static void
VPageChannelPeerDetachCB(VMCIId subId,             // IN
                         VMCI_EventData *eData,    // IN
                         void *clientData)         // IN
{
   VPageChannel *channel;
   VMCIEventPayload_QP *ePayload;

   ASSERT(eData);
   ASSERT(clientData);

   channel = (VPageChannel *)clientData;
   ePayload = VMCIEventDataPayload(eData);

   if (VMCI_HANDLE_EQUAL(channel->qpHandle, ePayload->handle)) {
      VMCI_DEBUG_LOG(10,
                     (LGPFX"Peer detached (channel=%p) "
                      "(qp handle=0x%x:0x%x).\n",
                      channel,
                      ePayload->handle.context,
                      ePayload->handle.resource));
      channel->qpConnected = FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelDestroyQueuePair --
 *
 *    Destroy the channel's queuepair, along with the event subscriptions.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify page channel state.
 *
 *----------------------------------------------------------------------------
 */

static void
VPageChannelDestroyQueuePair(VPageChannel *channel) // IN/OUT
{
   ASSERT(channel);

   if (channel->attachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(channel->attachSubId);
      channel->attachSubId = VMCI_INVALID_ID;
   }

   if (channel->detachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(channel->detachSubId);
      channel->detachSubId = VMCI_INVALID_ID;
   }

   if (!VMCI_HANDLE_INVALID(channel->qpHandle)) {
      ASSERT(channel->qpair);
      VMCIQPair_Detach(&channel->qpair);
      channel->qpHandle = VMCI_INVALID_HANDLE;
      channel->qpair = NULL;
   }

   channel->qpConnected = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VPageChannelCreateQueuePair --
 *
 *    Create queuepair for data communication.
 *
 * Results:
 *    VMCI_SUCCESS if the queuepair is created, negative values on failure.
 *
 * Side effects:
 *    May modify page channel state.
 *
 *----------------------------------------------------------------------------
 */

static int
VPageChannelCreateQueuePair(VPageChannel *channel) // IN/OUT
{
   int err;
   uint32 flags;

   ASSERT(channel);
   ASSERT(VMCI_HANDLE_INVALID(channel->qpHandle));
   ASSERT(NULL == channel->qpair);
   ASSERT(VMCI_INVALID_ID == channel->detachSubId);
   ASSERT(VMCI_INVALID_ID == channel->attachSubId);

   if (channel->flags & VPAGECHANNEL_FLAGS_SEND_WHILE_ATOMIC ||
       !(channel->flags & VPAGECHANNEL_FLAGS_RECV_DELAYED)) {
      channel->useSpinLock = TRUE;
      spin_lock_init(&channel->qpSendLock);
      spin_lock_init(&channel->qpRecvLock);
   } else {
      sema_init(&channel->qpSendMutex, 1);
      sema_init(&channel->qpRecvMutex, 1);
   }

   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_PEER_ATTACH,
                             VMCI_FLAG_EVENT_NONE,
                             VPageChannelPeerAttachCB,
                             channel, &channel->attachSubId);
   if (err < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to subscribe to attach event "
                    "(channel=%p) (err=%d).\n",
                    channel,
                    err));
      goto error;
   }

   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_PEER_DETACH,
                             VMCI_FLAG_EVENT_NONE,
                             VPageChannelPeerDetachCB,
                             channel, &channel->detachSubId);
   if (err < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to subscribe to detach event "
                    "(channel=%p) (err=%d).\n",
                    channel,
                    err));
      goto error;
   }

   if (channel->useSpinLock) {
      flags = VMCI_QPFLAG_NONBLOCK | VMCI_QPFLAG_PINNED;
   } else {
      flags = 0;
   }
   err = VMCIQPair_Alloc(&channel->qpair, &channel->qpHandle,
                         channel->produceQSize, channel->consumeQSize,
                         VMCI_HOST_CONTEXT_ID, flags, VMCI_NO_PRIVILEGE_FLAGS);
   if (err < 0) {
      VMCI_WARNING((LGPFX"Could not create queue pair (err=%d).\n",
                    err));
      goto error;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Allocated queuepair (channel=%p) "
                   "(qp handle=0x%x:0x%x) "
                   "(produce=%"FMT64"u) (consume=%"FMT64"u).\n",
                   channel,
                   channel->qpHandle.context,
                   channel->qpHandle.resource,
                   channel->produceQSize,
                   channel->consumeQSize));

   return VMCI_SUCCESS;

error:
   VPageChannelDestroyQueuePair(channel);
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannel_CreateInVM --
 *
 *     Create a page channel in the guest kernel.
 *
 * Results:
 *     VMCI_SUCCESS if created, negative errno value otherwise.
 *
 * Side effects:
 *     May set the receive buffers if a default size is given.
 *
 *-----------------------------------------------------------------------------
 */

int
VPageChannel_CreateInVM(VPageChannel **channel,              // IN/OUT
                        VMCIId resourceId,                   // IN
                        VMCIId peerResourceId,               // IN
                        uint64 produceQSize,                 // IN
                        uint64 consumeQSize,                 // IN
                        uint32 channelFlags,                 // IN
                        VPageChannelRecvCB recvCB,           // IN
                        void *clientRecvData,                // IN
                        VPageChannelAllocElemFn elemAllocFn, // IN
                        void *allocClientData,               // IN
                        VPageChannelFreeElemFn elemFreeFn,   // IN
                        void *freeClientData,                // IN
                        int defaultRecvBuffers,              // IN
                        int maxRecvBuffers)                  // IN
{
   int retval;
   int flags;
   VPageChannel *pageChannel;

   ASSERT(channel);
   ASSERT(VMCI_INVALID_ID != resourceId);
   ASSERT(VMCI_INVALID_ID != peerResourceId);
   ASSERT(recvCB);

   if (channelFlags & ~(VPAGECHANNEL_FLAGS_ALL)) {
      VMCI_WARNING((LGPFX"Invalid argument (flags=0x%x).\n",
                    channelFlags));
      return VMCI_ERROR_INVALID_ARGS;
   }

   pageChannel =
      VMCI_AllocKernelMem(sizeof *pageChannel, VMCI_MEMORY_NONPAGED);
   if (!pageChannel) {
      VMCI_WARNING((LGPFX"Failed to allocate channel memory.\n"));
      return VMCI_ERROR_NO_MEM;
   }

   /*
    * XXX, we should support a default internal allocation function.
    */

   memset(pageChannel, 0, sizeof *pageChannel);
   pageChannel->dgHandle = VMCI_INVALID_HANDLE;
   pageChannel->attachSubId = VMCI_INVALID_ID;
   pageChannel->detachSubId = VMCI_INVALID_ID;
   pageChannel->qpHandle = VMCI_INVALID_HANDLE;
   pageChannel->qpair = NULL;
   pageChannel->doorbellHandle = VMCI_INVALID_HANDLE;
   pageChannel->peerDoorbellHandle = VMCI_INVALID_HANDLE;
   pageChannel->qpConnected = FALSE;
   pageChannel->flags = channelFlags;
   pageChannel->recvCB = recvCB;
   pageChannel->clientRecvData = clientRecvData;
   pageChannel->elemAllocFn = elemAllocFn;
   pageChannel->allocClientData = allocClientData;
   pageChannel->elemFreeFn = elemFreeFn;
   pageChannel->freeClientData = freeClientData;
   pageChannel->resourceId = resourceId;
   pageChannel->peerDgHandle = VMCI_MAKE_HANDLE(VMCI_HOST_CONTEXT_ID,
                                                  peerResourceId);
   pageChannel->curRecvBufs = 0;
   pageChannel->recvBufsTarget = defaultRecvBuffers;
   pageChannel->defaultRecvBufs = defaultRecvBuffers;
   pageChannel->maxRecvBufs = maxRecvBuffers + VMCI_PACKET_RECV_THRESHOLD;
   pageChannel->produceQSize = produceQSize;
   pageChannel->consumeQSize = consumeQSize;

   /*
    * Create a datagram handle over which we will connection handshake packets
    * (once the queuepair is created we can send packets over that instead).
    * This handle has a delayed callback regardless of the channel flags,
    * because we may have to create a queuepair inside the callback.
    */

   flags = VMCI_FLAG_DG_DELAYED_CB;
   retval = VMCIDatagram_CreateHnd(resourceId, flags,
                                   VPageChannelDgRecvFunc, pageChannel,
                                   &pageChannel->dgHandle);
   if (retval < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to create datagram handle "
                    "(channel=%p) (err=%d).\n",
                    channel,
                    retval));
      goto error;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Created datagram (channel=%p) "
                   "(handle=0x%x:0x%x).\n",
                   channel,
                   pageChannel->dgHandle.context,
                   pageChannel->dgHandle.resource));

   /*
    * Create a doorbell handle.  This is used by the peer to signal the
    * arrival of packets in the queuepair.  This handle has a delayed
    * callback depending on the channel flags.
    */

   flags = channelFlags & VPAGECHANNEL_FLAGS_RECV_DELAYED ?
      VMCI_FLAG_DELAYED_CB : 0;
   retval = VMCIDoorbell_Create(&pageChannel->doorbellHandle,
                                flags, VMCI_PRIVILEGE_FLAG_RESTRICTED,
                                VPageChannelDoorbellCallback, pageChannel);
   if (retval < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to create doorbell "
                    "(channel=%p) (err=%d).\n",
                    channel,
                    retval));
      goto error;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Created doorbell (channel=%p) "
                   "(handle=0x%x:0x%x).\n",
                   channel,
                   pageChannel->doorbellHandle.context,
                   pageChannel->doorbellHandle.resource));

   /*
    * Now create the queuepair, over which we can pass data packets.
    */

   retval = VPageChannelCreateQueuePair(pageChannel);
   if (retval < VMCI_SUCCESS) {
      goto error;
   }

   /*
    * Set the receiving buffers before sending the connection message to
    * avoid a race when the connection is made, but there is no receiving
    * buffer yet.
    */

   if (defaultRecvBuffers) {
      int numElems = defaultRecvBuffers + VMCI_PACKET_RECV_THRESHOLD;
      retval = VPageChannelSetRecvBuffers(pageChannel, numElems, TRUE);
      if (retval < VMCI_SUCCESS) {
         goto error;
      }
   }

   retval = VPageChannelSendConnectionMessage(pageChannel);
   if (retval < VMCI_SUCCESS) {
      goto error;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Created (channel=%p) (handle=0x%x:0x%x).\n",
                   pageChannel,
                   pageChannel->dgHandle.context,
                   pageChannel->dgHandle.resource));

   *channel = pageChannel;

   return retval;

 error:
   VPageChannel_Destroy(pageChannel);
   return retval;
}
EXPORT_SYMBOL(VPageChannel_CreateInVM);


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannel_Destroy --
 *
 *      Destroy the page channel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VPageChannel_Destroy(VPageChannel *channel) // IN/OUT
{
   ASSERT(channel);

   VPageChannelDestroyQueuePair(channel);

   if (!VMCI_HANDLE_INVALID(channel->doorbellHandle)) {
      VMCIDoorbell_Destroy(channel->doorbellHandle);
   }

   if (!VMCI_HANDLE_INVALID(channel->dgHandle)) {
      VMCIDatagram_DestroyHnd(channel->dgHandle);
   }

   VMCI_FreeKernelMem(channel, sizeof *channel);

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Destroyed (channel=%p).\n",
                   channel));
}
EXPORT_SYMBOL(VPageChannel_Destroy);


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelAllocDatagram --
 *
 *      Allocate a datagram for the packet.  This is only used until the
 *      connection is made; after that, packets are passed over the queuepair.
 *
 *      XXX, this function is now identical to the one of the same name in
 *      modules/vmkernel/vmci/vmciPacketVMK.c.  We should share this code.
 *
 * Results:
 *      VMCI_SUCCESS and a datagram if successfully allocated, negative error
 *      value on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelAllocDatagram(VPageChannel *channel,       // IN
                          size_t messageLen,           // IN
                          int numElems,                // IN
                          VMCIDatagram **outDg)        // OUT
{
   size_t size;
   VMCIDatagram *dg;

   ASSERT(channel);
   ASSERT(outDg);

   *outDg = NULL;

   size = VMCI_DG_HEADERSIZE + sizeof(VPageChannelPacket) + messageLen +
      numElems * sizeof (VPageChannelElem);

   if (size > VMCI_MAX_DG_SIZE) {
      VMCI_WARNING((LGPFX"Requested datagram size too large (channel=%p) "
                   "(size=%"FMTSZ"u).",
                   channel,
                   size));
      return VMCI_ERROR_PAYLOAD_TOO_LARGE;
   }

   dg = VMCI_AllocKernelMem(size, VMCI_MEMORY_ATOMIC);
   if (!dg) {
      VMCI_WARNING((LGPFX"Failed to allocate datagram (channel=%p).",
                    channel));
      return VMCI_ERROR_NO_MEM;
   }

   memset(dg, 0, size);
   dg->dst = channel->peerDgHandle;
   dg->src = channel->dgHandle;
   dg->payloadSize = size - VMCI_DG_HEADERSIZE;

   /* Chatty. */
   // VMCI_DEBUG_LOG(10,
   //                (LGPFX"Allocated datagram (payload=%"FMT64"u).\n",
   //                 dg->payloadSize));

   *outDg = dg;

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannelSendControl --
 *
 *      A packet is constructed to send the message and buffer to the guest
 *      via the control channel (datagram).  This is only necessary until the
 *      queuepair is connected.
 *
 * Results:
 *      VMCI_SUCCESS if the packet is sent successfully, negative error number
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelSendControl(VPageChannel *channel,       // IN
                        char *message,               // IN
                        int len,                     // IN
                        VPageChannelPacketType type, // IN
                        int numElems,                // IN
                        VPageChannelElem *elems)     // IN
{
   int retval;
   VPageChannelPacket *packet;
   VMCIDatagram *dg;

   ASSERT(channel);
   ASSERT(type == VPCPacket_Data ||
          type == VPCPacket_GuestConnect ||
          type == VPCPacket_SetRecvBuffer);

   dg = NULL;
   retval = VPageChannelAllocDatagram(channel, len, numElems, &dg);
   if (retval < VMCI_SUCCESS) {
      return retval;
   }

   packet = (VPageChannelPacket *)VMCI_DG_PAYLOAD(dg);
   packet->type = type;
   packet->msgLen = len;
   packet->numElems = numElems;

   if (len) {
      ASSERT(message);
      memcpy(VPAGECHANNEL_PACKET_MESSAGE(packet), message, len);
   }

   if (numElems) {
      ASSERT(elems);
      memcpy(VPAGECHANNEL_PACKET_ELEMS(packet), elems,
             numElems * sizeof (VPageChannelElem));
   }

   retval = VMCIDatagram_Send(dg);
   if (retval < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to send packet (channel=%p) to "
                    "(handle=0x%x:0x%x) (err=%d).\n",
                    channel,
                    dg->dst.context,
                    dg->dst.resource,
                    retval));
   } else {
      /*
       * We don't care about how many bytes were sent, and callers may not
       * expect > 0 to mean success, so just convert to exactly success.
       */

      retval = VMCI_SUCCESS;
   }

   VMCI_FreeKernelMem(dg, VMCI_DG_SIZE(dg));

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannel_SendPacket --
 *
 *     Send a VMCI packet to the hypervisor.
 *
 *     XXX, this is now identical to the function of the same name in
 *     modules/vmkernel/vmci/vmciPacketVMK.c.  We should share this code.
 *
 * Results:
 *     VMCI_SUCCESS if sent, negative error number otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

int
VPageChannel_SendPacket(VPageChannel *channel,         // IN
                        VPageChannelPacket *packet)    // IN
{
   ssize_t totalSize, sentSize, curSize;
   ssize_t freeSpace;
   unsigned long flags;

   ASSERT(channel);

   if (!channel->qpConnected) {
      VMCI_WARNING((LGPFX"Not connected (channel=%p).\n",
                    channel));
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   ASSERT(packet);

   totalSize = sizeof(VPageChannelPacket) + packet->msgLen +
      packet->numElems * sizeof(VPageChannelElem);

   VPageChannelAcquireSendLock(channel, &flags);

   freeSpace = VMCIQPair_ProduceFreeSpace(channel->qpair);
   if (freeSpace < totalSize) {
      VMCI_WARNING((LGPFX"No free space in queuepair (channel=%p) "
                    "(required=%"FMTSZ"d) (actual=%"FMTSZ"d).\n",
                    channel,
                    totalSize,
                    freeSpace));
      VPageChannelReleaseSendLock(channel, flags);
      return VMCI_ERROR_NO_MEM;
   }

   sentSize = VMCIQPair_Enqueue(channel->qpair, packet, totalSize, 0);
   curSize = VMCIQPair_ProduceBufReady(channel->qpair);

   if (curSize == sentSize) {
      int retval;
      retval = VMCIDoorbell_Notify(channel->peerDoorbellHandle,
                                   /* XXX, TRUSTED for VMKernel. */
                                   VMCI_PRIVILEGE_FLAG_RESTRICTED);
      if (retval < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to notify doorbell (channel=%p) "
                       "(handle=0x%x:0x%x) (err=%d).\n",
                       channel,
                       channel->peerDoorbellHandle.context,
                       channel->peerDoorbellHandle.resource,
                       retval));
         VPageChannelReleaseSendLock(channel, flags);
         return retval;
      }
   }

   VPageChannelReleaseSendLock(channel, flags);

   if (sentSize < totalSize) {
      /*
       * XXX, deal with partial sending.
       */

      VMCI_WARNING((LGPFX"No free space in queuepair (channel=%p) "
                    "(required=%"FMTSZ"d) (actual=%"FMTSZ"d).\n",
                    channel,
                    totalSize,
                    sentSize));
      return VMCI_ERROR_NO_MEM;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Sent packet (channel=%p) (size=%"FMTSZ"d).\n",
                   channel,
                   sentSize));

   return VMCI_SUCCESS;
}
EXPORT_SYMBOL(VPageChannel_SendPacket);


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannel_Send --
 *
 *      A packet is constructed to send the message and buffer to the guest.
 *
 *      XXX, this is now identical to the function of the same name in
 *      modules/vmkernel/vmci/vmciPacketVMK.c.  We should share this code.
 *
 * Results:
 *      VMCI_SUCCESS if the packet is sent successfully, negative error number
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VPageChannel_Send(VPageChannel *channel,       // IN/OUT
                  VPageChannelPacketType type, // IN
                  char *message,               // IN
                  int len,                     // IN
                  VPageChannelBuffer *buffer)  // IN
{
   int retval;
   int numElems;
   ssize_t totalSize;
   VPageChannelPacket *packet;

   ASSERT(channel);

   if (!channel->qpConnected) {
      VMCI_WARNING((LGPFX"Not connected (channel=%p).\n",
                    channel));
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   if (buffer) {
      numElems = buffer->numElems;
   } else {
      numElems = 0;
   }

   totalSize = sizeof(VPageChannelPacket) + len +
      numElems * sizeof(VPageChannelElem);
   packet = (VPageChannelPacket *)
      VMCI_AllocKernelMem(totalSize, VMCI_MEMORY_NORMAL);
   if (!packet) {
      VMCI_WARNING((LGPFX"Failed to allocate packet (channel=%p) "
                    "(size=%"FMTSZ"d).",
                    channel,
                    totalSize));
      return VMCI_ERROR_NO_MEM;
   }

   packet->type = type;
   packet->msgLen = len;
   packet->numElems = numElems;

   if (len) {
      ASSERT(message);
      memcpy(VPAGECHANNEL_PACKET_MESSAGE(packet), message, len);
   }

   if (numElems) {
      ASSERT(buffer);
      ASSERT(buffer->elems);
      memcpy(VPAGECHANNEL_PACKET_ELEMS(packet), buffer->elems,
             numElems * sizeof (VPageChannelElem));
   }

   retval = VPageChannel_SendPacket(channel, packet);

   VMCI_FreeKernelMem(packet, totalSize);

   return retval;
}
EXPORT_SYMBOL(VPageChannel_Send);


/*
 *-----------------------------------------------------------------------------
 *
 * VPageChannel_PollTx --
 *
 *      The caller does its own coalescing and notifies us that it starts tx.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      We do not do our own coalescing.
 *
 *-----------------------------------------------------------------------------
 */

void
VPageChannel_PollRecvQ(VPageChannel *channel)     // IN
{
   if (channel->qpConnected) {
      VPageChannelDoDoorbellCallback(channel);
   }
}
EXPORT_SYMBOL(VPageChannel_PollRecvQ);

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
 * Maximum number of elements per DGRAM packet (for setting receive buffers).
 */

#define VMCI_PACKET_DGRAM_MAX_ELEMS \
   ((VMCI_MAX_DG_PAYLOAD_SIZE - sizeof(VPageChannelPacket)) / \
    sizeof(VPageChannelElem))

/*
 * Maximum number of elements in a PAGE-sized packet (as above).
 */

#define VMCI_PACKET_PAGE_MAX_ELEMS \
   ((PAGE_SIZE - sizeof(VPageChannelPacket)) / \
    sizeof(VPageChannelElem))

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
   VPageChannelState state;

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

   Atomic_Int curRecvBufs;
   int recvBufsTarget;
   int defaultRecvBufs;
   int maxRecvBufs;

   VMCIId resourceId;
   VMCIHandle peerDgHandle;

   Bool inPoll;
};


static int VPageChannelSendControl(VPageChannel *channel,
                                   VPageChannelPacketType type,
                                   char *message, int len, int numElems,
                                   VPageChannelElem *elems);
static int VPageChannelSignal(VPageChannel *channel);
static int VPageChannelSendPacket(VPageChannel *channel,
                                  VPageChannelPacket *packet,
                                  Bool needsLock, Bool signalPending);

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
 * VPageChannelAddRecvBuffers --
 *
 *      Add receiving buffers for the channel.  This will ask the client to
 *      to allocate the required elements and then pass those to the peer.
 *
 *      If "onInit" is TRUE (which is is during channel initialization) then
 *      the DGRAM control channel will be used, and multiple packets will be
 *      sent if necessary.  Also, the packet allocation will be blocking.
 *
 *      If "onInit" is FALSE, then the queuepair will be used, multiple
 *      packets may be sent, and the packet allocation may be atomic,
 *      depending on how the channel is configured.
 *
 * Results:
 *      The number of buffers actually sent to the peer.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelAddRecvBuffers(VPageChannel *channel,     // IN
                           int numElems,              // IN
                           Bool onInit)               // IN
{
   int n;
   int sent;
   int maxElems;
   Bool isAtomic;
   size_t size;
   unsigned long flags;
   VPageChannelElem *elems;
   VPageChannelPacket *packet;

   ASSERT(channel);

   sent = 0;
   size = 0;
   elems = NULL;
   packet = NULL;

   if (onInit || (channel->flags & VPAGECHANNEL_FLAGS_RECV_DELAYED)) {
      /*
       * If we are initializing the channel, or we are running in a delayed
       * context (recv() in this case), then we can using blocking allocation
       * and we can allocate large packets.  Also, no need to take the
       * send lock here, we can just take it for each packet.
       */

      isAtomic = FALSE;
      maxElems = VMCI_PACKET_DGRAM_MAX_ELEMS;
      flags = 0; /* Silence compiler. */
   } else {
      /*
       * We're in an atomic context.  We must allocate page-sized packets
       * atomically and send them over the queuepair.  Since this can
       * cause a lot of signalling, we optimize by taking the send lock
       * once for all packets, and only signalling when we are done.
       */

      isAtomic = TRUE;
      maxElems = VMCI_PACKET_PAGE_MAX_ELEMS;
      VPageChannelAcquireSendLock(channel, &flags);
   }

   n = min_t(int, maxElems, numElems);
   while (n > 0) {
      int retval;
      int allocNum;

      /*
       * First packet is always big enough to cover any remaining elements,
       * so just allocate it once.
       */

      if (NULL == packet) {
         size = sizeof(VPageChannelPacket) + (n * sizeof(VPageChannelElem));
         packet = (VPageChannelPacket *)
            VMCI_AllocKernelMem(size,
                        isAtomic ? VMCI_MEMORY_ATOMIC : VMCI_MEMORY_NORMAL);
         if (packet == NULL) {
            VMCI_WARNING((LGPFX"Failed to allocate packet (channel=%p) "
                          "(size=%"FMTSZ"u).\n",
                          channel,
                          size));
            goto exit;
         }

         packet->type = VPCPacket_SetRecvBuffer;
         packet->msgLen = 0;
         elems = VPAGECHANNEL_PACKET_ELEMS(packet);
      }

      allocNum = channel->elemAllocFn(channel->allocClientData, elems, n);
      if (0 == allocNum) {
         /*
          * If the client failed to allocate any elements at all then just
          * bail out and return whatever number we managed to send so far
          * (if any).
          */

         VMCI_WARNING((LGPFX"Failed to allocate receive buffer (channel=%p) "
                       "(expected=%d).\n",
                       channel,
                       n));
         goto exit;
      }

      /*
       * We wanted "n" elements, but we might only have "allocNum" because
       * that's all the client could allocate.  Pass down whatever we got.
       */

      packet->numElems = allocNum;

      if (onInit) {
         retval = VPageChannelSendControl(channel, VPCPacket_SetRecvBuffer,
                                          NULL, 0, allocNum, elems);
      } else {
         /*
          * Do not ask for the lock here if we are atomic, we take care of
          * that ourselves.  Similarly, if we are atomic then we will do our
          * own signalling, so inform the send that there is a signal already
          * pending.
          */

         retval = VPageChannelSendPacket(channel, packet,
                                     isAtomic ? FALSE : TRUE,  // needsLock
                                     isAtomic ? TRUE : FALSE); // signalPending
         /*
          * XXX, what if this is a non-blocking queuepair and we fail to
          * send because it's full and we can't wait?  Is it even worth it
          * to loop?
          */
      }
      if (retval < VMCI_SUCCESS) {
         /*
          * Failure to send is fatal.  Release the client's elements and
          * bail out.
          */

         VMCI_WARNING((LGPFX"Failed to set receive buffers (channel=%p) "
                       "(err=%d).\n",
                       channel,
                       retval));
         channel->elemFreeFn(channel->freeClientData, elems, allocNum);
         goto exit;
      }

      Atomic_Add32(&channel->curRecvBufs, allocNum);

      sent += allocNum;
      numElems -= allocNum;
      n = min_t(int, maxElems, numElems);
   }

exit:
   if (isAtomic) {
      /*
       * We're done sending packets, so now we can signal.  Even if we only
       * sent some of the requested buffers, we must signal anyway, otherwise
       * the peer won't know about the ones we did send.
       */

      (void)VPageChannelSignal(channel);
      VPageChannelReleaseSendLock(channel, flags);
   }
   if (NULL != packet) {
      VMCI_FreeKernelMem(packet, size);
   }
   return sent;
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
   int curRecvBufs;
   int recvBufsTarget;

   ASSERT(channel);
   ASSERT(packet);

   if (packet->type != VPCPacket_Data &&
       packet->type != VPCPacket_Completion_Notify &&
       packet->type != VPCPacket_RequestBuffer &&
       packet->type != VPCPacket_HyperConnect &&
       packet->type != VPCPacket_HyperDisconnect) {
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
                      Atomic_Read32(&channel->curRecvBufs),
                      channel->recvBufsTarget,
                      channel->maxRecvBufs));

      if (channel->recvBufsTarget < channel->maxRecvBufs) {
         recvBufsTarget = channel->recvBufsTarget + channel->defaultRecvBufs;
      }
      break;

   case VPCPacket_Data:
      channel->recvCB(channel->clientRecvData, packet);
      Atomic_Sub32(&channel->curRecvBufs, packet->numElems);
      ASSERT(Atomic_Read32(&channel->curRecvBufs) > 0);
      break;

   case VPCPacket_Completion_Notify:
      channel->recvCB(channel->clientRecvData, packet);
      break;

   case VPCPacket_HyperDisconnect:
      VMCI_DEBUG_LOG(10,
                     (LGPFX"Hypervisor requested disconnection "
                      "(channel=%p) (numElems=%d).\n",
                      channel,
                      packet->numElems));
      if (packet->numElems > 0) {
         channel->elemFreeFn(channel->freeClientData,
                             VPAGECHANNEL_PACKET_ELEMS(packet),
                             packet->numElems);
      }
      (void)VPageChannelSendControl(channel, VPCPacket_GuestDisconnect,
                                    NULL, 0, 0, NULL);
      if (channel->state < VPCState_Disconnecting) {
         channel->state = VPCState_Disconnecting;
      }
      return VMCI_SUCCESS;

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

   curRecvBufs = Atomic_Read32(&channel->curRecvBufs);
   if (curRecvBufs < (recvBufsTarget - VMCI_PACKET_RECV_THRESHOLD)) {
      int numElems = recvBufsTarget + VMCI_PACKET_RECV_THRESHOLD - curRecvBufs;

      (void)VPageChannelAddRecvBuffers(channel, numElems, FALSE);
      channel->recvBufsTarget = recvBufsTarget;
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

   if (VPCState_Connected != channel->state) {
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
   while (vmci_qpair_consume_buf_ready(channel->qpair) >= sizeof packetHeader) {
      ssize_t retSize, totalSize;
      VPageChannelPacket *packet;

      retSize = vmci_qpair_peek(channel->qpair, &packetHeader,
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

      retSize = vmci_qpair_consume_buf_ready(channel->qpair);
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

      retSize = vmci_qpair_dequeue(channel->qpair, packet,
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

   if (vmci_qpair_consume_buf_ready(channel->qpair) >= sizeof packetHeader) {
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

   channel->state = VPCState_Connecting;

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

   return VPageChannelSendControl(channel, VPCPacket_GuestConnect,
                                  (char *)&message, sizeof message, 0, NULL);
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
      channel->state = VPCState_Connected;
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
      channel->state = VPCState_Disconnected;
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
      vmci_event_unsubscribe(channel->attachSubId);
      channel->attachSubId = VMCI_INVALID_ID;
   }

   if (channel->detachSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(channel->detachSubId);
      channel->detachSubId = VMCI_INVALID_ID;
   }

   if (!VMCI_HANDLE_INVALID(channel->qpHandle)) {
      ASSERT(channel->qpair);
      vmci_qpair_detach(&channel->qpair);
      channel->qpHandle = VMCI_INVALID_HANDLE;
      channel->qpair = NULL;
   }

   channel->state = VPCState_Disconnected;
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

   err = vmci_event_subscribe(VMCI_EVENT_QP_PEER_ATTACH,
#if !defined(linux) || defined(VMKERNEL)
                              VMCI_FLAG_EVENT_NONE,
#endif // !linux || VMKERNEL
                              VPageChannelPeerAttachCB,
                              channel, &channel->attachSubId);
   if (err < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to subscribe to attach event "
                    "(channel=%p) (err=%d).\n",
                    channel,
                    err));
      goto error;
   }

   err = vmci_event_subscribe(VMCI_EVENT_QP_PEER_DETACH,
#if !defined(linux) || defined(VMKERNEL)
                              VMCI_FLAG_EVENT_NONE,
#endif // !linux || VMKERNEL
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
   err = vmci_qpair_alloc(&channel->qpair, &channel->qpHandle,
                          channel->produceQSize, channel->consumeQSize,
                          VMCI_HOST_CONTEXT_ID, flags,
                          VMCI_NO_PRIVILEGE_FLAGS);
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
   pageChannel->state = VPCState_Unconnected;
   pageChannel->dgHandle = VMCI_INVALID_HANDLE;
   pageChannel->attachSubId = VMCI_INVALID_ID;
   pageChannel->detachSubId = VMCI_INVALID_ID;
   pageChannel->qpHandle = VMCI_INVALID_HANDLE;
   pageChannel->qpair = NULL;
   pageChannel->doorbellHandle = VMCI_INVALID_HANDLE;
   pageChannel->peerDoorbellHandle = VMCI_INVALID_HANDLE;
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
   Atomic_Write32(&pageChannel->curRecvBufs, 0);
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
   retval = vmci_datagram_create_handle(resourceId, flags,
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
   retval = vmci_doorbell_create(&pageChannel->doorbellHandle,
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
      if (0 == VPageChannelAddRecvBuffers(pageChannel, numElems, TRUE)) {
         /*
          * AddRecvBuffers() returns the number of buffers actually added.  If
          * we failed to add any at all, then fail.
          */

         retval = VMCI_ERROR_NO_MEM;
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
      vmci_doorbell_destroy(channel->doorbellHandle);
   }

   if (!VMCI_HANDLE_INVALID(channel->dgHandle)) {
      vmci_datagram_destroy_handle(channel->dgHandle);
   }

   channel->state = VPCState_Free;
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
                        VPageChannelPacketType type, // IN
                        char *message,               // IN
                        int len,                     // IN
                        int numElems,                // IN
                        VPageChannelElem *elems)     // IN
{
   int retval;
   VPageChannelPacket *packet;
   VMCIDatagram *dg;

   ASSERT(channel);
   ASSERT(type == VPCPacket_Data ||
          type == VPCPacket_GuestConnect ||
          type == VPCPacket_SetRecvBuffer ||
          type == VPCPacket_GuestDisconnect);

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

   retval = vmci_datagram_send(dg);
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
 * VPageChannelSignal --
 *
 *     Signal the channel's peer via the doorbell.
 *
 * Results:
 *     VMCI_SUCCESS if signalled, negative error number otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelSignal(VPageChannel *channel) // IN
{
   int retval;

   ASSERT(channel);

   retval = vmci_doorbell_notify(channel->peerDoorbellHandle,
                                 /* XXX, TRUSTED for VMKernel. */
                                 VMCI_PRIVILEGE_FLAG_RESTRICTED);
   if (retval < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to notify doorbell (channel=%p) "
               "(handle=0x%x:0x%x) (err=%d).\n",
               channel,
               channel->peerDoorbellHandle.context,
               channel->peerDoorbellHandle.resource,
               retval));
   }

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
 *     "needsLock" indicates whether this function should acquire the send
 *     lock.  If TRUE, then it will be acquired; if FALSE, then it is the
 *     caller's responsibility.  This is internal only.
 *
 *     "signalPending" indicates whether the caller will take care of
 *     signalling/the caller knows that there is already a signal pending,
 *     in which case this function will not check for/send one.  This is
 *     internal only, clients cannot specify this.
 *
 * Results:
 *     VMCI_SUCCESS if sent, negative error number otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VPageChannelSendPacket(VPageChannel *channel,         // IN
                       VPageChannelPacket *packet,    // IN
                       Bool needsLock,                // IN
                       Bool signalPending)            // IN
{
   int retval;
   ssize_t totalSize, sentSize;
   ssize_t freeSpace;
   unsigned long flags;

   ASSERT(channel);

   if (VPCState_Connected != channel->state) {
      VMCI_WARNING((LGPFX"Not connected (channel=%p).\n",
                    channel));
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   ASSERT(packet);

   totalSize = sizeof(VPageChannelPacket) + packet->msgLen +
      packet->numElems * sizeof(VPageChannelElem);

   if (needsLock) {
      VPageChannelAcquireSendLock(channel, &flags);
   } else {
      flags = 0; /* Silence compiler. */
   }

   freeSpace = vmci_qpair_produce_free_space(channel->qpair);
   if (freeSpace < totalSize) {
      VMCI_WARNING((LGPFX"No free space in queuepair (channel=%p) "
                    "(required=%"FMTSZ"d) (actual=%"FMTSZ"d).\n",
                    channel,
                    totalSize,
                    freeSpace));
      retval = VMCI_ERROR_NO_MEM;
      goto exit;
   }

   sentSize = vmci_qpair_enqueue(channel->qpair, packet, totalSize, 0);

   if (!signalPending) {
      if (sentSize == vmci_qpair_produce_buf_ready(channel->qpair)) {
         retval = VPageChannelSignal(channel);
         if (retval < VMCI_SUCCESS) {
            goto exit;
         }
      }
   }

   if (sentSize < totalSize) {
      /*
       * XXX, deal with partial sending.
       */

      VMCI_WARNING((LGPFX"No free space in queuepair (channel=%p) "
                    "(required=%"FMTSZ"d) (actual=%"FMTSZ"d).\n",
                    channel,
                    totalSize,
                    sentSize));
      retval = VMCI_ERROR_NO_MEM;
      goto exit;
   }

   VMCI_DEBUG_LOG(10,
                  (LGPFX"Sent packet (channel=%p) (size=%"FMTSZ"d).\n",
                   channel,
                   sentSize));

   retval = VMCI_SUCCESS;

exit:
   if (needsLock) {
      VPageChannelReleaseSendLock(channel, flags);
   }
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
   return VPageChannelSendPacket(channel, packet, TRUE, FALSE);
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

   if (VPCState_Connected != channel->state) {
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
      VMCI_AllocKernelMem(totalSize,
                        channel->flags & VPAGECHANNEL_FLAGS_SEND_WHILE_ATOMIC ?
                        VMCI_MEMORY_ATOMIC : VMCI_MEMORY_NORMAL);
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
   if (VPCState_Connected != channel->state) {
      VPageChannelDoDoorbellCallback(channel);
   }
}
EXPORT_SYMBOL(VPageChannel_PollRecvQ);

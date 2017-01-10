/*********************************************************
 * Copyright (C) 2009-2014 VMware, Inc. All rights reserved.
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
 * notify.c --
 *
 *      Linux control notifications for the VMCI Stream Sockets protocol.
 */


#include "driver-config.h"

#include <linux/socket.h>

#include "compat_sock.h"

#include "notify.h"
#include "af_vsock.h"

#define PKT_FIELD(vsk, fieldName) \
   (vsk)->notify.pkt.fieldName

#define VSOCK_MAX_DGRAM_RESENDS       10


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyWaitingWrite --
 *
 *      Determines if the conditions have been met to notify a waiting writer.
 *
 * Results:
 *      TRUE if a notification should be sent, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciNotifyWaitingWrite(VSockVmciSock *vsk)    // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   Bool retval;
   uint64 notifyLimit;

   if (!PKT_FIELD(vsk, peerWaitingWrite)) {
      return FALSE;
   }

#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   /*
    * When the sender blocks, we take that as a sign that the sender
    * is faster than the receiver. To reduce the transmit rate of the
    * sender, we delay the sending of the read notification by
    * decreasing the writeNotifyWindow. The notification is delayed
    * until the number of bytes used in the queue drops below the
    * writeNotifyWindow.
    */

   if (!PKT_FIELD(vsk, peerWaitingWriteDetected)) {
      PKT_FIELD(vsk, peerWaitingWriteDetected) = TRUE;
      if (PKT_FIELD(vsk, writeNotifyWindow) < PAGE_SIZE) {
         PKT_FIELD(vsk, writeNotifyWindow) =
            PKT_FIELD(vsk, writeNotifyMinWindow);
      } else {
         PKT_FIELD(vsk, writeNotifyWindow) -= PAGE_SIZE;
         if (PKT_FIELD(vsk, writeNotifyWindow) <
             PKT_FIELD(vsk, writeNotifyMinWindow)) {
            PKT_FIELD(vsk, writeNotifyWindow) =
               PKT_FIELD(vsk, writeNotifyMinWindow);
         }
      }
   }
   notifyLimit = vsk->consumeSize - PKT_FIELD(vsk, writeNotifyWindow);
#else
   notifyLimit = 0;
#endif // VSOCK_OPTIMIZATION_FLOW_CONTROL

   /*
    * For now we ignore the wait information and just see if the free
    * space exceeds the notify limit.  Note that improving this
    * function to be more intelligent will not require a protocol
    * change and will retain compatibility between endpoints with
    * mixed versions of this function.
    *
    * The notifyLimit is used to delay notifications in the case where
    * flow control is enabled. Below the test is expressed in terms of
    * free space in the queue:
    *   if freeSpace > ConsumeSize - writeNotifyWindow then notify
    * An alternate way of expressing this is to rewrite the expression
    * to use the data ready in the receive queue:
    *   if writeNotifyWindow > bufferReady then notify
    * as freeSpace == ConsumeSize - bufferReady.
    */
   retval = vmci_qpair_consume_free_space(vsk->qpair) > notifyLimit;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   if (retval) {
      /*
       * Once we notify the peer, we reset the detected flag so the
       * next wait will again cause a decrease in the window size.
       */

      PKT_FIELD(vsk, peerWaitingWriteDetected) = FALSE;
   }
#endif // VSOCK_OPTIMIZATION_FLOW_CONTROL
   return retval;
#else
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyWaitingRead --
 *
v *      Determines if the conditions have been met to notify a waiting reader.
 *
 * Results:
 *      TRUE if a notification should be sent, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciNotifyWaitingRead(VSockVmciSock *vsk)  // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   if (!PKT_FIELD(vsk, peerWaitingRead)) {
      return FALSE;
   }

   /*
    * For now we ignore the wait information and just see if there is any data
    * for our peer to read.  Note that improving this function to be more intelligent will
    * not require a protocol change and will retain compatibility between
    * endpoints with mixed versions of this function.
    */
   return vmci_qpair_produce_buf_ready(vsk->qpair) > 0;
#else
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciHandleWaitingRead --
 *
 *      Handles an incoming waiting read message.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May send a notification to the peer, may update socket's wait info
 *      structure.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciHandleWaitingRead(struct sock *sk,             // IN
                           VSockPacket *pkt,            // IN
                           Bool bottomHalf,             // IN
                           struct sockaddr_vm *dst,     // IN
                           struct sockaddr_vm *src)     // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);

   PKT_FIELD(vsk, peerWaitingRead) = TRUE;
   memcpy(&PKT_FIELD(vsk, peerWaitingReadInfo), &pkt->u.wait,
          sizeof PKT_FIELD(vsk, peerWaitingReadInfo));

   if (VSockVmciNotifyWaitingRead(vsk)) {
      Bool sent;

      if (bottomHalf) {
         sent = VSOCK_SEND_WROTE_BH(dst, src) > 0;
      } else {
         sent = VSOCK_SEND_WROTE(sk) > 0;
      }

      if (sent) {
         PKT_FIELD(vsk, peerWaitingRead) = FALSE;
      }
   }
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciHandleWaitingWrite --
 *
 *      Handles an incoming waiting write message.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May send a notification to the peer, may update socket's wait info
 *      structure.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciHandleWaitingWrite(struct sock *sk,            // IN
                            VSockPacket *pkt,           // IN
                            Bool bottomHalf,            // IN
                            struct sockaddr_vm *dst,    // IN
                            struct sockaddr_vm *src)    // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);

   PKT_FIELD(vsk, peerWaitingWrite) = TRUE;
   memcpy(&PKT_FIELD(vsk, peerWaitingWriteInfo), &pkt->u.wait,
          sizeof PKT_FIELD(vsk,peerWaitingWriteInfo));

   if (VSockVmciNotifyWaitingWrite(vsk)) {
      Bool sent;

      if (bottomHalf) {
         sent = VSOCK_SEND_READ_BH(dst, src) > 0;
      } else {
         sent = VSOCK_SEND_READ(sk) > 0;
      }

      if (sent) {
         PKT_FIELD(vsk, peerWaitingWrite) = FALSE;
      }
   }
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciHandleRead --
 *
 *      Handles an incoming read message.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciHandleRead(struct sock *sk,            // IN
                    VSockPacket *pkt,           // IN: unused
                    Bool bottomHalf,            // IN: unused
                    struct sockaddr_vm *dst,    // IN: unused
                    struct sockaddr_vm *src)    // IN: unused
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);
   PKT_FIELD(vsk, sentWaitingWrite) = FALSE;
#endif

   sk->sk_write_space(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendWaitingRead --
 *
 *      Sends a waiting read notification to this socket's peer.
 *
 * Results:
 *      TRUE if the datagram is sent successfully, FALSE otherwise.
 *
 * Side effects:
 *      Our peer will notify us when there is data to read from our consume
 *      queue.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciSendWaitingRead(struct sock *sk,    // IN
                         uint64 roomNeeded)  // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;
   VSockWaitingInfo waitingInfo;
   uint64 tail;
   uint64 head;
   uint64 roomLeft;
   Bool ret;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   if (PKT_FIELD(vsk, sentWaitingRead)) {
      return TRUE;
   }

   if (PKT_FIELD(vsk, writeNotifyWindow) < vsk->consumeSize) {
      PKT_FIELD(vsk, writeNotifyWindow) =
         MIN(PKT_FIELD(vsk, writeNotifyWindow) + PAGE_SIZE,
             vsk->consumeSize);
   }

   vmci_qpair_get_consume_indexes(vsk->qpair, &tail, &head);
   roomLeft = vsk->consumeSize - head;
   if (roomNeeded >= roomLeft) {
      waitingInfo.offset = roomNeeded - roomLeft;
      waitingInfo.generation = PKT_FIELD(vsk, consumeQGeneration) + 1;
   } else {
      waitingInfo.offset = head + roomNeeded;
      waitingInfo.generation = PKT_FIELD(vsk, consumeQGeneration);
   }

   ret = VSOCK_SEND_WAITING_READ(sk, &waitingInfo) > 0;
   if (ret) {
      PKT_FIELD(vsk, sentWaitingRead) = TRUE;
   }
   return ret;
#else
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendWaitingWrite --
 *
 *      Sends a waiting write notification to this socket's peer.
 *
 * Results:
 *      TRUE if the datagram is sent successfully or does not need to be sent.
 *      FALSE otherwise.
 *
 * Side effects:
 *      Our peer will notify us when there is room to write in to our produce
 *      queue.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciSendWaitingWrite(struct sock *sk,   // IN
                          uint64 roomNeeded) // IN
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;
   VSockWaitingInfo waitingInfo;
   uint64 tail;
   uint64 head;
   uint64 roomLeft;
   Bool ret;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   if (PKT_FIELD(vsk, sentWaitingWrite)) {
      return TRUE;
   }

   vmci_qpair_get_produce_indexes(vsk->qpair, &tail, &head);
   roomLeft = vsk->produceSize - tail;
   if (roomNeeded + 1 >= roomLeft) {
      /* Wraps around to current generation. */
      waitingInfo.offset = roomNeeded + 1 - roomLeft;
      waitingInfo.generation = PKT_FIELD(vsk, produceQGeneration);
   } else {
      waitingInfo.offset = tail + roomNeeded + 1;
      waitingInfo.generation = PKT_FIELD(vsk, produceQGeneration) - 1;
   }

   ret = VSOCK_SEND_WAITING_WRITE(sk, &waitingInfo) > 0;
   if (ret) {
      PKT_FIELD(vsk, sentWaitingWrite) = TRUE;
   }
   return ret;
#else
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendReadNotification --
 *
 *      Sends a read notification to this socket's peer.
 *
 * Results:
 *      >= 0 if the datagram is sent successfully, negative error value
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciSendReadNotification(struct sock *sk)  // IN
{
   VSockVmciSock *vsk;
   Bool sentRead;
   unsigned int retries;
   int err;

   ASSERT(sk);

   vsk = vsock_sk(sk);
   sentRead = FALSE;
   retries = 0;
   err = 0;

   if (VSockVmciNotifyWaitingWrite(vsk)) {
      /*
       * Notify the peer that we have read, retrying the send on failure up to our
       * maximum value.  XXX For now we just log the failure, but later we should
       * schedule a work item to handle the resend until it succeeds.  That would
       * require keeping track of work items in the vsk and cleaning them up upon
       * socket close.
       */
      while (!(vsk->peerShutdown & RCV_SHUTDOWN) &&
             !sentRead &&
             retries < VSOCK_MAX_DGRAM_RESENDS) {
         err = VSOCK_SEND_READ(sk);
         if (err >= 0) {
            sentRead = TRUE;
         }

         retries++;
      }

      if (retries >= VSOCK_MAX_DGRAM_RESENDS) {
         Warning("unable to send read notification to peer for socket %p.\n", sk);
      } else {
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
         PKT_FIELD(vsk, peerWaitingWrite) = FALSE;
#endif
      }
   }
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciHandleWrote --
 *
 *      Handles an incoming wrote message.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciHandleWrote(struct sock *sk,            // IN
                     VSockPacket *pkt,           // IN: unused
                     Bool bottomHalf,            // IN: unused
                     struct sockaddr_vm *dst,    // IN: unused
                     struct sockaddr_vm *src)    // IN: unused
{
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);
   PKT_FIELD(vsk, sentWaitingRead) = FALSE;
#endif

   sk->sk_data_ready(sk, 0);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktSocketInit --
 *
 *      Function that is called after a socket is created and before any
 *      notify ops are used.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciNotifyPktSocketInit(struct sock *sk) // IN
{
   VSockVmciSock *vsk;
   vsk = vsock_sk(sk);

   PKT_FIELD(vsk, writeNotifyWindow) = PAGE_SIZE;
   PKT_FIELD(vsk, writeNotifyMinWindow) = PAGE_SIZE;
   PKT_FIELD(vsk, peerWaitingRead) = FALSE;
   PKT_FIELD(vsk, peerWaitingWrite) = FALSE;
   PKT_FIELD(vsk, peerWaitingWriteDetected) = FALSE;
   PKT_FIELD(vsk, sentWaitingRead) = FALSE;
   PKT_FIELD(vsk, sentWaitingWrite) = FALSE;
   PKT_FIELD(vsk, produceQGeneration) = 0;
   PKT_FIELD(vsk, consumeQGeneration) = 0;

   memset(&PKT_FIELD(vsk, peerWaitingReadInfo), 0,
          sizeof PKT_FIELD(vsk, peerWaitingReadInfo));
   memset(&PKT_FIELD(vsk, peerWaitingWriteInfo), 0,
          sizeof PKT_FIELD(vsk, peerWaitingWriteInfo));
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktSocketDestruct --
 *
 *      Function that is called when the socket is being released.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciNotifyPktSocketDestruct(struct sock *sk) // IN
{
   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktPollIn --
 *
 *      Called by the poll function to figure out if there is data to read
 *      and to setup future notifications if needed. Only called on sockets
 *      that aren't shutdown for recv.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktPollIn(struct sock *sk,    // IN
                         size_t target,      // IN
                         Bool *dataReadyNow) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(dataReadyNow);

   vsk = vsock_sk(sk);

   if (VSockVmciStreamHasData(vsk)) {
      *dataReadyNow = TRUE;
   } else {
      /*
       * We can't read right now because there is nothing in the queue.
       * Ask for notifications when there is something to read.
       */
      if (sk->sk_state == SS_CONNECTED) {
         if (!VSockVmciSendWaitingRead(sk, 1)) {
            return -1;
         }
      }
      *dataReadyNow = FALSE;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktPollOut
 *
 *      Called by the poll function to figure out if there is space to write
 *      and to setup future notifications if needed. Only called on a
 *      connected socket that isn't shutdown for send.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktPollOut(struct sock *sk,     // IN
                          size_t target,       // IN
                          Bool *spaceAvailNow) // IN
{
   int64 produceQFreeSpace;
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(spaceAvailNow);

   vsk = vsock_sk(sk);

   produceQFreeSpace =
      VSockVmciStreamHasSpace(vsk);
   if (produceQFreeSpace > 0) {
      *spaceAvailNow = TRUE;
      return 0;
   } else if (produceQFreeSpace == 0) {
      /*
       * This is a connected socket but we can't currently send data. Notify
       * the peer that we are waiting if the queue is full.
       * We only send a waiting write if the queue is full because otherwise
       * we end up in an infinite WAITING_WRITE, READ, WAITING_WRITE, READ, etc.
       * loop. Treat failing to send the notification as a socket error, passing
       * that back through the mask.
       */
      if (!VSockVmciSendWaitingWrite(sk, 1)) {
         return -1;
      }
      *spaceAvailNow = FALSE;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktRecvInit --
 *
 *      Called at the start of a stream recv call with the socket lock held.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktRecvInit(struct sock *sk,               // IN
                           size_t target,                 // IN
                           VSockVmciRecvNotifyData *data) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(data);

   vsk = vsock_sk(sk);

#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   data->consumeHead = 0;
   data->produceTail = 0;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   data->notifyOnBlock = FALSE;

   if (PKT_FIELD(vsk, writeNotifyMinWindow) < target + 1) {
      ASSERT(target < vsk->consumeSize);
      PKT_FIELD(vsk, writeNotifyMinWindow) = target + 1;
      if (PKT_FIELD(vsk, writeNotifyWindow) <
          PKT_FIELD(vsk, writeNotifyMinWindow)) {
         /*
          * If the current window is smaller than the new minimal
          * window size, we need to reevaluate whether we need to
          * notify the sender. If the number of ready bytes are
          * smaller than the new window, we need to send a
          * notification to the sender before we block.
          */

         PKT_FIELD(vsk, writeNotifyWindow) =
            PKT_FIELD(vsk, writeNotifyMinWindow);
         data->notifyOnBlock = TRUE;
      }
   }
#endif
#endif

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktRecvPreBlock --
 *
 *      Called right before a socket is about to block with the socket lock
 *      held. The socket lock may have been released between the entry
 *      function and the preblock call.
 *
 *      Note: This function may be called multiple times before the post
 *      block function is called.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktRecvPreBlock(struct sock *sk,               // IN
                               size_t target,                 // IN
                               VSockVmciRecvNotifyData *data) // IN
{
   int err;

   ASSERT(sk);
   ASSERT(data);

   err = 0;

   /* Notify our peer that we are waiting for data to read. */
   if (!VSockVmciSendWaitingRead(sk, target)) {
      err = -EHOSTUNREACH;
      return err;
   }

#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   if (data->notifyOnBlock) {
      err = VSockVmciSendReadNotification(sk);
      if (err < 0) {
         return err;
      }
      data->notifyOnBlock = FALSE;
   }
#endif

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktRecvPreDequeue --
 *
 *      Called right before we dequeue / peek data from a socket.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktRecvPreDequeue(struct sock *sk,               // IN
                                 size_t target,                 // IN
                                 VSockVmciRecvNotifyData *data) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(data);

   vsk = vsock_sk(sk);

   /*
    * Now consume up to len bytes from the queue.  Note that since we have the
    * socket locked we should copy at least ready bytes.
    */
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   vmci_qpair_get_consume_indexes(vsk->qpair,
                                  &data->produceTail,
                                  &data->consumeHead);
#endif

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktRecvPostDequeue --
 *
 *      Called right after we dequeue / peek data from a socket.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktRecvPostDequeue(struct sock *sk,               // IN
                                  size_t target,                 // IN
                                  ssize_t copied,                // IN
                                  Bool dataRead,                 // IN
                                  VSockVmciRecvNotifyData *data) // IN
{
   VSockVmciSock *vsk;
   int err;

   ASSERT(sk);
   ASSERT(data);

   vsk = vsock_sk(sk);
   err = 0;

   if (dataRead) {
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
      /*
       * Detect a wrap-around to maintain queue generation.  Note that this is
       * safe since we hold the socket lock across the two queue pair
       * operations.
       */
      if (copied >= vsk->consumeSize - data->consumeHead) {
         PKT_FIELD(vsk, consumeQGeneration)++;
      }
#endif

      err = VSockVmciSendReadNotification(sk);
      if (err < 0) {
         return err;
      }

   }
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktSendInit --
 *
 *      Called at the start of a stream send call with the socket lock held.
 *
 * Results:
 *      0 on success. A negative error code on failure.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktSendInit(struct sock *sk,               // IN
                           VSockVmciSendNotifyData *data) // IN
{
   ASSERT(sk);
   ASSERT(data);

#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   data->consumeHead = 0;
   data->produceTail = 0;
#endif

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktSendPreBlock --
 *
 *      Called right before a socket is about to block with the socket lock
 *      held. The socket lock may have been released between the entry
 *      function and the preblock call.
 *
 *      Note: This function may be called multiple times before the post
 *      block function is called.
 *
 * Results.
 *      0 on success. A negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktSendPreBlock(struct sock *sk,               // IN
                               VSockVmciSendNotifyData *data) // IN
{
   ASSERT(sk);
   ASSERT(data);

   /* Notify our peer that we are waiting for room to write. */
   if (!VSockVmciSendWaitingWrite(sk, 1)) {
      return -EHOSTUNREACH;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifySendPreEnqueue --
 *
 *      Called right before we Enqueue to a socket.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktSendPreEnqueue(struct sock *sk,               // IN
                                 VSockVmciSendNotifyData *data) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(data);

   vsk = vsock_sk(sk);

#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
      vmci_qpair_get_produce_indexes(vsk->qpair,
                                     &data->produceTail,
                                     &data->consumeHead);
#endif

   return 0;;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifySendPostEnqueue --
 *
 *      Called right after we enqueue data to a socket.
 *
 * Results:
 *      0 on success. Negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int32
VSockVmciNotifyPktSendPostEnqueue(struct sock *sk,               // IN
                                  ssize_t written,               // IN
                                  VSockVmciSendNotifyData *data) // IN
{
   int err = 0;
   VSockVmciSock *vsk;
   Bool sentWrote = FALSE;
   int retries = 0;

   ASSERT(sk);
   ASSERT(data);

   vsk = vsock_sk(sk);

#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
      /*
       * Detect a wrap-around to maintain queue generation.  Note that this is
       * safe since we hold the socket lock across the two queue pair
       * operations.
       */
   if (written >= vsk->produceSize - data->produceTail) {
      PKT_FIELD(vsk, produceQGeneration)++;
   }
#endif

   if (VSockVmciNotifyWaitingRead(vsk)) {
      /*
       * Notify the peer that we have written, retrying the send on failure up to
       * our maximum value. See the XXX comment for the corresponding piece of
       * code in StreamRecvmsg() for potential improvements.
       */
      while (!(vsk->peerShutdown & RCV_SHUTDOWN) &&
             !sentWrote &&
             retries < VSOCK_MAX_DGRAM_RESENDS) {
         err = VSOCK_SEND_WROTE(sk);
         if (err >= 0) {
            sentWrote = TRUE;
         }

         retries++;
      }

      if (retries >= VSOCK_MAX_DGRAM_RESENDS) {
         Warning("unable to send wrote notification to peer for socket %p.\n", sk);
         return err;
      } else {
#if defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
         PKT_FIELD(vsk, peerWaitingRead) = FALSE;
#endif
      }
   }
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktHandlePkt
 *
 *      Called when a notify packet is recieved for a socket in the connected
 *      state. Note this might be called from a bottom half.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciNotifyPktHandlePkt(struct sock *sk,         // IN
                            VSockPacket *pkt,        // IN
                            Bool bottomHalf,         // IN
                            struct sockaddr_vm *dst, // IN
                            struct sockaddr_vm *src, // IN
                            Bool *pktProcessed)      // In
{
   Bool processed = FALSE;

   ASSERT(sk);
   ASSERT(pkt);

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_WROTE:
      VSockVmciHandleWrote(sk, pkt, bottomHalf, dst, src);
      processed = TRUE;
      break;
   case VSOCK_PACKET_TYPE_READ:
      VSockVmciHandleRead(sk, pkt, bottomHalf, dst, src);
      processed = TRUE;
      break;
   case VSOCK_PACKET_TYPE_WAITING_WRITE:
      VSockVmciHandleWaitingWrite(sk, pkt, bottomHalf, dst, src);
      processed = TRUE;
      break;

   case VSOCK_PACKET_TYPE_WAITING_READ:
      VSockVmciHandleWaitingRead(sk, pkt, bottomHalf, dst, src);
      processed = TRUE;
      break;
   }

   if (pktProcessed) {
      *pktProcessed = processed;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktProcessRequest
 *
 *      Called near the end of process request.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciNotifyPktProcessRequest(struct sock *sk) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   PKT_FIELD(vsk, writeNotifyWindow) = vsk->consumeSize;
   if (vsk->consumeSize < PKT_FIELD(vsk, writeNotifyMinWindow)) {
      PKT_FIELD(vsk, writeNotifyMinWindow) = vsk->consumeSize;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNotifyPktProcessNegotiate
 *
 *      Called near the end of process negotiate.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciNotifyPktProcessNegotiate(struct sock *sk) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   PKT_FIELD(vsk, writeNotifyWindow) = vsk->consumeSize;
   if (vsk->consumeSize < PKT_FIELD(vsk, writeNotifyMinWindow)) {
      PKT_FIELD(vsk, writeNotifyMinWindow) = vsk->consumeSize;
   }
}


/* Socket control packet based operations. */
VSockVmciNotifyOps vSockVmciNotifyPktOps = {
   VSockVmciNotifyPktSocketInit,
   VSockVmciNotifyPktSocketDestruct,
   VSockVmciNotifyPktPollIn,
   VSockVmciNotifyPktPollOut,
   VSockVmciNotifyPktHandlePkt,
   VSockVmciNotifyPktRecvInit,
   VSockVmciNotifyPktRecvPreBlock,
   VSockVmciNotifyPktRecvPreDequeue,
   VSockVmciNotifyPktRecvPostDequeue,
   VSockVmciNotifyPktSendInit,
   VSockVmciNotifyPktSendPreBlock,
   VSockVmciNotifyPktSendPreEnqueue,
   VSockVmciNotifyPktSendPostEnqueue,
   VSockVmciNotifyPktProcessRequest,
   VSockVmciNotifyPktProcessNegotiate,
};

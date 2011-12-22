/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * tcp.c --
 *
 * Provides TCP channel to the HGFS client.
 *
 * Compiled conditionally.
 * Need to specify host ip at module load to enable the channel.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/kthread.h>

#include "compat_kernel.h"
#include "compat_mutex.h"
#include "compat_version.h"
#include "compat_sched.h"
#include "compat_sock.h"
#include "compat_timer.h"

#include "vm_assert.h"

#include "hgfsProto.h"
#include "hgfsDevLinux.h"
#include "module.h"
#include "transport.h"

static char *HOST_IP;
module_param(HOST_IP, charp, 0444);

static int HOST_PORT = 2000; /* Defaulted to 2000. */
module_param(HOST_PORT, int, 0444);

static int HOST_VSOCKET_PORT = 0; /* Disabled by default. */
module_param(HOST_VSOCKET_PORT, int, 0444);

#ifdef INCLUDE_VSOCKETS

#include "vmci_defs.h"
#include "vmci_sockets.h"

#else
/*
 *  At the moment I can't check in HGFS that is dependent on vsock
 *  because of unresolved installer problems. Installer need to properly handle
 *  dependecies between vmhgfs and vsock modules.
 *  Following stub functions must be removed when installer problems are resolved.
 *  Stubs for undefined functions.
 */

void
VMCISock_KernelDeregister(void)
{
}

void
VMCISock_KernelRegister(void)
{
}

#endif

/* Indicates that data is ready to be received */
#define HGFS_REQ_THREAD_RECV        (1 << 0)

/* Recv states for the recvBuffer. */
typedef enum {
   HGFS_CONN_RECV_SOCK_HDR,    /* Waiting for socket header */
   HGFS_CONN_RECV_REP_HDR,     /* Waiting for HgfsReply header */
   HGFS_CONN_RECV_REP_PAYLOAD, /* Waiting for reply payload */
} HgfsSocketRecvState;

/* HGFS receive buffer. */
typedef struct HgfsSocketRecvBuffer {
   HgfsSocketHeader header;            /* Buffer for receiving header. */
   HgfsReply reply;                    /* Buffer for receiving reply */
   HgfsReq *req;                       /* Request currently being received. */
   char sink[HGFS_PACKET_MAX];         /* Buffer for data to be discarded. */
   HgfsSocketRecvState state;          /* Reply receive state. */
   int len;                            /* Number of bytes to receive. */
   char *buf;                          /* Pointer to the buffer. */
} HgfsSocketRecvBuffer;

static HgfsSocketRecvBuffer recvBuffer;   /* Accessed only by the recv thread. */

static struct task_struct *recvThread; /* The recv thread. */
static DECLARE_WAIT_QUEUE_HEAD(hgfsRecvThreadWait); /* Wait queue for recv thread. */
static unsigned long hgfsRecvThreadFlags; /* Used to signal recv data availability. */

static void (*oldSocketDataReady)(struct sock *, int);

static Bool HgfsVSocketChannelOpen(HgfsTransportChannel *channel);
static int HgfsSocketChannelSend(HgfsTransportChannel *channel, HgfsReq *req);
static void HgfsVSocketChannelClose(HgfsTransportChannel *channel);
static Bool HgfsTcpChannelOpen(HgfsTransportChannel *channel);
static void HgfsTcpChannelClose(HgfsTransportChannel *channel);
static HgfsReq * HgfsSocketChannelAllocate(size_t payloadSize);
void HgfsSocketChannelFree(HgfsReq *req);

static HgfsTransportChannel vsockChannel = {
   .name = "vsocket",
   .ops.close = HgfsVSocketChannelClose,
   .ops.send = HgfsSocketChannelSend,
   .ops.open = HgfsVSocketChannelOpen,
   .ops.allocate = NULL,
   .ops.free = NULL,
   .priv = NULL,
   .status = HGFS_CHANNEL_NOTCONNECTED
};

static HgfsTransportChannel tcpChannel = {
   .name = "tcp",
   .ops.open = HgfsTcpChannelOpen,
   .ops.close = HgfsTcpChannelClose,
   .ops.allocate = HgfsSocketChannelAllocate,
   .ops.free = HgfsSocketChannelFree,
   .ops.send = HgfsSocketChannelSend,
   .priv = NULL,
   .status = HGFS_CHANNEL_NOTCONNECTED
};


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketDataReady --
 *
 *     Called when there is data to read on the connected socket.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Wakes up the receiving thread.
 *
 *----------------------------------------------------------------------
 */

static void
HgfsSocketDataReady(struct sock *sk,   // IN: Server socket
                    int len)           // IN: Data length
{
   LOG(4, (KERN_DEBUG "VMware hgfs: %s: data ready\n", __func__));

   /* Call the original data_ready function. */
   oldSocketDataReady(sk, len);

   /* Wake up the recv thread. */
   set_bit(HGFS_REQ_THREAD_RECV, &hgfsRecvThreadFlags);
   wake_up_interruptible(&hgfsRecvThreadWait);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketResetRecvBuffer --
 *
 *     Reset recv buffer.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsSocketResetRecvBuffer(void)
{
   recvBuffer.state = HGFS_CONN_RECV_SOCK_HDR;
   recvBuffer.req = NULL;
   recvBuffer.len = sizeof recvBuffer.header;
   recvBuffer.buf = (char *)&recvBuffer.header;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketIsReceiverIdle --
 *
 *     Checks whether we are in the middle of receiving a packet.
 *
 * Results:
 *     FALSE if receiving thread is in the middle of receiving packet,
 *     otherwise TRUE.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsSocketIsReceiverIdle(void)
{
   return recvBuffer.state == HGFS_CONN_RECV_SOCK_HDR &&
          recvBuffer.len == sizeof recvBuffer.header;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketRecvMsg --
 *
 *     Receive the message on the socket.
 *
 * Results:
 *     On success returns number of bytes received.
 *     On failure returns the negative errno.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSocketRecvMsg(struct socket *socket,   // IN: TCP socket
                  char *buffer,            // IN: Buffer to recv the message
                  size_t bufferLen)        // IN: Buffer length
{
   struct iovec iov;
   struct msghdr msg;
   int ret;
   int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
   mm_segment_t oldfs = get_fs();

   memset(&msg, 0, sizeof msg);
   msg.msg_flags = flags;
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;
   iov.iov_base = buffer;
   iov.iov_len = bufferLen;

   set_fs(KERNEL_DS);
   ret = sock_recvmsg(socket, &msg, bufferLen, flags);
   set_fs(oldfs);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelRecvAsync --
 *
 *     Receive as much data from the socket as possible without blocking.
 *     Note, that we may return early with just a part of the packet
 *     received.
 *
 * Results:
 *     On failure returns the negative errno, otherwise 0.
 *
 * Side effects:
 *     Changes state of the receive buffer depending on what part of
 *     the packet has been received so far.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSocketChannelRecvAsync(HgfsTransportChannel *channel) // IN:  Channel
{
   int ret;

   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost.\n", __func__));
      return -ENOTCONN;
   }

   /* We want to read as much data as possible without blocking */
   for (;;) {

      LOG(10, (KERN_DEBUG "VMware hgfs: %s: receiving %s\n", __func__,
               recvBuffer.state == HGFS_CONN_RECV_SOCK_HDR ? "header" :
               recvBuffer.state == HGFS_CONN_RECV_REP_HDR ? "reply" : "data"));
      ret = HgfsSocketRecvMsg(channel->priv, recvBuffer.buf, recvBuffer.len);
      LOG(10, (KERN_DEBUG "VMware hgfs: %s: sock_recvmsg returns: %d\n",
               __func__, ret));

      if (ret <= 0) {
         break;
      }

      ASSERT(ret <= recvBuffer.len);
      recvBuffer.len -= ret;
      recvBuffer.buf += ret;

      if (recvBuffer.len != 0) {
         continue;
      }

      /* Complete segment received. */
      switch (recvBuffer.state) {

      case HGFS_CONN_RECV_SOCK_HDR:
         LOG(10, (KERN_DEBUG "VMware hgfs: %s: received packet header\n",
                  __func__));
         ASSERT(recvBuffer.header.version == HGFS_SOCKET_VERSION1);
         ASSERT(recvBuffer.header.size == sizeof recvBuffer.header);
         ASSERT(recvBuffer.header.status == HGFS_SOCKET_STATUS_SUCCESS);

         recvBuffer.state = HGFS_CONN_RECV_REP_HDR;
         recvBuffer.len = sizeof recvBuffer.reply;
         recvBuffer.buf = (char *)&recvBuffer.reply;
         break;

      case HGFS_CONN_RECV_REP_HDR:
         LOG(10, (KERN_DEBUG "VMware hgfs: %s: received packet reply\n",
                  __func__));
         recvBuffer.req = HgfsTransportGetPendingRequest(recvBuffer.reply.id);
         if (recvBuffer.req) {
            ASSERT(recvBuffer.header.packetLen <= recvBuffer.req->bufferSize);
            recvBuffer.req->payloadSize = recvBuffer.header.packetLen;
            memcpy(recvBuffer.req->payload, &recvBuffer.reply, sizeof recvBuffer.reply);
            recvBuffer.buf = recvBuffer.req->payload + sizeof recvBuffer.reply;
         } else {
            recvBuffer.buf = recvBuffer.sink;
         }

         recvBuffer.state = HGFS_CONN_RECV_REP_PAYLOAD;
         recvBuffer.len = recvBuffer.header.packetLen - sizeof recvBuffer.reply;
         if (recvBuffer.len)
            break;

         /* There is no actual payload, fall through */

      case HGFS_CONN_RECV_REP_PAYLOAD:
         LOG(10, (KERN_DEBUG "VMware hgfs: %s: received packet payload\n",
                  __func__));
         if (recvBuffer.req) {
            HgfsCompleteReq(recvBuffer.req);
            HgfsRequestPutRef(recvBuffer.req);
            recvBuffer.req = NULL;
         }
         HgfsSocketResetRecvBuffer();
         break;

      default:
         ASSERT(0);
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketReceiveHandler --
 *
 *     Function run in background thread and wait on the data in the
 *     connected channel.
 *
 * Results:
 *     Always returns zero.
 *
 * Side effects:
 *     Can be many.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSocketReceiveHandler(void *data)
{
   HgfsTransportChannel *channel = data;
   int ret;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: thread started\n", __func__));

   compat_set_freezable();

   for (;;) {

      /* Wait for data to become available */
      wait_event_interruptible(hgfsRecvThreadWait,
                  (HgfsSocketIsReceiverIdle() && kthread_should_stop()) ||
                  test_bit(HGFS_REQ_THREAD_RECV, &hgfsRecvThreadFlags));

      /* Kill yourself if told so. */
      if (kthread_should_stop()) {
         LOG(6, (KERN_DEBUG "VMware hgfs: %s: told to exit\n", __func__));
         break;
      }

      /* Check for suspend. */
      if (compat_try_to_freeze()) {
         LOG(6, (KERN_DEBUG "VMware hgfs: %s: continuing after resume.\n",
                 __func__));
         continue;
      }

      if (test_and_clear_bit(HGFS_REQ_THREAD_RECV, &hgfsRecvThreadFlags)) {

         /* There is some data witing for us, let's read it */
         ret = HgfsSocketChannelRecvAsync(channel);

         if (ret < 0 && ret != -EINTR && ret != -ERESTARTSYS && ret != -EAGAIN) {

            if (recvBuffer.req) {
               HgfsFailReq(recvBuffer.req, -EIO);
               HgfsRequestPutRef(recvBuffer.req);
               recvBuffer.req = NULL;
            }

            /* The connection is broken, leave it to the senders to restore it. */
            HgfsTransportMarkDead();
         }
      }
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: thread exited\n", __func__));
   recvThread = NULL;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCreateTcpSocket --
 *
 *     Connect to HGFS TCP server.
 *
 * Results:
 *     NULL on failure; otherwise address of the newly created and
 *     connected TCP socket.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static struct socket *
HgfsCreateTcpSocket(void)
{
   struct socket *socket;
   struct sockaddr_in addr;
   int error;

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = in_aton(HOST_IP);
   addr.sin_port = htons(HOST_PORT);

   error = sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP, &socket);
   if (error < 0) {
      LOG(8, ("%s: sock_create_kern failed: %d.\n", __func__, error));
      return NULL;
   }

   error = socket->ops->connect(socket, (struct sockaddr *)&addr,
                                    sizeof addr, 0);
   if (error < 0) {
      LOG(8, ("%s: connect failed: %d.\n", __func__, error));
      sock_release(socket);
      return NULL;
   }

   return socket;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCreateVsockSocket --
 *
 *     Connect to HGFS VSocket server.
 *
 * Results:
 *     NULL on failure; otherwise address of the newly created and
 *     connected VSock socket.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static struct socket *
HgfsCreateVsockSocket(void)
{
#ifdef INCLUDE_VSOCKETS
   struct socket *socket;
   struct sockaddr_vm addr;
   int family = VMCISock_GetAFValue();
   int error;

   memset(&addr, 0, sizeof addr);
   addr.svm_family = family;
   addr.svm_cid = VMCI_HOST_CONTEXT_ID;
   addr.svm_port = HOST_VSOCKET_PORT;

   error = sock_create_kern(family, SOCK_STREAM, IPPROTO_TCP, &socket);
   if (error < 0) {
      LOG(8, ("%s: sock_create_kern failed: %d.\n", __func__, error));
      return NULL;
   }

   error = socket->ops->connect(socket, (struct sockaddr *)&addr,
                                    sizeof addr, 0);
   if (error < 0) {
      LOG(8, ("%s: connect failed: %d.\n", __func__, error));
      sock_release(socket);
      return NULL;
   }

   return socket;

#else
   return NULL;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelOpen --
 *
 *     Connect to HGFS TCP or VSocket server in an idempotent way.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsSocketChannelOpen(HgfsTransportChannel *channel,
                      struct socket *(*create_socket)(void))
{
   struct socket *socket;

   ASSERT(channel->status == HGFS_CHANNEL_NOTCONNECTED);
   ASSERT(!recvThread);

   socket = create_socket();
   if (socket == NULL)
      return FALSE;

   /*
    * Install the new "data ready" handler that will wake up the
    * receiving thread.
    */
   oldSocketDataReady = xchg(&socket->sk->sk_data_ready,
                             HgfsSocketDataReady);

   /* Reset receive buffer when a new connection is connected. */
   HgfsSocketResetRecvBuffer();

   channel->priv = socket;

   LOG(8, ("%s: socket channel connected.\n", __func__));

   /* Create the recv thread. */
   recvThread = kthread_run(HgfsSocketReceiveHandler, channel, "vmhgfs-rep");
   if (IS_ERR(recvThread)) {
      LOG(4, (KERN_ERR "VMware hgfs: %s: "
              "failed to create recv thread, err %ld\n",
              __func__, PTR_ERR(recvThread)));
      recvThread = NULL;
      sock_release(channel->priv);
      channel->priv = NULL;
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelOpen --
 *
 *     Connect to HGFS TCP server in an idempotent way.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsTcpChannelOpen(HgfsTransportChannel *channel)
{
   return HgfsSocketChannelOpen(channel, HgfsCreateTcpSocket);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsVSocketChannelOpen --
 *
 *     Connect to HGFS VSocket server in an idempotent way.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsVSocketChannelOpen(HgfsTransportChannel *channel)
{
   VMCISock_KernelRegister();

   if (!HgfsSocketChannelOpen(channel, HgfsCreateVsockSocket)) {
      VMCISock_KernelDeregister();
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelClose --
 *
 *     Closes socket-based channel by closing socket and stopping the
 *     receiving thread.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsSocketChannelClose(HgfsTransportChannel *channel)
{
   /* Stop the recv thread before we change the channel status. */
   ASSERT(recvThread != NULL);
   kthread_stop(recvThread);

   sock_release(channel->priv);
   channel->priv = NULL;

   LOG(8, ("VMware hgfs: %s: socket channel closed.\n", __func__));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelClose --
 *
 *     Closes TCP channel.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsTcpChannelClose(HgfsTransportChannel *channel)
{
   HgfsSocketChannelClose(channel);

   LOG(8, ("VMware hgfs: %s: tcp channel closed.\n", __func__));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsVSocketChannelClose --
 *
 *     See above.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsVSocketChannelClose(HgfsTransportChannel *channel)
{
   HgfsSocketChannelClose(channel);
   VMCISock_KernelDeregister();

   LOG(8, ("VMware hgfs: %s: VSock channel closed.\n", __func__));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketSendMsg --
 *
 *     Send the message via the socket. Add the header before sending.
 *
 * Results:
 *     On success returns 0;
 *     On failure returns the negative errno.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSocketSendMsg(struct socket *socket,   // IN: socket
                  void *buffer,            // IN: Buffer to send
                  size_t bufferLen)        // IN: Buffer length
{
   struct iovec iov;
   struct msghdr msg;
   int ret = 0;
   int i = 0;
   mm_segment_t oldfs = get_fs();

   memset(&msg, 0, sizeof msg);
   iov.iov_base = buffer;
   iov.iov_len = bufferLen;
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;

   while (bufferLen > 0) {
      set_fs(KERNEL_DS);
      ret = sock_sendmsg(socket, &msg, bufferLen);
      set_fs(oldfs);
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: sock_sendmsg returns %d.\n",
              __func__, ret));

      if (likely(ret == bufferLen)) { /* Common case. */
         break;
      } else if (ret < 0) {
         if (ret == -ENOSPC || ret == -EAGAIN) {
            if (++i <= 12) {
               LOG(6, (KERN_DEBUG "VMware hgfs: %s: "
                       "Sleep for %d milliseconds before retry.\n",
                       __func__, (1 << i)));
               compat_msleep(1 << i);
               continue;
            }

            LOG(2, ("VMware hgfs: %s: send stuck for 8 seconds.\n", __func__));
            ret = -EIO;
         }
         break;
      } else if (ret >= bufferLen) {
         LOG(2, ("VMware hgfs: %s: sent more than expected bytes. Sent: %d, "
                 "expected: %d\n", __func__, ret, (int)bufferLen));
         break;
      } else {
         i = 0;
         bufferLen -= ret;
         iov.iov_base += ret;
         iov.iov_len -= ret;
      }
   }

   if (ret > 0) {
      ret = 0; /* Indicate success. */
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelSend --
 *
 *     Send the request via a socket channel.
 *
 * Results:
 *     0 on success, negative error on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSocketChannelSend(HgfsTransportChannel *channel, // IN:  Channel
                      HgfsReq *req)                  // IN: request to send
{
   int result;

   ASSERT(req);

   HgfsSocketHeaderInit((HgfsSocketHeader *)req->buffer, HGFS_SOCKET_VERSION1,
                        sizeof(HgfsSocketHeader), HGFS_SOCKET_STATUS_SUCCESS,
                        req->payloadSize, 0);

   req->state = HGFS_REQ_STATE_SUBMITTED;
   result = HgfsSocketSendMsg((struct socket *)channel->priv, req->buffer,
                              sizeof(HgfsSocketHeader) + req->payloadSize);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: sendmsg, err: %d.\n",
              __func__, result));
      req->state = HGFS_REQ_STATE_UNSENT;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelAllocate --
 *
 *     Allocates memory for HgfsReq, its payload plus additional memory
 *     needed for the socket transport itself.
 *
 * Results:
 *     NULL on failure otherwise address of allocated memory.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static HgfsReq *
HgfsSocketChannelAllocate(size_t payloadSize) // IN: size of the payload
{
   HgfsReq *req;

   req = kmalloc(sizeof(*req) + sizeof(HgfsSocketHeader) + payloadSize,
                 GFP_KERNEL);
   if (likely(req)) {
      req->payload = req->buffer + sizeof(HgfsSocketHeader);
      req->bufferSize = payloadSize;
   }

   return req;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSocketChannelFree --
 *
 *     Free previously allocated request.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      Object is freed
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsSocketChannelFree(HgfsReq *req)
{
   ASSERT(req);
   kfree(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetTcpChannel --
 *
 *     Initialize TCP channel.
 *
 * Results:
 *     Pointer to a channel on success, otherwise NULL.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

HgfsTransportChannel *
HgfsGetTcpChannel(void)
{
   if (!HOST_IP) {
      return NULL;
   }

   return &tcpChannel;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetVSocketChannel --
 *
 *     Initialize VSocket channel.
 *
 * Results:
 *     Pointer to a channel on success, otherwise NULL.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

HgfsTransportChannel *
HgfsGetVSocketChannel(void)
{
   if (!HOST_VSOCKET_PORT) {
      return NULL;
   }

   return &vsockChannel;
}


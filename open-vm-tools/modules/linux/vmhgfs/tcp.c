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
#include <linux/errno.h>

#include "compat_kernel.h"
#include "compat_kthread.h"
#include "compat_mutex.h"
#include "compat_version.h"
#include "compat_sched.h"
#include "compat_sock.h"
#include "compat_timer.h"
#include "vm_assert.h"

#include "hgfsProto.h"

#include "hgfsDevLinux.h"
#include "module.h"
#include "tcp.h"

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

int
VMCISock_GetAFValue (void)
{
   return -1;
}

void
VMCISock_KernelDeregister(void)
{
}

void
VMCISock_KernelRegister(void)
{
}

#endif


/* Socket recv timeout value, counted in HZ. */
#define HGFS_SOCKET_RECV_TIMEOUT 10

/* Recv states for the recvBuffer. */
typedef enum {
   HGFS_TCP_CONN_RECV_HEADER,
   HGFS_TCP_CONN_RECV_DATA,
} HgfsTcpRecvState;

/* HGFS receive buffer. */
typedef struct HgfsTcpRecvBuffer {
   HgfsSocketHeader header;            /* Buffer for receiving header. */
   char data[HGFS_PACKET_MAX];         /* Buffer for receiving packet. */
   HgfsTcpRecvState state;             /* Reply receive state. */
   int len;                            /* Number of bytes to receive. */
   char *buf;                          /* Pointer to the buffer. */
} HgfsTcpRecvBuffer;

static HgfsTcpRecvBuffer recvBuffer;   /* Accessed only by the recv thread. */
static struct task_struct *recvThread; /* The recv thread. */

HgfsTransportChannel tcpChannel;
HgfsTransportChannel vsocketChannel;
typedef Bool (*HgfsConnect)(HgfsTransportChannel *);


/* Private functions. */
static Bool HgfsTcpChannelOpen(HgfsTransportChannel *);
static Bool HgfsVSocketChannelOpen(HgfsTransportChannel *);
static void HgfsSocketChannelClose(HgfsTransportChannel *);
static int HgfsTcpChannelRecvAsync(HgfsTransportChannel *,
                                   char **replyPacket,
                                   size_t *packetSize);


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpReceiveHandler --
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
HgfsTcpReceiveHandler(void *data)
{
   char *receivedPacket = NULL;
   size_t receivedSize = 0;
   int ret = 0;
   HgfsTransportChannel *channel = data;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: thread started\n", __func__));

   compat_set_freezable();
   for (;;) {
      /* Kill yourself if told so. */
      if (compat_kthread_should_stop()) {
         LOG(6, (KERN_DEBUG "VMware hgfs: %s: told to exit\n", __func__));
         break;
      }

      /* Check for suspend. */
      if (compat_try_to_freeze()) {
         LOG(6, (KERN_DEBUG "VMware hgfs: %s: closing transport and exiting "
                 "the thread after resume.\n", __func__));
         channel->ops.close(channel);
         break;
      }

      /* Waiting on the data, may be blocked. */
      ret = HgfsTcpChannelRecvAsync(channel, &receivedPacket, &receivedSize);

      /* The connection is not lost for the following returns, just continue. */
      if (ret == -EINTR || ret == -ERESTARTSYS || ret == -EAGAIN) {
         continue;
      }

      if (ret < 0) {
         /* The connection is broken. Close it to free up the resources. */
         channel->ops.close(channel);
         if (!channel->ops.open(channel)) {
            /* We are unable to reconnect to the server, exit the thread. */
            LOG(6, (KERN_DEBUG "VMware hgfs: %s: recv error: %d. Lost the "
                    "connection to server.\n", __func__, ret));
            break;
         }
      } else if (ret > 0) {
         /* Process the received packet. */
         HgfsTransportProcessPacket(receivedPacket, receivedSize);
      }
   }

   HgfsTransportBeforeExitingRecvThread();
   LOG(6, (KERN_DEBUG "VMware hgfs: %s: thread exited\n", __func__));
   recvThread = NULL;
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpStopReceivingThread --
 *
 *     Helper function to stop channel handler thread.
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
HgfsTcpStopReceivingThread(void)
{
   if (recvThread && current != recvThread) { /* Don't stop myself. */
      /* Wake up socket by sending signal. */
      force_sig(SIGKILL, recvThread);
      compat_kthread_stop(recvThread);
      recvThread = NULL;
      LOG(8, ("VMware hgfs: %s: recv thread stopped.\n", __func__));
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpResetRecvBuffer --
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
HgfsTcpResetRecvBuffer(void)
{
   recvBuffer.state = HGFS_TCP_CONN_RECV_HEADER;
   recvBuffer.len = sizeof recvBuffer.header;
   recvBuffer.buf = (char *)&recvBuffer.header;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpConnect --
 *
 *     Connect to HGFS TCP server.
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
HgfsTcpConnect(HgfsTransportChannel *channel)
{
   int error;
   struct socket *sock;
   struct sockaddr_in addr;
   Bool ret = FALSE;

   if (channel->priv != NULL) {
      sock_release(channel->priv);
      channel->priv = NULL;
   }

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = in_aton(HOST_IP);
   addr.sin_port = htons(HOST_PORT);

   error = compat_sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                   &sock);
   if (error >= 0) {
      error = sock->ops->connect(sock, (struct sockaddr *)&addr,
                                 sizeof addr, 0);
      if (error >= 0) {
         channel->priv = sock;

         /* Reset receive buffer when a new connection is connected. */
         recvBuffer.state = HGFS_TCP_CONN_RECV_HEADER;

         channel->status = HGFS_CHANNEL_CONNECTED;
         LOG(8, ("%s: tcp channel connected.\n", __func__));
         ret = TRUE;
      } else {
         LOG(8, ("%s: connect failed: %d.\n", __func__, error));
         sock_release(sock);
      }
   } else {
      LOG(8, ("%s: sock_create_kern failed: %d.\n", __func__, error));
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsVSocketConnect --
 *
 *     Connect to HGFS VSOCKET server.
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
HgfsVSocketConnect(HgfsTransportChannel *channel)
{
#ifdef INCLUDE_VSOCKETS
   int error;
   struct socket *sock;
   struct sockaddr_vm addr;
   int family;
   Bool ret = FALSE;

   memset(&addr, 0, sizeof addr);

   family = VMCISock_GetAFValue();

   addr.svm_family = family;
   addr.svm_cid = VMCI_HOST_CONTEXT_ID;
   addr.svm_port = HOST_VSOCKET_PORT;

   error = compat_sock_create_kern(family, SOCK_STREAM, 0,
                                   &sock);
   if (error >= 0) {
      error = sock->ops->connect(sock, (struct sockaddr *)&addr,
                                 sizeof addr, 0);
      if (error >= 0) {
         channel->priv = sock;

         /* Reset receive buffer when a new connection is connected. */
         HgfsTcpResetRecvBuffer();
         sock->sk->compat_sk_rcvtimeo = HGFS_SOCKET_RECV_TIMEOUT * HZ;

         channel->status = HGFS_CHANNEL_CONNECTED;
         LOG(8, ("%s: vsocket channel connected.\n", __func__));
         ret = TRUE;
      } else {
         LOG(4, ("%s: connect failed: %d.\n", __func__, error));
         sock_release(sock);
      }
   } else {
      LOG(8, ("%s: sock_create_kern failed: %d.\n", __func__, error));
   }
   return ret;
#else
   return FALSE;
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
                      HgfsConnect connect)
{
   Bool ret = FALSE;

   compat_mutex_lock(&channel->connLock);
   switch (channel->status) {
   case HGFS_CHANNEL_UNINITIALIZED:
      ret = FALSE;
      break;
   case HGFS_CHANNEL_CONNECTED:
      ret = TRUE;
      break;
   case HGFS_CHANNEL_NOTCONNECTED:
      ret = connect(channel);

      /*
       * Ensure this function won't race with HgfsTcpChannelRecvAsync, which
       * is called only by recvThread. If this thread is not the recv thread,
       * make sure the recv thread is finished before we proceed.
       */
      ASSERT(!recvThread || current == recvThread);

      if (ret && !recvThread) {
         /* Create the recv thread. */
         recvThread = compat_kthread_run(HgfsTcpReceiveHandler,
                                         channel, "vmhgfs-rep");
         if (IS_ERR(recvThread)) {
            LOG(4, (KERN_ERR "VMware hgfs: %s: failed to create recv thread\n",
                    __func__));
            recvThread = NULL;
            channel->ops.close(channel);
            ret = FALSE;
            break;
         }
      }
      break;
   default:
      ASSERT(0); /* Not reached. */
   }

   compat_mutex_unlock(&channel->connLock);
   return ret;
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
   return HgfsSocketChannelOpen(channel, HgfsTcpConnect);
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
   return HgfsSocketChannelOpen(channel, HgfsVSocketConnect);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelCloseWork --
 *
 *     Close the tcp channel in an idempotent way.
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
HgfsTcpChannelCloseWork(HgfsTransportChannel *channel)
{
   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      return;
   }

   if (channel->priv != NULL) {
      sock_release(channel->priv);
      channel->priv = NULL;
   }
   channel->status = HGFS_CHANNEL_NOTCONNECTED;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelClose --
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
HgfsSocketChannelClose(HgfsTransportChannel *channel)
{
   /* Stop the recv thread before we change the channel status. */
   HgfsTcpStopReceivingThread();
   compat_mutex_lock(&channel->connLock);
   HgfsTcpChannelCloseWork(channel);
   compat_mutex_unlock(&channel->connLock);
   LOG(8, ("VMware hgfs: %s: tcp channel closed.\n", __func__));
}

/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpRecvMsg --
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
HgfsTcpRecvMsg(struct socket *socket,   // IN: TCP socket
               char *buffer,            // IN: Buffer to recv the message
               size_t bufferLen)        // IN: Buffer length
{
   struct iovec iov;
   struct msghdr msg;
   int ret;
   int flags = 0;
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
 * HgfsTcpChannelRecvAsync --
 *
 *     Receive the packet. Note, the recv may timeout and return just
 *     a part of the packet according to our setting of the socket.
 *
 * Results:
 *     On complete packet received, returns its size.
 *     On part of the packet received, returns 0.
 *     On failure returns the negative errno.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsTcpChannelRecvAsync(HgfsTransportChannel *channel, // IN:  Channel
                        char **replyPacket,            // OUT: Reply buffer
                        size_t *packetSize)            // OUT: Packet size
{
   int ret = -EIO;

   ASSERT(replyPacket);
   ASSERT(packetSize);

   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost.\n", __func__));
      return -ENOTCONN;
   }

   LOG(10, (KERN_DEBUG "VMware hgfs: %s: receiving %s\n", __func__,
            recvBuffer.state == 0? "header" : "data"));
   ret = HgfsTcpRecvMsg(channel->priv, recvBuffer.buf, recvBuffer.len);
   LOG(10, (KERN_DEBUG "VMware hgfs: %s: sock_recvmsg returns: %d\n",
            __func__, ret));
   if (ret > 0) {
      ASSERT(ret <= recvBuffer.len);
      recvBuffer.len -= ret;
      if ( recvBuffer.len == 0) {
         /* Complete header or reply packet received. */
         switch(recvBuffer.state) {
         case HGFS_TCP_CONN_RECV_HEADER:
            ASSERT(recvBuffer.header.version == HGFS_SOCKET_VERSION1);
            ASSERT(recvBuffer.header.size == sizeof recvBuffer.header);
            ASSERT(recvBuffer.header.status == HGFS_SOCKET_STATUS_SUCCESS);
            *packetSize = 0;
            recvBuffer.state = HGFS_TCP_CONN_RECV_DATA;
            recvBuffer.len = recvBuffer.header.packetLen;
            recvBuffer.buf = &recvBuffer.data[0];
            ret = 0; /* To indicate the reply packet is not fully received. */
            break;
         case HGFS_TCP_CONN_RECV_DATA:
            *packetSize = recvBuffer.header.packetLen;
            *replyPacket = &recvBuffer.data[0];
            HgfsTcpResetRecvBuffer();
            break;
         default:
            ASSERT(0);
         }
      } else if (recvBuffer.len > 0) { /* No complete packet received. */
         recvBuffer.buf += ret;
         ret = 0;
      }
   }else if (ret == 0) { /* The connection is actually broken. */
      ret = -ENOTCONN;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpSendMsg --
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
HgfsTcpSendMsg(struct socket *socket,   // IN: socket
               char *buffer,            // IN: Buffer to send
               size_t bufferLen)        // IN: Buffer length
{
   HgfsSocketHeader sockHeader;
   struct iovec iov[2];
   struct msghdr msg;
   int totalLen;
   int ret = 0;
   int i = 0;
   mm_segment_t oldfs = get_fs();

   HgfsSocketHeaderInit(&sockHeader, HGFS_SOCKET_VERSION1, sizeof sockHeader,
                        HGFS_SOCKET_STATUS_SUCCESS, bufferLen, 0);

   memset(&msg, 0, sizeof msg);
   iov[0].iov_base = &sockHeader;
   iov[0].iov_len = sizeof sockHeader;
   iov[1].iov_base = buffer;
   iov[1].iov_len = bufferLen;
   msg.msg_iov = &iov[0];
   msg.msg_iovlen = 2;
   totalLen = sizeof sockHeader + bufferLen;

   while (totalLen > 0) {
      set_fs(KERNEL_DS);
      ret = sock_sendmsg(socket, &msg, totalLen);
      set_fs(oldfs);
      LOG(6, ("VMware hgfs: %s: sock_sendmsg returns %d.\n", __func__, ret));

      if (ret == totalLen) { /* Common case. */
         break;
      }

      if (ret == -ENOSPC || ret == -EAGAIN) {
         i++;
         if (i > 12) {
            LOG(2, ("VMware hgfs: %s: send stuck for 8 seconds.\n", __func__));
            ret = -EIO;
            break;
         }
         LOG(6, ("VMware hgfs: %s: Sleep for %d milliseconds before retry.\n",
                 __func__, (1 << i)));
         compat_msleep(1 << i);
         continue;
      } else if (ret < 0) {
         break;
      } else if (ret > totalLen) {
         LOG(2, ("VMware hgfs: %s: sent more than expected bytes. Sent: %d, "
                 "expected: %d\n", __func__, ret, totalLen));
         break;
      }

      i = 0;
      totalLen -= ret;
      if (ret < iov[0].iov_len) {
         /* Only part of the header has been sent out. */
         iov[0].iov_base += ret;
         iov[0].iov_len -= ret;
      } else {
         /* The header and part of the body have been sent out. */
         int temp = ret - iov[0].iov_len;
         iov[1].iov_base += temp;
         iov[1].iov_len -= temp;
         msg.msg_iov = &iov[1];
         msg.msg_iovlen = 1;
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
 * HgfsTcpChannelSendAsync --
 *
 *     Send the request via TCP channel.
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
HgfsTcpChannelSendAsync(HgfsTransportChannel *channel, // IN:  Channel
                        HgfsReq *req)                  // IN: request to send
{
   struct socket *socket;
   int result;

   ASSERT(req);
   compat_mutex_lock(&channel->connLock);
   if (channel->status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost\n", __func__));
      compat_mutex_unlock(&channel->connLock);
      return -ENOTCONN;
   }

   req->state = HGFS_REQ_STATE_SUBMITTED;
   socket = channel->priv;

   result = HgfsTcpSendMsg(socket, HGFS_REQ_PAYLOAD(req), req->payloadSize);

   compat_mutex_unlock(&channel->connLock);

   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: sendmsg, err: %d.\n",
              __func__, result));
      channel->ops.close(channel);
      req->state = HGFS_REQ_STATE_UNSENT;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSocketChannelExit --
 *
 *     Tear down the channel.
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
HgfsSocketChannelExit(HgfsTransportChannel* channel)  // IN OUT
{
   HgfsTcpStopReceivingThread();
   compat_mutex_lock(&channel->connLock);
   if (channel->status != HGFS_CHANNEL_UNINITIALIZED) {
      HgfsTcpChannelCloseWork(channel);
      channel->status = HGFS_CHANNEL_UNINITIALIZED;
      LOG(8, ("VMware hgfs: %s: tcp channel exited.\n", __func__));
   }
   compat_mutex_unlock(&channel->connLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsVSocketChannelExit --
 *
 *     Tear down the channel.
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
HgfsVSocketChannelExit(HgfsTransportChannel* channel)  // IN OUT
{
   HgfsSocketChannelExit(channel);
   VMCISock_KernelDeregister();
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

HgfsTransportChannel*
HgfsGetTcpChannel(void)
{
   tcpChannel.name = "tcp";
   tcpChannel.ops.open = HgfsTcpChannelOpen;
   tcpChannel.ops.close = HgfsSocketChannelClose;
   tcpChannel.ops.send = HgfsTcpChannelSendAsync;
   tcpChannel.ops.recv = HgfsTcpChannelRecvAsync;
   tcpChannel.ops.exit = HgfsSocketChannelExit;
   tcpChannel.priv = NULL;
   compat_mutex_init(&tcpChannel.connLock);

   memset(&recvBuffer, 0, sizeof recvBuffer);
   HgfsTcpResetRecvBuffer();
   recvThread = NULL;
   tcpChannel.status = HGFS_CHANNEL_NOTCONNECTED;

   if (!HOST_IP) {/* HOST_IP is defined in module.c. */
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

HgfsTransportChannel*
HgfsGetVSocketChannel(void)
{
   vsocketChannel.name = "vsocket";
   vsocketChannel.ops.open = HgfsVSocketChannelOpen;
   vsocketChannel.ops.close = HgfsSocketChannelClose;
   vsocketChannel.ops.send = HgfsTcpChannelSendAsync;
   vsocketChannel.ops.recv = HgfsTcpChannelRecvAsync;
   vsocketChannel.ops.exit = HgfsVSocketChannelExit;
   vsocketChannel.priv = NULL;
   compat_mutex_init(&vsocketChannel.connLock);

   /* Initialize hgfsConn. */
   memset(&recvBuffer, 0, sizeof recvBuffer);
   HgfsTcpResetRecvBuffer();
   recvThread = NULL;
   vsocketChannel.status = HGFS_CHANNEL_NOTCONNECTED;

   if (!HOST_VSOCKET_PORT) {/* HOST_VSOCKET_PORT is defined in module.c. */
      return NULL;
   }

   VMCISock_KernelRegister();
   return &vsocketChannel;
}

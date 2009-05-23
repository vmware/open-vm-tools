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


/* Private functions. */
static Bool HgfsTcpChannelOpen(void);
static void HgfsTcpChannelClose(void);
static int HgfsTcpChannelRecvAsync(char **replyPacket,
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
HgfsTcpReceiveHandler(void *dummyarg)
{
   char *receivedPacket = NULL;
   size_t receivedSize = 0;
   int ret = 0;

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
         HgfsTcpChannelClose();
         break;
      }

      /* Waiting on the data, may be blocked. */
      ret = HgfsTcpChannelRecvAsync(&receivedPacket, &receivedSize);

      /* The connection is not lost for the following returns, just continue. */
      if (ret == -EINTR || ret == -ERESTARTSYS || ret == -EAGAIN) {
         continue;
      }

      if (ret < 0) {
         /* The connection is broken. Close it to free up the resources. */
         HgfsTcpChannelClose();
         if (!HgfsTcpChannelOpen()) {
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
HgfsTcpChannelOpen(void)
{
   Bool ret = FALSE;

   compat_mutex_lock(&tcpChannel.connLock);
   switch (tcpChannel.status) {
   case HGFS_CHANNEL_UNINITIALIZED:
      ret = FALSE;
      LOG(4, ("%s: open tcp channel NOT UNINITIALIZED.\n", __func__));
      break;
   case HGFS_CHANNEL_CONNECTED:
      ret = TRUE;
      LOG(8, ("%s: open tcp channel CONNECTED.\n", __func__));
      break;
   case HGFS_CHANNEL_NOTCONNECTED: {
      int error;
      struct socket *sock;
      struct sockaddr_in addr;

      /*
       * Ensure this function won't race with HgfsTcpChannelRecvAsync, which
       * is called only by recvThread. If this thread is not the recv thread,
       * make sure the recv thread is finished before we proceed.
       */
      ASSERT(!recvThread || current == recvThread);

      if (!HOST_IP) {/* HOST_IP is defined in module.c. */
         ret = FALSE;
         break;
      }

      ASSERT(!tcpChannel.priv);

      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = in_aton(HOST_IP);
      addr.sin_port = htons(HOST_PORT);

      error = compat_sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                      &sock);
      if (error < 0) {
         LOG(4, ("%s: sock_create_kern failed: %d.\n", __func__, error));
         ret = FALSE;
         break;
      }

      error = sock->ops->connect(sock, (struct sockaddr *)&addr,
                                 sizeof addr, 0);
      if (error < 0) {
         LOG(4, ("%s: connect failed: %d.\n", __func__, error));
         sock_release(sock);
         ret = FALSE;
         break;
      }
      sock->sk->compat_sk_rcvtimeo = HGFS_SOCKET_RECV_TIMEOUT * HZ;

      tcpChannel.priv = sock;
      /* Reset receive buffer when a new connection is connected. */
      HgfsTcpResetRecvBuffer();
      tcpChannel.status = HGFS_CHANNEL_CONNECTED;

      if (!recvThread) {
         /* Creat the recv thread. */
         recvThread = compat_kthread_run(HgfsTcpReceiveHandler,
                                         NULL, "vmhgfs-rep");
         if (IS_ERR(recvThread)) {
            LOG(4, (KERN_ERR "VMware hgfs: %s: failed to create recv thread\n",
                    __func__));
            recvThread = NULL;
            sock_release(sock);
            ret = FALSE;
            break;
         }
      }

      LOG(8, ("%s: tcp channel conneted and the recv thread created.\n",
              __func__));
      ret = TRUE;
      break;
   }
   default:
      ASSERT(0); /* Not reached. */
   }

   compat_mutex_unlock(&tcpChannel.connLock);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelCloseWork --
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
HgfsTcpChannelCloseWork(void)
{
   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      return;
   }

   if (tcpChannel.priv != NULL) {
      sock_release(tcpChannel.priv);
      tcpChannel.priv = NULL;
   }
   tcpChannel.status = HGFS_CHANNEL_NOTCONNECTED;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelClose --
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
HgfsTcpChannelClose(void)
{
   /* Stop the recv thread before we change the channel status. */
   HgfsTcpStopReceivingThread();
   compat_mutex_lock(&tcpChannel.connLock);
   HgfsTcpChannelCloseWork();
   compat_mutex_unlock(&tcpChannel.connLock);
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
HgfsTcpChannelRecvAsync(char **replyPacket,   // IN/OUT: Reply buffer
                        size_t *packetSize)   // IN/OUT: Reply packet size
{
   int ret = -EIO;

   ASSERT(replyPacket);
   ASSERT(packetSize);

   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost.\n", __func__));
      return -ENOTCONN;
   }

   LOG(10, (KERN_DEBUG "VMware hgfs: %s: receiving %s\n", __func__,
            recvBuffer.state == 0? "header" : "data"));
   ret = HgfsTcpRecvMsg(tcpChannel.priv, recvBuffer.buf, recvBuffer.len);
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
HgfsTcpChannelSendAsync(HgfsReq *req)    // IN: request to send
{
   struct socket *socket;
   int result;

   ASSERT(req);
   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost\n", __func__));
      compat_mutex_unlock(&tcpChannel.connLock);
      return -ENOTCONN;
   }

   req->state = HGFS_REQ_STATE_SUBMITTED;
   socket = tcpChannel.priv;

   result = HgfsTcpSendMsg(socket, HGFS_REQ_PAYLOAD(req), req->payloadSize);

   compat_mutex_unlock(&tcpChannel.connLock);

   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: sendmsg, err: %d.\n",
              __func__, result));
      HgfsTcpChannelClose();
      req->state = HGFS_REQ_STATE_UNSENT;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelExit --
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
HgfsTcpChannelExit(void)
{
   HgfsTcpStopReceivingThread();
   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.status != HGFS_CHANNEL_UNINITIALIZED) {
      HgfsTcpChannelCloseWork();
      tcpChannel.status = HGFS_CHANNEL_UNINITIALIZED;
      LOG(8, ("VMware hgfs: %s: tcp channel exited.\n", __func__));
   }
   compat_mutex_unlock(&tcpChannel.connLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelInit --
 *
 *     Initialize TCP channel.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

void
HgfsTcpChannelInit(void)
{
   tcpChannel.name = "tcp";
   tcpChannel.ops.open = HgfsTcpChannelOpen;
   tcpChannel.ops.close = HgfsTcpChannelClose;
   tcpChannel.ops.send = HgfsTcpChannelSendAsync;
   tcpChannel.ops.recv = HgfsTcpChannelRecvAsync;
   tcpChannel.ops.exit = HgfsTcpChannelExit;
   tcpChannel.priv = NULL;
   compat_mutex_init(&tcpChannel.connLock);

   memset(&recvBuffer, 0, sizeof recvBuffer);
   HgfsTcpResetRecvBuffer();
   recvThread = NULL;
   tcpChannel.status = HGFS_CHANNEL_NOTCONNECTED;
}

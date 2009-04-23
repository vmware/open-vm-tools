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
#include "compat_mutex.h"
#include "compat_version.h"
#include "compat_sched.h"
#include "compat_sock.h"
#include "vm_assert.h"

#include "hgfsProto.h"

#include "hgfsDevLinux.h"
#include "module.h"
#include "tcp.h"

#ifndef HGFS_TCP_SERVER
/* Should be removed after server-side changes are checked in. */
typedef struct HgfsTcpHeader {
   uint32 version;        /* Packet magic. */
   uint32 packetLen;      /* The length of the packet to follow. */
} HgfsTcpHeader;

#define HGFS_TCP_VERSION1 1
#endif

/* HGFS receive buffer. */
typedef struct HgfsTcpRecvBuffer {
   HgfsTcpHeader header;               /* Buffer for receiving header. */
   char data[HGFS_PACKET_MAX];         /* Buffer for receiving packet. */
   uint32 state;                       /* Reply receive state. */
} HgfsTcpRecvBuffer;

/* Recv states for the recvBuffer. */
typedef enum {
   HGFS_TCP_CONN_RECV_HEADER,
   HGFS_TCP_CONN_RECV_DATA,
} HgfsTcpRecvState;

/* No lock to protect recvBuffer. It is accessed only by the recv thread. */
static HgfsTcpRecvBuffer recvBuffer;

HgfsTransportChannel tcpChannel;

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
   int error;
   struct socket *sock;
   struct sockaddr_in addr;

   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.status == HGFS_CHANNEL_CONNECTED) {
      compat_mutex_unlock(&tcpChannel.connLock);
      return TRUE;
   } else if (tcpChannel.status == HGFS_CHANNEL_UNINITIALIZED) {
      compat_mutex_unlock(&tcpChannel.connLock);
      return FALSE;
   }

   if (tcpChannel.priv != NULL) {
      sock_release(tcpChannel.priv);
   }

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = in_aton(HOST_IP);
   addr.sin_port = htons(HOST_PORT);

   error = compat_sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
   if (error < 0) {
      LOG(8, ("%s: sock_create_kern failed: %d.\n", __FUNCTION__, error));
      compat_mutex_unlock(&tcpChannel.connLock);
      return FALSE;
   }

   error = sock->ops->connect(sock, (struct sockaddr *)&addr,
                              sizeof addr, 0);
   if (error < 0) {
      LOG(8, ("%s: connect failed: %d.\n", __FUNCTION__, error));
      if (sock) {
         sock_release(sock);
      }
      compat_mutex_unlock(&tcpChannel.connLock);
      return FALSE;
   }

   tcpChannel.priv = sock;

   tcpChannel.status = HGFS_CHANNEL_CONNECTED;
   LOG(8, ("%s: tcp channel conneted.\n", __FUNCTION__));
   compat_mutex_unlock(&tcpChannel.connLock);
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelClose --
 *
 *     Close the tcp channel in an idempotent way.
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
   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      compat_mutex_unlock(&tcpChannel.connLock);
      return;
   }

   if (tcpChannel.priv != NULL) {
      sock_release(tcpChannel.priv);
      tcpChannel.priv = NULL;
   }
   tcpChannel.status = HGFS_CHANNEL_DISCONNECTED;
   compat_mutex_unlock(&tcpChannel.connLock);
   LOG(8, ("%s: tcp channel closed.\n", __FUNCTION__));
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
   mm_segment_t fs;
   int ret;
   int flags = 0;

   memset(&msg, 0, sizeof msg);
   msg.msg_flags = flags;
   msg.msg_iov = &iov;
   msg.msg_iovlen = 1;
   iov.iov_base = buffer;
   iov.iov_len = bufferLen;

   /* get_fs/set_fs give us more privilidges so that we are able to call
      sock_recvmsg from kernel. */
   fs = get_fs();
   set_fs(get_ds());
   ret = sock_recvmsg(socket, &msg, bufferLen, flags);
   set_fs(fs);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelRecvAsync --
 *
 *     Receive the packet.
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
HgfsTcpChannelRecvAsync(char **replyPacket,   // IN/OUT: Reply buffer
                        size_t *packetSize)   // IN/OUT: Reply packet size
{
   int ret = -EIO;

   ASSERT(replyPacket);
   ASSERT(packetSize);

   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost.\n", __FUNCTION__));
      return -ENOTCONN;
   }

   switch(recvBuffer.state) {
   case HGFS_TCP_CONN_RECV_HEADER:
      ret = HgfsTcpRecvMsg(tcpChannel.priv, (char *)&recvBuffer.header,
                           sizeof(HgfsTcpHeader));
      if (ret > 0) {
         ASSERT(ret == sizeof(HgfsTcpHeader));
         recvBuffer.state = HGFS_TCP_CONN_RECV_DATA;
         ret = 0;
         LOG(8, (KERN_DEBUG "VMware hgfs: %s: header received. packet len: %d"
                 "\n", __FUNCTION__, recvBuffer.header.packetLen));
      }
      break;
   case HGFS_TCP_CONN_RECV_DATA:
      ret = HgfsTcpRecvMsg(tcpChannel.priv, &recvBuffer.data[0],
                           recvBuffer.header.packetLen);
      if (ret > 0) {
         ASSERT(ret == recvBuffer.header.packetLen);
         *packetSize = ret;
         *replyPacket = &recvBuffer.data[0];
         LOG(8, (KERN_DEBUG "VMware hgfs: %s: packet received. len: %d\n",
                 __FUNCTION__, ret));
         recvBuffer.state = HGFS_TCP_CONN_RECV_HEADER;
      }
      break;
   default:
      ASSERT(0);
   }

   if (ret < 0) {
      LOG(8, (KERN_DEBUG "VMware hgfs: %s: sock_recvmsg returns: %d\n",
              __FUNCTION__, ret));
      /* The connection is not lost for these three cases. */
      if (ret != -EINTR && ret != -ERESTARTSYS && ret != -EAGAIN) {
         recvBuffer.state = HGFS_TCP_CONN_RECV_HEADER;
         tcpChannel.status = HGFS_CHANNEL_DISCONNECTED;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelSendAsync --
 *
 *     Send the request via TCP channel. Add the header before sending.
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
   HgfsTcpHeader sockHeader;
   struct socket *socket;
   mm_segment_t fs;
   struct msghdr msg;
   struct iovec iov[2];
   int result;

   ASSERT(req);
   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.status != HGFS_CHANNEL_CONNECTED) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Connection lost\n", __FUNCTION__));
      compat_mutex_unlock(&tcpChannel.connLock);
      return -ENOTCONN;
   }

   socket = tcpChannel.priv;
   memset(&msg, 0, sizeof msg);
   sockHeader.version = HGFS_TCP_VERSION1;
   sockHeader.packetLen = req->payloadSize;

   msg.msg_iov = iov;
   msg.msg_iovlen = 2;

   iov[0].iov_base = &sockHeader;
   iov[0].iov_len = sizeof sockHeader;
   iov[1].iov_base = HGFS_REQ_PAYLOAD(req);
   iov[1].iov_len = req->payloadSize;

   /* get_fs/set_fs give us more privilidges so that we are able to call
      sock_sendmsg from kernel. */
   fs = get_fs();
   set_fs(KERNEL_DS);
   result = sock_sendmsg(socket, &msg, req->payloadSize);
   set_fs(fs);

   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: sendmsg, err: %d.\n",
              __FUNCTION__, result));
      tcpChannel.status = HGFS_CHANNEL_DISCONNECTED;
   } else  {
      LOG(8, ("VMware hgfs: %s: sendmsg, bytes sent: %d.\n",
              __FUNCTION__, result));
      ASSERT(result == req->payloadSize + sizeof sockHeader);
      req->state = HGFS_REQ_STATE_SUBMITTED;
      result = 0;
   }

   compat_mutex_unlock(&tcpChannel.connLock);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelExit --
 *
 *     Tear down the channel and close TCP connection.
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
   compat_mutex_lock(&tcpChannel.connLock);
   if (tcpChannel.priv != NULL) {
      sock_release(tcpChannel.priv);
      tcpChannel.priv = NULL;
   }
   tcpChannel.status = HGFS_CHANNEL_UNINITIALIZED;
   compat_mutex_unlock(&tcpChannel.connLock);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTcpChannelInit --
 *
 *     Initialize TCP channel and open the connection.
 *
 * Results:
 *     TRUE on success, otherwise FALSE.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

Bool
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

   /* Initialize hgfsConn. */
   memset(&recvBuffer, 0, sizeof recvBuffer);
   recvBuffer.state = HGFS_TCP_CONN_RECV_HEADER;
   tcpChannel.status = HGFS_CHANNEL_DISCONNECTED;

   if (!HOST_IP) {/* HOST_IP is defined in module.c. */
      return FALSE;
   }

   return TRUE;
}

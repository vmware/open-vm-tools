/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * af_vsock.h --
 *
 *      Definitions for Linux VSockets module.
 */

#ifndef __AF_VSOCK_H__
#define __AF_VSOCK_H__

#include "vsockCommon.h"
#include "vsockPacket.h"
#include "compat_workqueue.h"

#include "vmciKernelAPI.h"

#include "notify.h"

#ifdef VMX86_DEVEL
extern int LOGLEVEL_THRESHOLD;

#define LOG(level, args) ((void) (LOGLEVEL_THRESHOLD >= (level) ? (Log args) : 0))
#else
#define LOG(level, args)
#endif

# define vsock_sk(__sk)    ((VSockVmciSock *)__sk)
# define sk_vsock(__vsk)   (&(__vsk)->sk)

typedef struct VSockVmciSock {
   /* sk must be the first member. */
   struct sock  sk;
   struct sockaddr_vm localAddr;
   struct sockaddr_vm remoteAddr;
   /* Links for the global tables of bound and connected sockets. */
   struct list_head boundTable;
   struct list_head connectedTable;
   /*
    * Accessed without the socket lock held. This means it can never be
    * modified outsided of socket create or destruct.
    */
   Bool trusted;
   Bool cachedPeerAllowDgram; /* Dgram communication allowed to cached peer? */
   VMCIId cachedPeer; /* Context ID of last dgram destination check. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
   kuid_t owner;
#else
   uid_t owner;
#endif
   VMCIHandle dgHandle;           /* For SOCK_DGRAM only. */
   /* Rest are SOCK_STREAM only. */
   VMCIHandle qpHandle;
   VMCIQPair *qpair;
   uint64 produceSize;
   uint64 consumeSize;
   uint64 queuePairSize;
   uint64 queuePairMinSize;
   uint64 queuePairMaxSize;
   long connectTimeout;
   VSockVmciNotify notify;
   VSockVmciNotifyOps *notifyOps;
   VMCIId attachSubId;
   VMCIId detachSubId;
   /* Listening socket that this came from. */
   struct sock *listener;
   /*
    * Used for pending list and accept queue during connection handshake.  The
    * listening socket is the head for both lists.  Sockets created for
    * connection requests are placed in the pending list until they are
    * connected, at which point they are put in the accept queue list so they
    * can be accepted in accept().  If accept() cannot accept the connection,
    * it is marked as rejected so the cleanup function knows to clean up the
    * socket.
    */
   struct list_head pendingLinks;
   struct list_head acceptQueue;
   Bool rejected;
   compat_delayed_work dwork;
   uint32 peerShutdown;
   Bool sentRequest;
   Bool ignoreConnectingRst;
} VSockVmciSock;

int VSockVmciSendControlPktBH(struct sockaddr_vm *src,
                              struct sockaddr_vm *dst,
                              VSockPacketType type,
                              uint64 size,
                              uint64 mode,
                              VSockWaitingInfo *wait,
                              VMCIHandle handle);
int VSockVmciReplyControlPktFast(VSockPacket *pkt, VSockPacketType type,
                                 uint64 size, uint64 mode,
                                 VSockWaitingInfo *wait, VMCIHandle handle);
int VSockVmciSendControlPkt(struct sock *sk, VSockPacketType type,
                            uint64 size, uint64 mode,
                            VSockWaitingInfo *wait,
                            VSockProtoVersion version,
                            VMCIHandle handle);

int64 VSockVmciStreamHasData(VSockVmciSock *vsk);
int64 VSockVmciStreamHasSpace(VSockVmciSock *vsk);

#define VSOCK_SEND_RESET_BH(_dst, _src, _pkt)                           \
   ((_pkt)->type == VSOCK_PACKET_TYPE_RST) ?                            \
      0 :                                                               \
      VSockVmciSendControlPktBH(_dst, _src, VSOCK_PACKET_TYPE_RST, 0,   \
                                0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_INVALID_BH(_dst, _src)                               \
   VSockVmciSendControlPktBH(_dst, _src, VSOCK_PACKET_TYPE_INVALID, 0,  \
                             0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_WROTE_BH(_dst, _src)                                 \
   VSockVmciSendControlPktBH(_dst, _src, VSOCK_PACKET_TYPE_WROTE, 0,    \
                             0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_READ_BH(_dst, _src)                                  \
   VSockVmciSendControlPktBH(_dst, _src, VSOCK_PACKET_TYPE_READ, 0,     \
                             0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_RESET(_sk, _pkt)                                     \
   ((_pkt)->type == VSOCK_PACKET_TYPE_RST) ?                            \
      0 :                                                               \
      VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_RST,               \
                              0, 0, NULL, VSOCK_PROTO_INVALID,          \
                              VMCI_INVALID_HANDLE)
#define VSOCK_SEND_NEGOTIATE(_sk, _size)                                \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_NEGOTIATE,            \
                           _size, 0, NULL, VSOCK_PROTO_INVALID,         \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_NEGOTIATE2(_sk, _size, signalProto)                  \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_NEGOTIATE2,           \
                           _size, 0, NULL, signalProto,                 \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_QP_OFFER(_sk, _handle)                               \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_OFFER,                \
                           0, 0, NULL, VSOCK_PROTO_INVALID, _handle)
#define VSOCK_SEND_CONN_REQUEST(_sk, _size)                             \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_REQUEST,              \
                           _size, 0, NULL, VSOCK_PROTO_INVALID,         \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_CONN_REQUEST2(_sk, _size, signalProto)               \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_REQUEST2,             \
                           _size, 0, NULL, signalProto,                 \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_ATTACH(_sk, _handle)                                 \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_ATTACH,               \
                           0, 0, NULL, VSOCK_PROTO_INVALID, _handle)
#define VSOCK_SEND_WROTE(_sk)                                           \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WROTE,                \
                           0, 0, NULL, VSOCK_PROTO_INVALID,             \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_READ(_sk)                                            \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_READ,                 \
                           0, 0, NULL, VSOCK_PROTO_INVALID,             \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_SHUTDOWN(_sk, _mode)                                 \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_SHUTDOWN,             \
                           0, _mode, NULL, VSOCK_PROTO_INVALID,         \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_WAITING_WRITE(_sk, _waitInfo)                        \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WAITING_WRITE,        \
                           0, 0, _waitInfo, VSOCK_PROTO_INVALID,        \
                           VMCI_INVALID_HANDLE)
#define VSOCK_SEND_WAITING_READ(_sk, _waitInfo)                         \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WAITING_READ,         \
                           0, 0, _waitInfo, VSOCK_PROTO_INVALID,        \
                           VMCI_INVALID_HANDLE)
#define VSOCK_REPLY_RESET(_pkt)                                         \
   VSockVmciReplyControlPktFast(_pkt, VSOCK_PACKET_TYPE_RST,            \
                                0, 0, NULL, VMCI_INVALID_HANDLE)

#endif /* __AF_VSOCK_H__ */

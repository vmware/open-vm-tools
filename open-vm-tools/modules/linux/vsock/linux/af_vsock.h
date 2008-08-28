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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
# define vsock_sk(__sk)    ((VSockVmciSock *)(__sk)->user_data)
# define sk_vsock(__vsk)   ((__vsk)->sk)
#else
# define vsock_sk(__sk)    ((VSockVmciSock *)__sk)
# define sk_vsock(__vsk)   (&(__vsk)->sk)
#endif

typedef struct VSockVmciSock {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   struct sock *sk;
#else
   /* sk must be the first member. */
   struct sock  sk;
#endif
   struct sockaddr_vm localAddr;
   struct sockaddr_vm remoteAddr;
   /* Links for the global tables of bound and connected sockets. */
   struct list_head boundTable;
   struct list_head connectedTable;
   VMCIHandle dgHandle;           /* For SOCK_DGRAM only. */
#ifdef VMX86_TOOLS
   /* Rest are SOCK_STREAM only. */
   VMCIHandle qpHandle;
   VMCIQueue *produceQ;
   VMCIQueue *consumeQ;
   uint64 produceQGeneration;
   uint64 consumeQGeneration;
   uint64 produceSize;
   uint64 consumeSize;
   uint64 queuePairSize;
   uint64 queuePairMinSize;
   uint64 queuePairMaxSize;
   Bool peerWaitingRead;
   Bool peerWaitingWrite;
   VSockWaitingInfo peerWaitingReadInfo;
   VSockWaitingInfo peerWaitingWriteInfo;
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
#endif
} VSockVmciSock;

#endif /* __AF_VSOCK_H__ */

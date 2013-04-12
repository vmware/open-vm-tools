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
 * util.c --
 *
 *      Utility functions for Linux VSocket module.
 */

#include "driver-config.h"
#include <linux/list.h>
#include <linux/socket.h>
#include "compat_sock.h"

#include "af_vsock.h"
#include "util.h"

struct list_head vsockBindTable[VSOCK_HASH_SIZE + 1];
struct list_head vsockConnectedTable[VSOCK_HASH_SIZE];

DEFINE_SPINLOCK(vsockTableLock);


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciLogPkt --
 *
 *    Logs the provided packet.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciLogPkt(char const *function,   // IN
                uint32 line,            // IN
                VSockPacket *pkt)       // IN
{
   char buf[256];
   char *cur = buf;
   int left = sizeof buf;
   int written = 0;
   char *typeStrings[] = {
      [VSOCK_PACKET_TYPE_INVALID]        = "INVALID",
      [VSOCK_PACKET_TYPE_REQUEST]        = "REQUEST",
      [VSOCK_PACKET_TYPE_NEGOTIATE]      = "NEGOTIATE",
      [VSOCK_PACKET_TYPE_OFFER]          = "OFFER",
      [VSOCK_PACKET_TYPE_ATTACH]         = "ATTACH",
      [VSOCK_PACKET_TYPE_WROTE]          = "WROTE",
      [VSOCK_PACKET_TYPE_READ]           = "READ",
      [VSOCK_PACKET_TYPE_RST]            = "RST",
      [VSOCK_PACKET_TYPE_SHUTDOWN]       = "SHUTDOWN",
      [VSOCK_PACKET_TYPE_WAITING_WRITE]  = "WAITING_WRITE",
      [VSOCK_PACKET_TYPE_WAITING_READ]   = "WAITING_READ",
      [VSOCK_PACKET_TYPE_REQUEST2]       = "REQUEST2",
      [VSOCK_PACKET_TYPE_NEGOTIATE2]     = "NEGOTIATE2",
   };

   written = snprintf(cur, left, "PKT: %u:%u -> %u:%u",
                      VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                      pkt->srcPort,
                      VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst),
                      pkt->dstPort);
   if (written >= left) {
      goto error;
   }

   left -= written;
   cur += written;

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_REQUEST:
   case VSOCK_PACKET_TYPE_NEGOTIATE:
      written = snprintf(cur, left, ", %s, size = %"FMT64"u",
                         typeStrings[pkt->type], pkt->u.size);
      break;

   case VSOCK_PACKET_TYPE_OFFER:
   case VSOCK_PACKET_TYPE_ATTACH:
      written = snprintf(cur, left, ", %s, handle = %u:%u",
                         typeStrings[pkt->type],
                         VMCI_HANDLE_TO_CONTEXT_ID(pkt->u.handle),
                         VMCI_HANDLE_TO_RESOURCE_ID(pkt->u.handle));
      break;

   case VSOCK_PACKET_TYPE_WROTE:
   case VSOCK_PACKET_TYPE_READ:
   case VSOCK_PACKET_TYPE_RST:
      written = snprintf(cur, left, ", %s", typeStrings[pkt->type]);
      break;
   case VSOCK_PACKET_TYPE_SHUTDOWN: {
      Bool recv;
      Bool send;

      recv = pkt->u.mode & RCV_SHUTDOWN;
      send = pkt->u.mode & SEND_SHUTDOWN;
      written = snprintf(cur, left, ", %s, mode = %c%c",
                         typeStrings[pkt->type],
                         recv ? 'R' : ' ',
                         send ? 'S' : ' ');
   }
   break;

   case VSOCK_PACKET_TYPE_WAITING_WRITE:
   case VSOCK_PACKET_TYPE_WAITING_READ:
      written = snprintf(cur, left, ", %s, generation = %"FMT64"u, "
                         "offset = %"FMT64"u", typeStrings[pkt->type],
                         pkt->u.wait.generation, pkt->u.wait.offset);

      break;

   case VSOCK_PACKET_TYPE_REQUEST2:
   case VSOCK_PACKET_TYPE_NEGOTIATE2:
      written = snprintf(cur, left, ", %s, size = %"FMT64"u, "
                         "proto = %u",
                         typeStrings[pkt->type], pkt->u.size,
                         pkt->proto);
      break;

   default:
      written = snprintf(cur, left, ", unrecognized type");
   }

   if (written >= left) {
      goto error;
   }

   left -= written;
   cur += written;

   written = snprintf(cur, left, "  [%s:%u]\n", function, line);
   if (written >= left) {
      goto error;
   }

   LOG(8, ("%s", buf));

   return;

error:
   LOG(8, ("could not log packet\n"));
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInitTables --
 *
 *    Initializes the tables used for socket lookup.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciInitTables(void)
{
   uint32 i;

   for (i = 0; i < ARRAYSIZE(vsockBindTable); i++) {
      INIT_LIST_HEAD(&vsockBindTable[i]);
   }

   for (i = 0; i < ARRAYSIZE(vsockConnectedTable); i++) {
      INIT_LIST_HEAD(&vsockConnectedTable[i]);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciInsertBound --
 *
 *    Inserts socket into the bound table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count for sk is incremented.
 *
 *----------------------------------------------------------------------------
 */

void
__VSockVmciInsertBound(struct list_head *list,          // IN
                       struct sock *sk)                 // IN
{
   VSockVmciSock *vsk;

   ASSERT(list);
   ASSERT(sk);

   vsk = vsock_sk(sk);

   sock_hold(sk);
   list_add(&vsk->boundTable, list);
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciInsertConnected --
 *
 *    Inserts socket into the connected table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count for sk is incremented.
 *
 *----------------------------------------------------------------------------
 */

void
__VSockVmciInsertConnected(struct list_head *list,   // IN
                           struct sock *sk)          // IN
{
   VSockVmciSock *vsk;

   ASSERT(list);
   ASSERT(sk);

   vsk = vsock_sk(sk);

   sock_hold(sk);
   list_add(&vsk->connectedTable, list);
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciRemoveBound --
 *
 *    Removes socket from the bound table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count for sk is decremented.
 *
 *----------------------------------------------------------------------------
 */

void
__VSockVmciRemoveBound(struct sock *sk)  // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(__VSockVmciInBoundTable(sk));

   vsk = vsock_sk(sk);

   list_del_init(&vsk->boundTable);
   sock_put(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciRemoveConnected --
 *
 *    Removes socket from the connected table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count for sk is decremented.
 *
 *----------------------------------------------------------------------------
 */

void
__VSockVmciRemoveConnected(struct sock *sk)  // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(__VSockVmciInConnectedTable(sk));

   vsk = vsock_sk(sk);

   list_del_init(&vsk->connectedTable);
   sock_put(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciFindBoundSocket --
 *
 *    Finds the socket corresponding to the provided address in the bound
 *    sockets hash table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    The sock structure if found, NULL if not found.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

struct sock *
__VSockVmciFindBoundSocket(struct sockaddr_vm *addr)  // IN
{
   VSockVmciSock *vsk;
   struct sock *sk;

   ASSERT(addr);

   list_for_each_entry(vsk, vsockBoundSockets(addr), boundTable) {
      if (addr->svm_port == vsk->localAddr.svm_port) {
         sk = sk_vsock(vsk);

         /* We only store stream sockets in the bound table. */
         ASSERT(sk->sk_socket ?
                   sk->sk_socket->type == SOCK_STREAM :
                   1);
         goto found;
      }
   }

   sk = NULL;
found:
   return sk;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciFindConnectedSocket --
 *
 *    Finds the socket corresponding to the provided addresses in the connected
 *    sockets hash table.
 *
 *    Note that this assumes any necessary locks are held.
 *
 * Results:
 *    The sock structure if found, NULL if not found.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

struct sock *
__VSockVmciFindConnectedSocket(struct sockaddr_vm *src,    // IN
                               struct sockaddr_vm *dst)    // IN
{
   VSockVmciSock *vsk;
   struct sock *sk;

   ASSERT(src);
   ASSERT(dst);

   list_for_each_entry(vsk, vsockConnectedSockets(src, dst), connectedTable) {
      if (VSockAddr_EqualsAddr(src, &vsk->remoteAddr) &&
          dst->svm_port == vsk->localAddr.svm_port) {
         sk = sk_vsock(vsk);
         goto found;
      }
   }

   sk = NULL;
found:
   return sk;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciInBoundTable --
 *
 *    Determines whether the provided socket is in the bound table.
 *
 * Results:
 *    TRUE is socket is in bound table, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
__VSockVmciInBoundTable(struct sock *sk)     // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   return !list_empty(&vsk->boundTable);
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciInConnectedTable --
 *
 *    Determines whether the provided socket is in the connected table.
 *
 * Results:
 *    TRUE is socket is in connected table, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
__VSockVmciInConnectedTable(struct sock *sk)     // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   return !list_empty(&vsk->connectedTable);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciGetPending --
 *
 *    Retrieves a pending connection that matches the addresses specified in
 *    the provided packet.
 *
 *    Assumes the socket lock is held for listener.
 *
 * Results:
 *    Socket of the pending connection on success, NULL if not found.
 *
 * Side effects:
 *    A reference is held on the socket until the release function is called.
 *
 *----------------------------------------------------------------------------
 */

struct sock *
VSockVmciGetPending(struct sock *listener,      // IN: listening socket
                    VSockPacket *pkt)           // IN: incoming packet
{
   VSockVmciSock *vlistener;
   VSockVmciSock *vpending;
   struct sock *pending;
   struct sockaddr_vm src;

   ASSERT(listener);
   ASSERT(pkt);

   VSockAddr_Init(&src, VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src), pkt->srcPort);

   vlistener = vsock_sk(listener);

   list_for_each_entry(vpending, &vlistener->pendingLinks, pendingLinks) {
      if (VSockAddr_EqualsAddr(&src, &vpending->remoteAddr) &&
          pkt->dstPort == vpending->localAddr.svm_port) {
         pending = sk_vsock(vpending);
         sock_hold(pending);
         goto found;
      }
   }

   pending = NULL;
found:
   return pending;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciReleasePending --
 *
 *    Releases the reference on a socket previously obtained by a call to
 *    VSockVmciGetPending().
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The socket may be freed if this was the last reference.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciReleasePending(struct sock *pending)   // IN: pending connection
{
   ASSERT(pending);

   sock_put(pending);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciAddPending --
 *
 *    Adds a pending connection on listener's pending list.
 *
 *    Assumes the socket lock is held for listener.
 *    Assumes the socket lock is held for pending.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count of the sockets is incremented.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciAddPending(struct sock *listener,  // IN: listening socket
                    struct sock *pending)   // IN: pending connection
{
   VSockVmciSock *vlistener;
   VSockVmciSock *vpending;

   ASSERT(listener);
   ASSERT(pending);

   vlistener = vsock_sk(listener);
   vpending = vsock_sk(pending);

   sock_hold(pending);
   sock_hold(listener);
   list_add_tail(&vpending->pendingLinks, &vlistener->pendingLinks);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRemovePending --
 *
 *    Removes a pending connection from the listener's pending list.
 *
 *    Assumes the socket lock is held for listener.
 *    Assumes the socket lock is held for pending.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The reference count of the sockets is decremented.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciRemovePending(struct sock *listener,   // IN: listening socket
                       struct sock *pending)    // IN: pending connection
{
   VSockVmciSock *vpending;

   ASSERT(listener);
   ASSERT(pending);

   vpending = vsock_sk(pending);

   list_del_init(&vpending->pendingLinks);
   sock_put(listener);
   sock_put(pending);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciEnqueueAccept --
 *
 *    Enqueues the connected socket on the listening socket's accepting
 *    queue.
 *
 *    Assumes the socket lock is held for listener.
 *    Assumes the socket lock is held for connected.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The sockets' reference counts are incremented.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciEnqueueAccept(struct sock *listener,    // IN: listening socket
                       struct sock *connected)   // IN: connected socket
{
   VSockVmciSock *vlistener;
   VSockVmciSock *vconnected;

   ASSERT(listener);
   ASSERT(connected);

   vlistener = vsock_sk(listener);
   vconnected = vsock_sk(connected);

   sock_hold(connected);
   sock_hold(listener);
   list_add_tail(&vconnected->acceptQueue, &vlistener->acceptQueue);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDequeueAccept --
 *
 *    Dequeues the next connected socket from the listening socket's accept
 *    queue.
 *
 *    Assumes the socket lock is held for listener.
 *
 *    Note that the caller must call sock_put() on the returned socket once it
 *    is done with the socket.
 *
 * Results:
 *    The next socket from the queue, or NULL if the queue is empty.
 *
 * Side effects:
 *    The reference count of the listener is decremented.
 *
 *----------------------------------------------------------------------------
 */

struct sock *
VSockVmciDequeueAccept(struct sock *listener)     // IN: listening socket
{
   VSockVmciSock *vlistener;
   VSockVmciSock *vconnected;

   ASSERT(listener);

   vlistener = vsock_sk(listener);

   if (list_empty(&vlistener->acceptQueue)) {
      return NULL;
   }

   vconnected = list_entry(vlistener->acceptQueue.next,
                           VSockVmciSock, acceptQueue);
   ASSERT(vconnected);

   list_del_init(&vconnected->acceptQueue);
   sock_put(listener);
   /*
    * The caller will need a reference on the connected socket so we let it
    * call sock_put().
    */

   ASSERT(sk_vsock(vconnected));
   return sk_vsock(vconnected);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRemoveAccept --
 *
 *    Removes a socket from the accept queue of a listening socket.
 *
 *    Assumes the socket lock is held for listener.
 *    Assumes the socket lock is held for connected.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The sockets' reference counts are decremented.
 *
 *----------------------------------------------------------------------------
 */

void
VSockVmciRemoveAccept(struct sock *listener,    // IN: listening socket
                      struct sock *connected)   // IN: connected socket
{
   VSockVmciSock *vconnected;

   ASSERT(listener);
   ASSERT(connected);

   if (!VSockVmciInAcceptQueue(connected)) {
      return;
   }

   vconnected = vsock_sk(connected);
   ASSERT(vconnected->listener == listener);

   list_del_init(&vconnected->acceptQueue);
   sock_put(listener);
   sock_put(connected);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInAcceptQueue --
 *
 *    Determines whether a socket is on an accept queue.
 *
 *    Assumes the socket lock is held for sk.
 *
 * Results:
 *    TRUE if the socket is in an accept queue, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockVmciInAcceptQueue(struct sock *sk)   // IN: socket
{
   ASSERT(sk);

   /*
    * If our accept queue isn't empty, it means we're linked into some listener
    * socket's accept queue.
    */
   return !VSockVmciIsAcceptQueueEmpty(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciIsAcceptQueueEmpty --
 *
 *    Determines whether the provided socket's accept queue is empty.
 *
 *    Assumes the socket lock is held for sk.
 *
 * Results:
 *    TRUE if the socket's accept queue is empty, FALSE otherwsise.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockVmciIsAcceptQueueEmpty(struct sock *sk)    // IN: socket
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);
   return list_empty(&vsk->acceptQueue);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciIsPending --
 *
 *    Determines whether a socket is pending.
 *
 *    Assumes the socket lock is held for sk.
 *
 * Results:
 *    TRUE if the socket is pending, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockVmciIsPending(struct sock *sk)     // IN: socket
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);
   return !list_empty(&vsk->pendingLinks);
}

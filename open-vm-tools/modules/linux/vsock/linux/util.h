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
 * util.h --
 *
 *      Utility functions for Linux VSocket module.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include "driver-config.h"
#include "compat_sock.h"
#include "compat_spinlock.h"

#include "vsockCommon.h"
#include "vsockPacket.h"

/*
 * Each bound VSocket is stored in the bind hash table and each connected
 * VSocket is stored in the connected hash table.
 *
 * Unbound sockets are all put on the same list attached to the end of the hash
 * table (vsockUnboundSockets).  Bound sockets are added to the hash table in
 * the bucket that their local address hashes to (vsockBoundSockets(addr)
 * represents the list that addr hashes to).
 *
 * Specifically, we initialize the vsockBindTable array to a size of
 * VSOCK_HASH_SIZE + 1 so that vsockBindTable[0] through
 * vsockBindTable[VSOCK_HASH_SIZE - 1] are for bound sockets and
 * vsockBindTable[VSOCK_HASH_SIZE] is for unbound sockets.  The hash function
 * mods with VSOCK_HASH_SIZE - 1 to ensure this.
 */
#define VSOCK_HASH_SIZE         251
#define LAST_RESERVED_PORT      1023
#define MAX_PORT_RETRIES        24

extern struct list_head vsockBindTable[VSOCK_HASH_SIZE + 1];
extern struct list_head vsockConnectedTable[VSOCK_HASH_SIZE];

extern spinlock_t vsockTableLock;

#define VSOCK_HASH(addr)        ((addr)->svm_port % (VSOCK_HASH_SIZE - 1))
#define vsockBoundSockets(addr) (&vsockBindTable[VSOCK_HASH(addr)])
#define vsockUnboundSockets     (&vsockBindTable[VSOCK_HASH_SIZE])

/* XXX This can probably be implemented in a better way. */
#define VSOCK_CONN_HASH(src, dst)       \
   (((src)->svm_cid ^ (dst)->svm_port) % (VSOCK_HASH_SIZE - 1))
#define vsockConnectedSockets(src, dst) \
   (&vsockConnectedTable[VSOCK_CONN_HASH(src, dst)])
#define vsockConnectedSocketsVsk(vsk)    \
   vsockConnectedSockets(&(vsk)->remoteAddr, &(vsk)->localAddr)

/*
 * Prototypes.
 */

void VSockVmciLogPkt(char const *function, uint32 line, VSockPacket *pkt);

void VSockVmciInitTables(void);
void __VSockVmciInsertBound(struct list_head *list, struct sock *sk);
void __VSockVmciInsertConnected(struct list_head *list, struct sock *sk);
void __VSockVmciRemoveBound(struct sock *sk);
void __VSockVmciRemoveConnected(struct sock *sk);
struct sock *__VSockVmciFindBoundSocket(struct sockaddr_vm *addr);
struct sock *__VSockVmciFindConnectedSocket(struct sockaddr_vm *src,
                                            struct sockaddr_vm *dst);
Bool __VSockVmciInBoundTable(struct sock *sk);
Bool __VSockVmciInConnectedTable(struct sock *sk);

struct sock *VSockVmciGetPending(struct sock *listener, VSockPacket *pkt);
void VSockVmciReleasePending(struct sock *pending);
void VSockVmciAddPending(struct sock *listener, struct sock *pending);
void VSockVmciRemovePending(struct sock *listener, struct sock *pending);
void VSockVmciEnqueueAccept(struct sock *listener, struct sock *connected);
struct sock *VSockVmciDequeueAccept(struct sock *listener);
void VSockVmciRemoveAccept(struct sock *listener, struct sock *connected);
Bool VSockVmciInAcceptQueue(struct sock *sk);
Bool VSockVmciIsAcceptQueueEmpty(struct sock *sk);
Bool VSockVmciIsPending(struct sock *sk);

static INLINE void VSockVmciInsertBound(struct list_head *list, struct sock *sk);
static INLINE void VSockVmciInsertConnected(struct list_head *list, struct sock *sk);
static INLINE void VSockVmciRemoveBound(struct sock *sk);
static INLINE void VSockVmciRemoveConnected(struct sock *sk);
static INLINE struct sock *VSockVmciFindBoundSocket(struct sockaddr_vm *addr);
static INLINE struct sock *VSockVmciFindConnectedSocket(struct sockaddr_vm *src,
                                                        struct sockaddr_vm *dst);
static INLINE Bool VSockVmciInBoundTable(struct sock *sk);
static INLINE Bool VSockVmciInConnectedTable(struct sock *sk);


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInsertBound --
 *
 *    Inserts socket into the bound table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciInsertBound(struct list_head *list,    // IN
                     struct sock *sk)           // IN
{
   ASSERT(list);
   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   __VSockVmciInsertBound(list, sk);
   spin_unlock_bh(&vsockTableLock);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInsertConnected --
 *
 *    Inserts socket into the connected table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciInsertConnected(struct list_head *list,    // IN
                         struct sock *sk)           // IN
{
   ASSERT(list);
   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   __VSockVmciInsertConnected(list, sk);
   spin_unlock_bh(&vsockTableLock);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRemoveBound --
 *
 *    Removes socket from the bound list.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciRemoveBound(struct sock *sk)                  // IN
{
   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   __VSockVmciRemoveBound(sk);
   spin_unlock_bh(&vsockTableLock);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRemoveConnected --
 *
 *    Removes socket from the connected list.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciRemoveConnected(struct sock *sk)                  // IN
{
   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   __VSockVmciRemoveConnected(sk);
   spin_unlock_bh(&vsockTableLock);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciFindBoundSocket --
 *
 *    Finds the socket corresponding to the provided address in the bound
 *    sockets hash table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these are called from tasklets.
 *
 * Results:
 *    The sock structure if found, NULL on failure.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *    The socket's reference count is increased.
 *
 *----------------------------------------------------------------------------
 */

static INLINE struct sock *
VSockVmciFindBoundSocket(struct sockaddr_vm *addr) // IN
{
   struct sock *sk;

   ASSERT(addr);

   spin_lock_bh(&vsockTableLock);
   sk = __VSockVmciFindBoundSocket(addr);
   if (sk) {
      sock_hold(sk);
   }
   spin_unlock_bh(&vsockTableLock);

   return sk;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciFindConnectedSocket --
 *
 *    Finds the socket corresponding to the provided address in the connected
 *    sockets hash table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these are called from tasklets.
 *
 * Results:
 *    The sock structure if found, NULL on failure.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *    The socket's reference count is increased.
 *
 *----------------------------------------------------------------------------
 */

static INLINE struct sock *
VSockVmciFindConnectedSocket(struct sockaddr_vm *src,   // IN
                             struct sockaddr_vm *dst)   // IN
{
   struct sock *sk;

   ASSERT(src);
   ASSERT(dst);

   spin_lock_bh(&vsockTableLock);
   sk = __VSockVmciFindConnectedSocket(src, dst);
   if (sk) {
      sock_hold(sk);
   }
   spin_unlock_bh(&vsockTableLock);

   return sk;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInBoundTable --
 *
 *    Determines whether the provided socket is in the bound table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    TRUE is socket is in bound table, FALSE otherwise.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
VSockVmciInBoundTable(struct sock *sk)  // IN
{
   Bool ret;

   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   ret = __VSockVmciInBoundTable(sk);
   spin_unlock_bh(&vsockTableLock);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInConnectedTable --
 *
 *    Determines whether the provided socket is in the connected table.
 *
 *    Note that it is important to invoke the bottom-half versions of the
 *    spinlock functions since these may be called from tasklets.
 *
 * Results:
 *    TRUE is socket is in connected table, FALSE otherwise.
 *
 * Side effects:
 *    vsockTableLock is acquired and released.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
VSockVmciInConnectedTable(struct sock *sk)  // IN
{
   Bool ret;

   ASSERT(sk);

   spin_lock_bh(&vsockTableLock);
   ret = __VSockVmciInConnectedTable(sk);
   spin_unlock_bh(&vsockTableLock);

   return ret;
}


#endif /* __UTIL_H__ */

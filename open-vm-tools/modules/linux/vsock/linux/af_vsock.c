/*********************************************************
 * Copyright (C) 2007-2014 VMware, Inc. All rights reserved.
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
 * af_vsock.c --
 *
 *      Linux socket module for the vSockets protocol family.
 */


/*
 * Implementation notes:
 *
 * - There are two kinds of sockets: those created by user action (such as
 *   calling socket(2)) and those created by incoming connection request
 *   packets.
 *
 * - There are two "global" tables, one for bound sockets (sockets that have
 *   specified an address that they are responsible for) and one for connected
 *   sockets (sockets that have established a connection with another socket).
 *   These tables are "global" in that all sockets on the system are placed
 *   within them.
 *   - Note, though, that the bound table contains an extra entry for a list of
 *     unbound sockets and SOCK_DGRAM sockets will always remain in that list.
 *     The bound table is used solely for lookup of sockets when packets are
 *     received and that's not necessary for SOCK_DGRAM sockets since we create
 *     a datagram handle for each and need not perform a lookup.  Keeping
 *     SOCK_DGRAM sockets out of the bound hash buckets will reduce the chance
 *     of collisions when looking for SOCK_STREAM sockets and prevents us from
 *     having to check the socket type in the hash table lookups.
 *
 * - Sockets created by user action will either be "client" sockets that
 *   initiate a connection or "server" sockets that listen for connections; we
 *   do not support simultaneous connects (two "client" sockets connecting).
 *
 * - "Server" sockets are referred to as listener sockets throughout this
 *   implementation because they are in the SS_LISTEN state.  When a connection
 *   request is received (the second kind of socket mentioned above), we create
 *   a new socket and refer to it as a pending socket.  These pending sockets
 *   are placed on the pending connection list of the listener socket.  When
 *   future packets are received for the address the listener socket is bound
 *   to, we check if the source of the packet is from one that has an existing
 *   pending connection.  If it does, we process the packet for the pending
 *   socket.  When that socket reaches the connected state, it is removed from
 *   the listener socket's pending list and enqueued in the listener socket's
 *   accept queue.  Callers of accept(2) will accept connected sockets from the
 *   listener socket's accept queue.  If the socket cannot be accepted for some
 *   reason then it is marked rejected.  Once the connection is accepted, it is
 *   owned by the user process and the responsibility for cleanup falls with
 *   that user process.
 *
 * - It is possible that these pending sockets will never reach the connected
 *   state; in fact, we may never receive another packet after the connection
 *   request.  Because of this, we must schedule a cleanup function to run in
 *   the future, after some amount of time passes where a connection should
 *   have been established.  This function ensures that the socket is off all
 *   lists so it cannot be retrieved, then drops all references to the socket
 *   so it is cleaned up (sock_put() -> sk_free() -> our sk_destruct
 *   implementation).  Note this function will also cleanup rejected sockets,
 *   those that reach the connected state but leave it before they have been
 *   accepted.
 *
 * - Sockets created by user action will be cleaned up when the user
 *   process calls close(2), causing our release implementation to be called.
 *   Our release implementation will perform some cleanup then drop the
 *   last reference so our sk_destruct implementation is invoked.  Our
 *   sk_destruct implementation will perform additional cleanup that's common
 *   for both types of sockets.
 *
 * - A socket's reference count is what ensures that the structure won't be
 *   freed.  Each entry in a list (such as the "global" bound and connected
 *   tables and the listener socket's pending list and connected queue) ensures
 *   a reference.  When we defer work until process context and pass a socket
 *   as our argument, we must ensure the reference count is increased to ensure
 *   the socket isn't freed before the function is run; the deferred function
 *   will then drop the reference.
 *
 */

#include "driver-config.h"

#define EXPORT_SYMTAB
#include <linux/kmod.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/init.h>
#include <asm/io.h>
#if defined(__x86_64__) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
#   include <linux/ioctl32.h>
/* Use weak: not all kernels export sys_ioctl for use by modules */
asmlinkage __attribute__((weak)) long
sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
#endif

#include "compat_cred.h"
#include "compat_module.h"
#include "compat_kernel.h"
#include "compat_sock.h"
#include "compat_version.h"
#include "compat_workqueue.h"
#include "compat_mutex.h"

#include "vmware.h"

#include "vsockCommon.h"
#include "vsockPacket.h"
#include "vsockVmci.h"

#include "vmci_iocontrols.h"

#include "af_vsock.h"
#include "stats.h"
#include "util.h"
#include "vsock_version.h"
#include "driverLog.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
#  error "Linux kernels before 2.6.9 are not supported."
#endif

/*
 * All kernels above 2.6.33 have the kern parameter for the create
 * call in struct net_proto_family.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33) && \
    !defined(VMW_NETCREATE_KERNARG)
#  define VMW_NETCREATE_KERNARG
#endif

#define VSOCK_INVALID_FAMILY        NPROTO
#define VSOCK_AF_IS_REGISTERED(val) ((val) >= 0 && (val) < NPROTO)

/* Some kernel versions don't define __user. Define it ourself if so. */
#ifndef __user
#define __user
#endif


/*
 * Prototypes
 */
int VSockVmci_GetAFValue(void);

/* Internal functions. */
static Bool VSockVmciProtoToNotifyStruct(struct sock *sk,
                                         VSockProtoVersion *proto,
                                         Bool oldPktProto);
static int VSockVmciGetAFValue(void);
static int VSockVmciRecvDgramCB(void *data, VMCIDatagram *dg);
static int VSockVmciRecvStreamCB(void *data, VMCIDatagram *dg);
static void VSockVmciPeerAttachCB(VMCIId subId,
                                  VMCI_EventData *ed, void *clientData);
static void VSockVmciPeerDetachCB(VMCIId subId,
                                  VMCI_EventData *ed, void *clientData);
static void VSockVmciRecvPktWork(compat_work_arg work);
static int VSockVmciRecvListen(struct sock *sk, VSockPacket *pkt);
static int VSockVmciRecvConnectingServer(struct sock *sk,
                                         struct sock *pending, VSockPacket *pkt);
static int VSockVmciRecvConnectingClient(struct sock *sk, VSockPacket *pkt);
static int VSockVmciRecvConnectingClientNegotiate(struct sock *sk,
                                                  VSockPacket *pkt);
static int VSockVmciRecvConnectingClientInvalid(struct sock *sk,
                                                VSockPacket *pkt);
static int VSockVmciRecvConnected(struct sock *sk, VSockPacket *pkt);
static int __VSockVmciBind(struct sock *sk, struct sockaddr_vm *addr);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
static struct sock *__VSockVmciCreate(struct socket *sock, struct sock *parent,
                                      unsigned int priority, unsigned short type);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static struct sock *__VSockVmciCreate(struct socket *sock, struct sock *parent,
                                      gfp_t priority, unsigned short type);
#else
static struct sock *__VSockVmciCreate(struct net *net,
                                      struct socket *sock, struct sock *parent,
                                      gfp_t priority, unsigned short type);
#endif
static void VSockVmciTestUnregister(void);
static int VSockVmciRegisterWithVmci(void);
static void VSockVmciUnregisterWithVmci(void);
static int VSockVmciRegisterAddressFamily(void);
static void VSockVmciUnregisterAddressFamily(void);

/* Socket operations. */
static void VSockVmciSkDestruct(struct sock *sk);
static int VSockVmciQueueRcvSkb(struct sock *sk, struct sk_buff *skb);
static int VSockVmciRelease(struct socket *sock);
static int VSockVmciBind(struct socket *sock,
                         struct sockaddr *addr, int addrLen);
static int VSockVmciDgramConnect(struct socket *sock,
                                 struct sockaddr *addr, int addrLen, int flags);
static int VSockVmciStreamConnect(struct socket *sock,
                                  struct sockaddr *addr, int addrLen, int flags);
static int VSockVmciAccept(struct socket *sock, struct socket *newsock, int flags);
static int VSockVmciGetname(struct socket *sock,
                            struct sockaddr *addr, int *addrLen, int peer);
static unsigned int VSockVmciPoll(struct file *file,
                                  struct socket *sock, poll_table *wait);
static int VSockVmciListen(struct socket *sock, int backlog);
static int VSockVmciShutdown(struct socket *sock, int mode);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
typedef int VSockSetsockoptLenType;
#else
typedef unsigned int VSockSetsockoptLenType;
#endif
static int VSockVmciStreamSetsockopt(struct socket *sock, int level, int optname,
                                     char __user *optval,
                                     VSockSetsockoptLenType optlen);

static int VSockVmciStreamGetsockopt(struct socket *sock, int level, int optname,
                                     char __user *optval, int __user * optlen);

static int VSockVmciDgramSendmsg(struct kiocb *kiocb,
                                 struct socket *sock, struct msghdr *msg, size_t len);
static int VSockVmciDgramRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, size_t len, int flags);
static int VSockVmciStreamSendmsg(struct kiocb *kiocb,
                                 struct socket *sock, struct msghdr *msg, size_t len);
static int VSockVmciStreamRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, size_t len, int flags);

static int VSockVmciCreate(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
                           struct net *net,
#endif
                           struct socket *sock, int protocol
#ifdef VMW_NETCREATE_KERNARG
                           , int kern
#endif
                          );


/*
 * Device operations.
 */
int VSockVmciDevOpen(struct inode *inode, struct file *file);
int VSockVmciDevRelease(struct inode *inode, struct file *file);
static int VSockVmciDevIoctl(struct inode *inode, struct file *filp,
                             u_int iocmd, unsigned long ioarg);
#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
static long VSockVmciDevUnlockedIoctl(struct file *filp,
                                      u_int iocmd, unsigned long ioarg);
#endif

/*
 * Variables.
 */

/* Protocol family. */
static struct proto vsockVmciProto = {
   .name     = "AF_VMCI",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
   /* Added in 2.6.10. */
   .owner    = THIS_MODULE,
#endif
   /*
    * From 2.6.9 until 2.6.11, these address families called sk_alloc_slab()
    * and the allocated slab was assigned to the slab variable in the proto
    * struct and was created of size slab_obj_size.
    * As of 2.6.12 and later, this slab allocation was moved into
    * proto_register() and only done if you specified a non-zero value for
    * the second argument (alloc_slab); the size of the slab element was
    * changed to obj_size.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   .slab_obj_size = sizeof (VSockVmciSock),
#else
   .obj_size = sizeof (VSockVmciSock),
#endif
};

static struct net_proto_family vsockVmciFamilyOps = {
   .family = VSOCK_INVALID_FAMILY,
   .create = VSockVmciCreate,
   .owner  = THIS_MODULE,
};

/* Socket operations, split for DGRAM and STREAM sockets. */
static struct proto_ops vsockVmciDgramOps = {
   .family     = VSOCK_INVALID_FAMILY,
   .owner      = THIS_MODULE,
   .release    = VSockVmciRelease,
   .bind       = VSockVmciBind,
   .connect    = VSockVmciDgramConnect,
   .socketpair = sock_no_socketpair,
   .accept     = sock_no_accept,
   .getname    = VSockVmciGetname,
   .poll       = VSockVmciPoll,
   .ioctl      = sock_no_ioctl,
   .listen     = sock_no_listen,
   .shutdown   = VSockVmciShutdown,
   .setsockopt = sock_no_setsockopt,
   .getsockopt = sock_no_getsockopt,
   .sendmsg    = VSockVmciDgramSendmsg,
   .recvmsg    = VSockVmciDgramRecvmsg,
   .mmap       = sock_no_mmap,
   .sendpage   = sock_no_sendpage,
};

static struct proto_ops vsockVmciStreamOps = {
   .family     = VSOCK_INVALID_FAMILY,
   .owner      = THIS_MODULE,
   .release    = VSockVmciRelease,
   .bind       = VSockVmciBind,
   .connect    = VSockVmciStreamConnect,
   .socketpair = sock_no_socketpair,
   .accept     = VSockVmciAccept,
   .getname    = VSockVmciGetname,
   .poll       = VSockVmciPoll,
   .ioctl      = sock_no_ioctl,
   .listen     = VSockVmciListen,
   .shutdown   = VSockVmciShutdown,
   .setsockopt = VSockVmciStreamSetsockopt,
   .getsockopt = VSockVmciStreamGetsockopt,
   .sendmsg    = VSockVmciStreamSendmsg,
   .recvmsg    = VSockVmciStreamRecvmsg,
   .mmap       = sock_no_mmap,
   .sendpage   = sock_no_sendpage,
};

static struct file_operations vsockVmciDeviceOps = {
   .owner = THIS_MODULE,
#ifdef HAVE_UNLOCKED_IOCTL
   .unlocked_ioctl = VSockVmciDevUnlockedIoctl,
#else
   .ioctl = VSockVmciDevIoctl,
#endif
#ifdef HAVE_COMPAT_IOCTL
   .compat_ioctl = VSockVmciDevUnlockedIoctl,
#endif
   .open = VSockVmciDevOpen,
   .release = VSockVmciDevRelease,
};

static struct miscdevice vsockVmciDevice = {
   .name = "vsock",
   .minor = MISC_DYNAMIC_MINOR,
   .fops = &vsockVmciDeviceOps,
};

typedef struct VSockRecvPktInfo {
   compat_work work;
   struct sock *sk;
   VSockPacket pkt;
} VSockRecvPktInfo;

static compat_define_mutex(registrationMutex);
static int devOpenCount = 0;
static int vsockVmciSocketCount = 0;
static int vsockVmciKernClientCount = 0;
static Bool vmciDevicePresent = FALSE;
static VMCIHandle vmciStreamHandle = { VMCI_INVALID_ID, VMCI_INVALID_ID };
static VMCIId qpResumedSubId = VMCI_INVALID_ID;

static int PROTOCOL_OVERRIDE = -1;

/*
 * Netperf benchmarks have shown significant throughput improvements when the
 * QP size is bumped from 64k to 256k. These measurements were taken during the
 * K/L.next timeframe. Give users better performance by default.
 */
#define VSOCK_DEFAULT_QP_SIZE_MIN   128
#define VSOCK_DEFAULT_QP_SIZE       262144
#define VSOCK_DEFAULT_QP_SIZE_MAX   262144

/*
 * The default peer timeout indicates how long we will wait for a peer
 * response to a control message.
 */
#define VSOCK_DEFAULT_CONNECT_TIMEOUT (2 * HZ)

#ifdef VMX86_DEVEL
# define LOG_PACKET(_pkt)  VSockVmciLogPkt(__FUNCTION__, __LINE__, _pkt)
#else
# define LOG_PACKET(_pkt)
#endif


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciOldProtoOverride --
 *
 *      Check to see if the user has asked us to override all sockets to use
 *      the vsock notify protocol.
 *
 * Results:
 *      TRUE if there is a protocol override in effect.
 *       - oldPktProto is TRUE the original protocol should be used.
 *      FALSE if there is no override in effect.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciOldProtoOverride(Bool *oldPktProto)        // IN
{
   ASSERT(oldPktProto);

   if (PROTOCOL_OVERRIDE != -1) {
      if (PROTOCOL_OVERRIDE == 0) {
         *oldPktProto = TRUE;
      } else {
         *oldPktProto = FALSE;
      }
      Warning("Proto override in use.\n");
      return TRUE;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciProtoToNotifyStruct --
 *
 *      Given a particular notify protocol version, setup the socket's notify
 *      struct correctly.
 *
 * Results:
 *      TRUE on success. FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
VSockVmciProtoToNotifyStruct(struct sock *sk,          // IN
                             VSockProtoVersion *proto, // IN
                             Bool oldPktProto)         // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(proto);

   vsk = vsock_sk(sk);

   if (oldPktProto) {
      if (*proto != VSOCK_PROTO_INVALID) {
         Warning("Can't set both an old and new protocol\n");
         return FALSE;
      }
      vsk->notifyOps = &vSockVmciNotifyPktOps;
      goto exit;
   }

   switch(*proto) {
   case VSOCK_PROTO_PKT_ON_NOTIFY:
      vsk->notifyOps= &vSockVmciNotifyPktQStateOps;
      break;
   default:
      Warning("Unknown notify protocol version\n");
      return FALSE;
   }

exit:
   NOTIFYCALL(vsk, socketInit, sk);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciNewProtoSupportedVersions
 *
 *      Gets the supported REQUEST2/NEGOTIATE2 vsock protocol versions.
 *
 * Results:
 *      Either 1 specific protocol version (override mode) or
 *      VSOCK_PROTO_ALL_SUPPORTED.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static VSockProtoVersion
VSockVmciNewProtoSupportedVersions(void) // IN
{
   if (PROTOCOL_OVERRIDE != -1) {
      return PROTOCOL_OVERRIDE;
   }

   return VSOCK_PROTO_ALL_SUPPORTED;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockSocket_Trusted --
 *
 *      We allow two kinds of sockets to communicate with a restricted VM:
 *      1) trusted sockets
 *      2) sockets from applications running as the same user as the VM (this
 *         is only true for the host side and only when using hosted products)
 *
 * Results:
 *      TRUE if trusted communication is allowed to peerCid, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockVmciTrusted(VSockVmciSock *vsock, // IN: Local socket
                 VMCIId peerCid)       // IN: Context ID of peer
{
   return vsock->trusted || vmci_is_context_owner(peerCid, vsock->owner);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockSocket_AllowDgram --
 *
 *      We allow sending datagrams to and receiving datagrams from a
 *      restricted VM only if it is trusted as described in
 *      VSockVmciTrusted.
 *
 * Results:
 *      TRUE if datagram communication is allowed to peerCid, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VSockVmciAllowDgram(VSockVmciSock *vsock, // IN: Local socket
                    VMCIId peerCid)       // IN: Context ID of peer
{
   if (peerCid == VMCI_HYPERVISOR_CONTEXT_ID) {
      return TRUE;
   }

   if (vsock->cachedPeer != peerCid) {
      vsock->cachedPeer = peerCid;
      if (!VSockVmciTrusted(vsock, peerCid) &&
          (vmci_context_get_priv_flags(peerCid) &
           VMCI_PRIVILEGE_FLAG_RESTRICTED)) {
         vsock->cachedPeerAllowDgram = FALSE;
      } else {
         vsock->cachedPeerAllowDgram = TRUE;
      }
   }

   return vsock->cachedPeerAllowDgram;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCISock_GetAFValue --
 *
 *      Kernel interface that allows external kernel modules to get the current
 *      vSockets address family.
 *      This version of the function is exported to kernel clients and should not
 *      change.
 *
 * Results:
 *      The address family on success, a negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VMCISock_GetAFValue(void)
{
   int afvalue;

   compat_mutex_lock(&registrationMutex);

   /*
    * Kernel clients are required to explicitly register themselves before they
    * can use vSockets.
    */
   if (vsockVmciKernClientCount <= 0) {
      afvalue = -1;
      goto exit;
   }

   afvalue = VSockVmciGetAFValue();

exit:
   compat_mutex_unlock(&registrationMutex);
   return afvalue;
}
EXPORT_SYMBOL(VMCISock_GetAFValue);


/*
 *----------------------------------------------------------------------------
 *
 * VMCISock_GetLocalCID --
 *
 *      Kernel interface that allows external kernel modules to get the current
 *      VMCI context id.
 *      This version of the function is exported to kernel clients and should not
 *      change.
 *
 * Results:
 *      The context id on success, a negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VMCISock_GetLocalCID(void)
{
   int cid;

   compat_mutex_lock(&registrationMutex);

   /*
    * Kernel clients are required to explicitly register themselves before they
    * can use vSockets.
    */
   if (vsockVmciKernClientCount <= 0) {
      cid = -1;
      goto exit;
   }

   cid = vmci_get_context_id();

exit:
   compat_mutex_unlock(&registrationMutex);
   return cid;
}
EXPORT_SYMBOL(VMCISock_GetLocalCID);


/*
 *----------------------------------------------------------------------------
 *
 * VMCISock_KernelRegister --
 *
 *      Allows a kernel client to register with vSockets. Must be called
 *      before VMCISock_GetAFValue within a kernel module. Note that we don't
 *      actually register the address family until the first time the module
 *      needs to use it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCISock_KernelRegister(void)
{
   compat_mutex_lock(&registrationMutex);
   vsockVmciKernClientCount++;
   compat_mutex_unlock(&registrationMutex);
}
EXPORT_SYMBOL(VMCISock_KernelRegister);


/*
 *----------------------------------------------------------------------------
 *
 * VMCISock_KernelDeregister --
 *
 *      Allows a kernel client to unregister with vSockets. Every call
 *      to VMCISock_KernRegister must be matched with a call to
 *      VMCISock_KernUnregister.
 *
 * Results:
        None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCISock_KernelDeregister(void)
{
   compat_mutex_lock(&registrationMutex);
   vsockVmciKernClientCount--;
   VSockVmciTestUnregister();
   compat_mutex_unlock(&registrationMutex);
}
EXPORT_SYMBOL(VMCISock_KernelDeregister);


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciGetAFValue --
 *
 *      Returns the address family value being used.
 *      Note: The registration mutex must be held when calling this function.
 *
 * Results:
 *      The address family on success, a negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciGetAFValue(void)
{
   int afvalue;

   afvalue = vsockVmciFamilyOps.family;
   if (!VSOCK_AF_IS_REGISTERED(afvalue)) {
      afvalue = VSockVmciRegisterAddressFamily();
   }

   return afvalue;
}

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmci_GetAFValue --
 *
 *      Returns the address family value being used.
 *
 * Results:
 *      The address family on success, a negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmci_GetAFValue(void)
{
   int afvalue;

   compat_mutex_lock(&registrationMutex);
   afvalue = VSockVmciGetAFValue();
   compat_mutex_unlock(&registrationMutex);

   return afvalue;
}


/*
 * Helper functions.
 */

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciQueuePairAlloc --
 *
 *      Allocates or attaches to a queue pair. Tries to register with trusted
 *      status if requested but does not fail if the queuepair could not be
 *      allocate as trusted (running in the guest)
 *
 * Results:
 *      0 on success. A VSock error on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciQueuePairAlloc(VMCIQPair **qpair,    // OUT
                        VMCIHandle *handle,   // IN/OUT
                        uint64 produceSize,   // IN
                        uint64 consumeSize,   // IN
                        VMCIId peer,          // IN
                        uint32 flags,         // IN
                        Bool trusted)         // IN
{
   int err = 0;

   if (trusted) {
      /*
       * Try to allocate our queue pair as trusted. This will only work
       * if vsock is running in the host.
       */

      err = vmci_qpair_alloc(qpair, handle, produceSize, consumeSize,
                             peer, flags, VMCI_PRIVILEGE_FLAG_TRUSTED);
      if (err != VMCI_ERROR_NO_ACCESS) {
         goto out;
      }
   }

   err = vmci_qpair_alloc(qpair, handle, produceSize, consumeSize,
                          peer, flags, VMCI_NO_PRIVILEGE_FLAGS);
out:
   if (err < 0) {
      Log("Could not attach to queue pair with %d\n", err);
      err = VSockVmci_ErrorToVSockError(err);
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDatagramCreateHnd --
 *
 *      Creates a datagram handle. Tries to register with trusted
 *      status but does not fail if the handler could not be allocated
 *      as trusted (running in the guest).
 *
 * Results:
 *      0 on success. A VMCI error on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciDatagramCreateHnd(VMCIId resourceID,            // IN
                           uint32 flags,                 // IN
                           VMCIDatagramRecvCB recvCB,    // IN
                           void *clientData,             // IN
                           VMCIHandle *outHandle)        // OUT
{
   int err = 0;

   /*
    * Try to allocate our datagram handler as trusted. This will only work
    * if vsock is running in the host.
    */

   err = vmci_datagram_create_handle_priv(resourceID, flags,
                                          VMCI_PRIVILEGE_FLAG_TRUSTED,
                                          recvCB, clientData, outHandle);

   if (err == VMCI_ERROR_NO_ACCESS) {
      err = vmci_datagram_create_handle(resourceID, flags, recvCB, clientData,
                                        outHandle);
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciTestUnregister --
 *
 *      Tests if it's necessary to unregister the socket family, and does so.
 *
 *      Note that this assumes the registration lock is held.
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
VSockVmciTestUnregister(void)
{
   if (devOpenCount <= 0 && vsockVmciSocketCount <= 0 &&
       vsockVmciKernClientCount <= 0) {
      if (VSOCK_AF_IS_REGISTERED(vsockVmciFamilyOps.family)) {
         VSockVmciUnregisterAddressFamily();
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvDgramCB --
 *
 *    VMCI Datagram receive callback.  This function is used specifically for
 *    SOCK_DGRAM sockets.
 *
 *    This is invoked as part of a tasklet that's scheduled when the VMCI
 *    interrupt fires.  This is run in bottom-half context and if it ever needs
 *    to sleep it should defer that work to a work queue.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    An sk_buff is created and queued with this socket.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvDgramCB(void *data,          // IN
                     VMCIDatagram *dg)    // IN
{
   struct sock *sk;
   size_t size;
   struct sk_buff *skb;
   VSockVmciSock *vsk;

   ASSERT(dg);
   ASSERT(dg->payloadSize <= VMCI_MAX_DG_PAYLOAD_SIZE);

   sk = (struct sock *)data;

   ASSERT(sk);
   /* XXX Figure out why sk->sk_socket can be NULL. */
   ASSERT(sk->sk_socket ? sk->sk_socket->type == SOCK_DGRAM : 1);

   /*
    * This handler is privileged when this module is running on the
    * host. We will get datagrams from all endpoints (even VMs that
    * are in a restricted context). If we get one from a restricted
    * context then the destination socket must be trusted.
    *
    * NOTE: We access the socket struct without holding the lock here. This
    * is ok because the field we are interested is never modified outside
    * of the create and destruct socket functions.
    */
   vsk = vsock_sk(sk);
   if (!VSockVmciAllowDgram(vsk, VMCI_HANDLE_TO_CONTEXT_ID(dg->src))) {
      return VMCI_ERROR_NO_ACCESS;
   }

   size = VMCI_DG_SIZE(dg);

   /*
    * Attach the packet to the socket's receive queue as an sk_buff.
    */
   skb = alloc_skb(size, GFP_ATOMIC);
   if (skb) {
      /* compat_sk_receive_skb() will do a sock_put(), so hold here. */
      sock_hold(sk);
      skb_put(skb, size);
      memcpy(skb->data, dg, size);
      compat_sk_receive_skb(sk, skb, 0);
   }

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvStreamCB --
 *
 *    VMCI stream receive callback for control datagrams.  This function is
 *    used specifically for SOCK_STREAM sockets.
 *
 *    This is invoked as part of a tasklet that's scheduled when the VMCI
 *    interrupt fires.  This is run in bottom-half context but it defers most
 *    of its work to the packet handling work queue.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvStreamCB(void *data,           // IN
                      VMCIDatagram *dg)     // IN
{
   struct sock *sk;
   struct sockaddr_vm dst;
   struct sockaddr_vm src;
   VSockPacket *pkt;
   VSockVmciSock *vsk;
   VMCIId expectedSrcRid;
   Bool bhProcessPkt;
   int err;

   ASSERT(dg);
   ASSERT(dg->payloadSize <= VMCI_MAX_DG_PAYLOAD_SIZE);

   sk = NULL;
   err = VMCI_SUCCESS;
   bhProcessPkt = FALSE;

   /*
    * Ignore incoming packets from contexts without sockets, or resources that
    * aren't vsock implementations.
    */

   expectedSrcRid =
      VMCI_HYPERVISOR_CONTEXT_ID == VMCI_HANDLE_TO_CONTEXT_ID(dg->src) ?
      VSOCK_PACKET_HYPERVISOR_RID : VSOCK_PACKET_RID;

   if (!VSockAddr_SocketContextStream(VMCI_HANDLE_TO_CONTEXT_ID(dg->src)) ||
       expectedSrcRid != VMCI_HANDLE_TO_RESOURCE_ID(dg->src)) {
      return VMCI_ERROR_NO_ACCESS;
   }

   if (VMCI_DG_SIZE(dg) < sizeof *pkt) {
      /* Drop datagrams that do not contain full VSock packets. */
      return VMCI_ERROR_INVALID_ARGS;
   }

   pkt = (VSockPacket *)dg;

   LOG_PACKET(pkt);

   /*
    * Find the socket that should handle this packet.  First we look for
    * a connected socket and if there is none we look for a socket bound to
    * the destintation address.
    *
    * Note that we don't initialize the family member of the src and dst
    * sockaddr_vm since we don't want to call VMCISock_GetAFValue() and
    * possibly register the address family.
    */
   VSockAddr_InitNoFamily(&src,
                          VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                          pkt->srcPort);

   VSockAddr_InitNoFamily(&dst,
                          VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst),
                          pkt->dstPort);

   sk = VSockVmciFindConnectedSocket(&src, &dst);
   if (!sk) {
      sk = VSockVmciFindBoundSocket(&dst);
      if (!sk) {
         /*
          * We could not find a socket for this specified address.  If this
          * packet is a RST, we just drop it.  If it is another packet, we send
          * a RST.  Note that we do not send a RST reply to RSTs so that we do
          * not continually send RSTs between two endpoints.
          *
          * Note that since this is a reply, dst is src and src is dst.
          */
         if (VSOCK_SEND_RESET_BH(&dst, &src, pkt) < 0) {
            Log("unable to send reset.\n");
         }
         err = VMCI_ERROR_NOT_FOUND;
         goto out;
      }
   }

   /*
    * If the received packet type is beyond all types known to this
    * implementation, reply with an invalid message.  Hopefully this will help
    * when implementing backwards compatibility in the future.
    */
   if (pkt->type >= VSOCK_PACKET_TYPE_MAX) {
      VSOCK_SEND_INVALID_BH(&dst, &src);
      err = VMCI_ERROR_INVALID_ARGS;
      goto out;
   }

   /*
    * This handler is privileged when this module is running on the host.
    * We will get datagram connect requests from all endpoints (even VMs that
    * are in a restricted context). If we get one from a restricted context
    * then the destination socket must be trusted.
    *
    * NOTE: We access the socket struct without holding the lock here. This
    * is ok because the field we are interested is never modified outside
    * of the create and destruct socket functions.
    */
   vsk = vsock_sk(sk);
   if (!VSockVmciAllowDgram(vsk, VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src))) {
      err = VMCI_ERROR_NO_ACCESS;
      goto out;
   }

   /*
    * We do most everything in a work queue, but let's fast path the
    * notification of reads and writes to help data transfer performance.  We
    * can only do this if there is no process context code executing for this
    * socket since that may change the state.
    */
   bh_lock_sock(sk);

   if (!sock_owned_by_user(sk)) {
      /* The local context ID may be out of date. */
      vsk->localAddr.svm_cid = dst.svm_cid;

      if (sk->sk_state == SS_CONNECTED) {
         NOTIFYCALL(vsk, handleNotifyPkt, sk, pkt, TRUE, &dst, &src,
                    &bhProcessPkt);
      }
   }

   bh_unlock_sock(sk);

   if (!bhProcessPkt) {
      VSockRecvPktInfo *recvPktInfo;

      recvPktInfo = kmalloc(sizeof *recvPktInfo, GFP_ATOMIC);
      if (!recvPktInfo) {
         if (VSOCK_SEND_RESET_BH(&dst, &src, pkt) < 0) {
            Warning("unable to send reset\n");
         }
         err = VMCI_ERROR_NO_MEM;
         goto out;
      }

      recvPktInfo->sk = sk;
      memcpy(&recvPktInfo->pkt, pkt, sizeof recvPktInfo->pkt);
      COMPAT_INIT_WORK(&recvPktInfo->work, VSockVmciRecvPktWork, recvPktInfo);

      compat_schedule_work(&recvPktInfo->work);
      /*
       * Clear sk so that the reference count incremented by one of the Find
       * functions above is not decremented below.  We need that reference
       * count for the packet handler we've scheduled to run.
       */
      sk = NULL;
   }

out:
   if (sk) {
      sock_put(sk);
   }
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciPeerAttachCB --
 *
 *    Invoked when a peer attaches to a queue pair.
 *
 *    Right now this does not do anything.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify socket state and signal socket.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciPeerAttachCB(VMCIId subId,             // IN
                      VMCI_EventData *eData,    // IN
                      void *clientData)         // IN
{
   struct sock *sk;
   VMCIEventPayload_QP *ePayload;
   VSockVmciSock *vsk;

   ASSERT(eData);
   ASSERT(clientData);

   sk = (struct sock *)clientData;
   ePayload = VMCIEventDataPayload(eData);

   vsk = vsock_sk(sk);

   /*
    * We don't ask for delayed CBs when we subscribe to this event (we pass 0
    * as flags to VMCIEvent_Subscribe()).  VMCI makes no guarantees in that
    * case about what context we might be running in, so it could be BH or
    * process, blockable or non-blockable.  And bh_lock_sock() is very
    * particular about how it gets called (it's *not* the same as
    * spin_lock_bh(), it expands directly into a spin_lock()).  So we need to
    * account for all possible contexts here.
    */
   local_bh_disable();
   bh_lock_sock(sk);

   /*
    * XXX This is lame, we should provide a way to lookup sockets by qpHandle.
    */
   if (VMCI_HANDLE_EQUAL(vsk->qpHandle, ePayload->handle)) {
      /*
       * XXX This doesn't do anything, but in the future we may want to set
       * a flag here to verify the attach really did occur and we weren't just
       * sent a datagram claiming it was.
       */
      goto out;
   }

out:
   bh_unlock_sock(sk);
   local_bh_enable();
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciHandleDetach --
 *
 *      Perform the work necessary when the peer has detached.
 *
 *      Note that this assumes the socket lock is held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The socket's and its peer's shutdown mask will be set appropriately,
 *      and any callers waiting on this socket will be awoken.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciHandleDetach(struct sock *sk) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);
   if (!VMCI_HANDLE_INVALID(vsk->qpHandle)) {
      ASSERT(vsk->qpair);

      sock_set_flag(sk, SOCK_DONE);

      /* On a detach the peer will not be sending or receiving anymore. */
      vsk->peerShutdown = SHUTDOWN_MASK;

      /*
       * We should not be sending anymore since the peer won't be there to
       * receive, but we can still receive if there is data left in our consume
       * queue.
       */
      if (VSockVmciStreamHasData(vsk) <= 0) {
         if (sk->sk_state == SS_CONNECTING) {
            /*
             * The peer may detach from a queue pair while we are
             * still in the connecting state, i.e., if the peer VM is
             * killed after attaching to a queue pair, but before we
             * complete the handshake. In that case, we treat the
             * detach event like a reset.
             */

            sk->sk_state = SS_UNCONNECTED;
            sk->sk_err = ECONNRESET;
            sk->sk_error_report(sk);
            return;
         }
         sk->sk_state = SS_UNCONNECTED;
      }
      sk->sk_state_change(sk);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciPeerDetachCB --
 *
 *    Invoked when a peer detaches from a queue pair.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify socket state and signal socket.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciPeerDetachCB(VMCIId subId,             // IN
                      VMCI_EventData *eData,    // IN
                      void *clientData)         // IN
{
   struct sock *sk;
   VMCIEventPayload_QP *ePayload;
   VSockVmciSock *vsk;

   ASSERT(eData);
   ASSERT(clientData);

   sk = (struct sock *)clientData;
   ePayload = VMCIEventDataPayload(eData);
   vsk = vsock_sk(sk);
   if (VMCI_HANDLE_INVALID(ePayload->handle)) {
      return;
   }

   /* Same rules for locking as for PeerAttachCB(). */
   local_bh_disable();
   bh_lock_sock(sk);

   /*
    * XXX This is lame, we should provide a way to lookup sockets by qpHandle.
    */
   if (VMCI_HANDLE_EQUAL(vsk->qpHandle, ePayload->handle)) {
      VSockVmciHandleDetach(sk);
   }

   bh_unlock_sock(sk);
   local_bh_enable();
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciQPResumedCB --
 *
 *    Invoked when a VM is resumed.  We must mark all connected stream sockets
 *    as detached.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May modify socket state and signal socket.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciQPResumedCB(VMCIId subId,             // IN
                     VMCI_EventData *eData,    // IN
                     void *clientData)         // IN
{
   uint32 i;

   spin_lock_bh(&vsockTableLock);

   /*
    * XXX This loop should probably be provided by util.{h,c}, but that's for
    * another day.
    */
   for (i = 0; i < ARRAYSIZE(vsockConnectedTable); i++) {
      VSockVmciSock *vsk;

      list_for_each_entry(vsk, &vsockConnectedTable[i], connectedTable) {
         struct sock *sk = sk_vsock(vsk);

         /*
          * XXX Technically this is racy but the resulting outcome from such
          * a race is relatively harmless.  My next change will be a fix to
          * this.
          */
         VSockVmciHandleDetach(sk);
      }
   }

   spin_unlock_bh(&vsockTableLock);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciPendingWork --
 *
 *    Releases the resources for a pending socket if it has not reached the
 *    connected state and been accepted by a user process.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The socket may be removed from the connected list and all its resources
 *    freed.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciPendingWork(compat_delayed_work_arg work)    // IN
{
   struct sock *sk;
   struct sock *listener;
   VSockVmciSock *vsk;
   Bool cleanup;

   vsk = COMPAT_DELAYED_WORK_GET_DATA(work, VSockVmciSock, dwork);
   ASSERT(vsk);

   sk = sk_vsock(vsk);
   listener = vsk->listener;
   cleanup = TRUE;

   ASSERT(listener);

   lock_sock(listener);
   lock_sock(sk);

   /*
    * The socket should be on the pending list or the accept queue, but not
    * both.  It's also possible that the socket isn't on either.
    */
   ASSERT(    ( VSockVmciIsPending(sk) && !VSockVmciInAcceptQueue(sk))
           || (!VSockVmciIsPending(sk) &&  VSockVmciInAcceptQueue(sk))
           || (!VSockVmciIsPending(sk) && !VSockVmciInAcceptQueue(sk)));

   if (VSockVmciIsPending(sk)) {
      VSockVmciRemovePending(listener, sk);
   } else if (!vsk->rejected) {
      /*
       * We are not on the pending list and accept() did not reject us, so we
       * must have been accepted by our user process.  We just need to drop our
       * references to the sockets and be on our way.
       */
      cleanup = FALSE;
      goto out;
   }

   listener->sk_ack_backlog--;

   /*
    * We need to remove ourself from the global connected sockets list so
    * incoming packets can't find this socket, and to reduce the reference
    * count.
    */
   if (VSockVmciInConnectedTable(sk)) {
      VSockVmciRemoveConnected(sk);
   }

   sk->sk_state = SS_FREE;

out:
   release_sock(sk);
   release_sock(listener);
   if (cleanup) {
      sock_put(sk);
   }
   sock_put(sk);
   sock_put(listener);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvPktWork --
 *
 *    Handles an incoming control packet for the provided socket.  This is the
 *    state machine for our stream sockets.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May set state and wakeup threads waiting for socket state to change.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciRecvPktWork(compat_work_arg work)  // IN
{
   VSockRecvPktInfo *recvPktInfo;
   VSockPacket *pkt;
   struct sock *sk;

   recvPktInfo = COMPAT_WORK_GET_DATA(work, VSockRecvPktInfo, work);
   ASSERT(recvPktInfo);

   sk = recvPktInfo->sk;
   pkt = &recvPktInfo->pkt;

   ASSERT(pkt);
   ASSERT(pkt->type < VSOCK_PACKET_TYPE_MAX);

   lock_sock(sk);

   /* The local context ID may be out of date. */
   vsock_sk(sk)->localAddr.svm_cid = VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst);

   switch (sk->sk_state) {
   case SS_LISTEN:
      VSockVmciRecvListen(sk, pkt);
      break;
   case SS_CONNECTING:
      /*
       * Processing of pending connections for servers goes through the
       * listening socket, so see VSockVmciRecvListen() for that path.
       */
      VSockVmciRecvConnectingClient(sk, pkt);
      break;
   case SS_CONNECTED:
      VSockVmciRecvConnected(sk, pkt);
      break;
   default:
      /*
       * Because this function does not run in the same context as
       * VSockVmciRecvStreamCB it is possible that the socket
       * has closed. We need to let the other side know or it could
       * be sitting in a connect and hang forever. Send a reset to prevent
       * that.
       */
      VSOCK_SEND_RESET(sk, pkt);
      goto out;
   }

out:
   release_sock(sk);
   kfree(recvPktInfo);
   /*
    * Release reference obtained in the stream callback when we fetched this
    * socket out of the bound or connected list.
    */
   sock_put(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvListen --
 *
 *    Receives packets for sockets in the listen state.
 *
 *    Note that this assumes the socket lock is held.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    A new socket may be created and a negotiate control packet is sent.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvListen(struct sock *sk,   // IN
                    VSockPacket *pkt)  // IN
{
   struct sock *pending;
   VSockVmciSock *vpending;
   int err;
   uint64 qpSize;
   Bool oldRequest = FALSE;
   Bool oldPktProto = FALSE;

   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(sk->sk_state == SS_LISTEN);

   err = 0;

   /*
    * Because we are in the listen state, we could be receiving a packet for
    * ourself or any previous connection requests that we received.  If it's
    * the latter, we try to find a socket in our list of pending connections
    * and, if we do, call the appropriate handler for the state that that
    * socket is in.  Otherwise we try to service the connection request.
    */
   pending = VSockVmciGetPending(sk, pkt);
   if (pending) {
      lock_sock(pending);

      /* The local context ID may be out of date. */
      vsock_sk(pending)->localAddr.svm_cid =
         VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst);

      switch (pending->sk_state) {
      case SS_CONNECTING:
         err = VSockVmciRecvConnectingServer(sk, pending, pkt);
         break;
      default:
         VSOCK_SEND_RESET(pending, pkt);
         err = -EINVAL;
      }

      if (err < 0) {
         VSockVmciRemovePending(sk, pending);
      }

      release_sock(pending);
      VSockVmciReleasePending(pending);

      return err;
   }

   /*
    * The listen state only accepts connection requests.  Reply with a reset
    * unless we received a reset.
    */

   if (!(pkt->type == VSOCK_PACKET_TYPE_REQUEST ||
         pkt->type == VSOCK_PACKET_TYPE_REQUEST2)) {
       VSOCK_REPLY_RESET(pkt);
       return -EINVAL;
   }

   if (pkt->u.size == 0) {
       VSOCK_REPLY_RESET(pkt);
       return -EINVAL;
   }

   /*
    * If this socket can't accommodate this connection request, we send
    * a reset.  Otherwise we create and initialize a child socket and reply
    * with a connection negotiation.
    */
   if (sk->sk_ack_backlog >= sk->sk_max_ack_backlog) {
      VSOCK_REPLY_RESET(pkt);
      return -ECONNREFUSED;
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   pending = __VSockVmciCreate(NULL, sk, GFP_KERNEL, sk->sk_type);
#else
   pending = __VSockVmciCreate(compat_sock_net(sk), NULL, sk, GFP_KERNEL,
                               sk->sk_type);
#endif
   if (!pending) {
      VSOCK_SEND_RESET(sk, pkt);
      return -ENOMEM;
   }

   vpending = vsock_sk(pending);
   ASSERT(vpending);
   ASSERT(vsock_sk(sk)->localAddr.svm_port == pkt->dstPort);

   VSockAddr_Init(&vpending->localAddr,
                  VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst),
                  pkt->dstPort);
   VSockAddr_Init(&vpending->remoteAddr,
                  VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                  pkt->srcPort);

   /*
    * If the proposed size fits within our min/max, accept
    * it. Otherwise propose our own size.
    */
   if (pkt->u.size >= vpending->queuePairMinSize &&
       pkt->u.size <= vpending->queuePairMaxSize) {
      qpSize = pkt->u.size;
   } else {
      qpSize = vpending->queuePairSize;
   }

   /*
    * Figure out if we are using old or new requests based on the overrides
    * pkt types sent by our peer.
    */
   if (VSockVmciOldProtoOverride(&oldPktProto)) {
      oldRequest = oldPktProto;
   } else {
      if (pkt->type == VSOCK_PACKET_TYPE_REQUEST) {
         oldRequest = TRUE;
      } else if (pkt->type == VSOCK_PACKET_TYPE_REQUEST2) {
         oldRequest = FALSE;
      }
   }

   if (oldRequest) {
      /* Handle a REQUEST (or override) */
      VSockProtoVersion version = VSOCK_PROTO_INVALID;
      if (VSockVmciProtoToNotifyStruct(pending, &version, TRUE)) {
         err = VSOCK_SEND_NEGOTIATE(pending, qpSize);
      } else {
         err = -EINVAL;
      }
   } else {
      /* Handle a REQUEST2 (or override) */
      int protoInt = pkt->proto;
      int pos;
      uint16 activeProtoVersion = 0;

      /*
       * The list of possible protocols is the intersection of all protocols
       * the client supports ... plus all the protocols we support.
       */
      protoInt &= VSockVmciNewProtoSupportedVersions();

      /* We choose the highest possible protocol version and use that one. */
      pos = mssb32(protoInt);
      if (pos) {
         activeProtoVersion = (1 << (pos - 1));
         if (VSockVmciProtoToNotifyStruct(pending, &activeProtoVersion, FALSE)) {
            err = VSOCK_SEND_NEGOTIATE2(pending, qpSize,
                                        activeProtoVersion);
         } else {
            err = -EINVAL;
         }
      } else {
         err = -EINVAL;
      }
   }

   if (err < 0) {
      VSOCK_SEND_RESET(sk, pkt);
      sock_put(pending);
      err = VSockVmci_ErrorToVSockError(err);
      goto out;
   }

   VSockVmciAddPending(sk, pending);
   sk->sk_ack_backlog++;

   pending->sk_state = SS_CONNECTING;
   vpending->produceSize = vpending->consumeSize = qpSize;
   vpending->queuePairSize = qpSize;

   NOTIFYCALL(vpending, processRequest, pending);

   /*
    * We might never receive another message for this socket and it's not
    * connected to any process, so we have to ensure it gets cleaned up
    * ourself.  Our delayed work function will take care of that.  Note that we
    * do not ever cancel this function since we have few guarantees about its
    * state when calling cancel_delayed_work().  Instead we hold a reference on
    * the socket for that function and make it capable of handling cases where
    * it needs to do nothing but release that reference.
    */
   vpending->listener = sk;
   sock_hold(sk);
   sock_hold(pending);
   COMPAT_INIT_DELAYED_WORK(&vpending->dwork, VSockVmciPendingWork, vpending);
   compat_schedule_delayed_work(&vpending->dwork, HZ);

out:
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvConnectingServer --
 *
 *    Receives packets for sockets in the connecting state on the server side.
 *
 *    Connecting sockets on the server side can only receive queue pair offer
 *    packets.  All others should be treated as cause for closing the
 *    connection.
 *
 *    Note that this assumes the socket lock is held for both sk and pending.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    A queue pair may be created, an attach control packet may be sent, the
 *    socket may transition to the connected state, and a pending caller in
 *    accept() may be woken up.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvConnectingServer(struct sock *listener, // IN: the listening socket
                              struct sock *pending,  // IN: the pending connection
                              VSockPacket *pkt)      // IN: current packet
{
   VSockVmciSock *vpending;
   VMCIHandle handle;
   VMCIQPair *qpair;
   Bool isLocal;
   uint32 flags;
   VMCIId detachSubId;
   int err;
   int skerr;

   ASSERT(listener);
   ASSERT(pkt);
   ASSERT(listener->sk_state == SS_LISTEN);
   ASSERT(pending->sk_state == SS_CONNECTING);

   vpending = vsock_sk(pending);
   detachSubId = VMCI_INVALID_ID;

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_OFFER:
      if (VMCI_HANDLE_INVALID(pkt->u.handle)) {
         VSOCK_SEND_RESET(pending, pkt);
         skerr = EPROTO;
         err = -EINVAL;
         goto destroy;
      }
      break;
   default:
      /* Close and cleanup the connection. */
      VSOCK_SEND_RESET(pending, pkt);
      skerr = EPROTO;
      err =  pkt->type == VSOCK_PACKET_TYPE_RST ?
                0 :
                -EINVAL;
      goto destroy;
   }

   ASSERT(pkt->type == VSOCK_PACKET_TYPE_OFFER);

   /*
    * In order to complete the connection we need to attach to the offered
    * queue pair and send an attach notification.  We also subscribe to the
    * detach event so we know when our peer goes away, and we do that before
    * attaching so we don't miss an event.  If all this succeeds, we update our
    * state and wakeup anything waiting in accept() for a connection.
    */

   /*
    * We don't care about attach since we ensure the other side has attached by
    * specifying the ATTACH_ONLY flag below.
    */
   err = vmci_event_subscribe(VMCI_EVENT_QP_PEER_DETACH,
                              VSockVmciPeerDetachCB,
                              pending,
                              &detachSubId);
   if (err < VMCI_SUCCESS) {
      VSOCK_SEND_RESET(pending, pkt);
      err = VSockVmci_ErrorToVSockError(err);
      skerr = -err;
      goto destroy;
   }

   vpending->detachSubId = detachSubId;

   /* Now attach to the queue pair the client created. */
   handle = pkt->u.handle;

   /*
    * vpending->localAddr always has a context id so we do not
    * need to worry about VMADDR_CID_ANY in this case.
    */
   isLocal = vpending->remoteAddr.svm_cid == vpending->localAddr.svm_cid;
   flags = VMCI_QPFLAG_ATTACH_ONLY;
   flags |= isLocal ? VMCI_QPFLAG_LOCAL : 0;

   err = VSockVmciQueuePairAlloc(&qpair,
                                 &handle,
                                 vpending->produceSize,
                                 vpending->consumeSize,
                                 VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                                 flags,
                                 VSockVmciTrusted(vpending,
                                                  vpending->remoteAddr.svm_cid));
   if (err < 0) {
      VSOCK_SEND_RESET(pending, pkt);
      skerr = -err;
      goto destroy;
   }

   ASSERT(VMCI_HANDLE_EQUAL(handle, pkt->u.handle));
   vpending->qpHandle = handle;
   vpending->qpair = qpair;

   /*
    * When we send the attach message, we must be ready to handle
    * incoming control messages on the newly connected socket. So we
    * move the pending socket to the connected state before sending
    * the attach message. Otherwise, an incoming packet triggered by
    * the attach being received by the peer may be processed
    * concurrently with what happens below after sending the attach
    * message, and that incoming packet will find the listening socket
    * instead of the (currently) pending socket. Note that enqueueing
    * the socket increments the reference count, so even if a reset
    * comes before the connection is accepted, the socket will be
    * valid until it is removed from the queue.
    *
    * If we fail sending the attach below, we remove the socket from
    * the connected list and move the socket to SS_UNCONNECTED before
    * releasing the lock, so a pending slow path processing of an
    * incoming packet will not see the socket in the connected state
    * in that case.
    */
   pending->sk_state = SS_CONNECTED;

   VSockVmciInsertConnected(vsockConnectedSocketsVsk(vpending), pending);

   /* Notify our peer of our attach. */
   err = VSOCK_SEND_ATTACH(pending, handle);
   if (err < 0) {
      VSockVmciRemoveConnected(pending);
      Log("Could not send attach\n");
      VSOCK_SEND_RESET(pending, pkt);
      err = VSockVmci_ErrorToVSockError(err);
      skerr = -err;
      goto destroy;
   }

   /*
    * We have a connection. Move the now connected socket from the
    * listener's pending list to the accept queue so callers of
    * accept() can find it.
    */
   VSockVmciRemovePending(listener, pending);
   VSockVmciEnqueueAccept(listener, pending);

   /*
    * Callers of accept() will be be waiting on the listening socket, not the
    * pending socket.
    */
   listener->sk_state_change(listener);

   return 0;

destroy:
   pending->sk_err = skerr;
   pending->sk_state = SS_UNCONNECTED;
   /*
    * As long as we drop our reference, all necessary cleanup will handle when
    * the cleanup function drops its reference and our destruct implementation
    * is called.  Note that since the listen handler will remove pending from
    * the pending list upon our failure, the cleanup function won't drop the
    * additional reference, which is why we do it here.
    */
   sock_put(pending);

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvConnectingClient --
 *
 *    Receives packets for sockets in the connecting state on the client side.
 *
 *    Connecting sockets on the client side should only receive attach packets.
 *    All others should be treated as cause for closing the connection.
 *
 *    Note that this assumes the socket lock is held for both sk and pending.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    The socket may transition to the connected state and wakeup the pending
 *    caller of connect().
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvConnectingClient(struct sock *sk,       // IN: socket
                              VSockPacket *pkt)      // IN: current packet
{
   VSockVmciSock *vsk;
   int err;
   int skerr;

   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(sk->sk_state == SS_CONNECTING);

   vsk = vsock_sk(sk);

   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_ATTACH:
      if (VMCI_HANDLE_INVALID(pkt->u.handle) ||
          !VMCI_HANDLE_EQUAL(pkt->u.handle, vsk->qpHandle)) {
         skerr = EPROTO;
         err = -EINVAL;
         goto destroy;
      }

      /*
       * Signify the socket is connected and wakeup the waiter in connect().
       * Also place the socket in the connected table for accounting (it can
       * already be found since it's in the bound table).
       */
      sk->sk_state = SS_CONNECTED;
      sk->sk_socket->state = SS_CONNECTED;
      VSockVmciInsertConnected(vsockConnectedSocketsVsk(vsk), sk);
      sk->sk_state_change(sk);

      break;
   case VSOCK_PACKET_TYPE_NEGOTIATE:
   case VSOCK_PACKET_TYPE_NEGOTIATE2:
      if (pkt->u.size == 0 ||
          VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src) != vsk->remoteAddr.svm_cid ||
          pkt->srcPort != vsk->remoteAddr.svm_port ||
          !VMCI_HANDLE_INVALID(vsk->qpHandle) ||
          vsk->qpair ||
          vsk->produceSize != 0 ||
          vsk->consumeSize != 0 ||
          vsk->attachSubId != VMCI_INVALID_ID ||
          vsk->detachSubId != VMCI_INVALID_ID) {
         skerr = EPROTO;
         err = -EINVAL;

         goto destroy;
      }

      err = VSockVmciRecvConnectingClientNegotiate(sk, pkt);
      if (err) {
         skerr = -err;
         goto destroy;
      }

      break;
   case VSOCK_PACKET_TYPE_INVALID:
      err = VSockVmciRecvConnectingClientInvalid(sk, pkt);
      if (err) {
         skerr = -err;
         goto destroy;
      }

      break;
   case VSOCK_PACKET_TYPE_RST:
      /*
       * Older versions of the linux code (WS 6.5 / ESX 4.0) used to continue
       * processing here after they sent an INVALID packet. This meant that we
       * got a RST after the INVALID. We ignore a RST after an INVALID. The
       * common code doesn't send the RST ... so we can hang if an old version
       * of the common code fails between getting a REQUEST and sending an
       * OFFER back. Not much we can do about it... except hope that it
       * doesn't happen.
       */
      if (vsk->ignoreConnectingRst) {
         vsk->ignoreConnectingRst = FALSE;
      } else {
         skerr = ECONNRESET;
         err = 0;
         goto destroy;
      }

      break;
   default:
      /* Close and cleanup the connection. */
      skerr = EPROTO;
      err = -EINVAL;
      goto destroy;
   }

   ASSERT(pkt->type == VSOCK_PACKET_TYPE_ATTACH ||
          pkt->type == VSOCK_PACKET_TYPE_NEGOTIATE ||
          pkt->type == VSOCK_PACKET_TYPE_NEGOTIATE2 ||
          pkt->type == VSOCK_PACKET_TYPE_INVALID ||
          pkt->type == VSOCK_PACKET_TYPE_RST);

   return 0;

destroy:
   VSOCK_SEND_RESET(sk, pkt);

   sk->sk_state = SS_UNCONNECTED;
   sk->sk_err = skerr;
   sk->sk_error_report(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvConnectingClientNegotiate --
 *
 *    Handles a negotiate packet for a client in the connecting state.
 *
 *    Note that this assumes the socket lock is held for both sk and pending.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    The socket may transition to the connected state and wakeup the pending
 *    caller of connect().
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvConnectingClientNegotiate(struct sock *sk,   // IN: socket
                                       VSockPacket *pkt)  // IN: current packet
{
   int err;
   VSockVmciSock *vsk;
   VMCIHandle handle;
   VMCIQPair *qpair;
   VMCIId attachSubId;
   VMCIId detachSubId;
   Bool isLocal;
   uint32 flags;
   Bool oldProto = TRUE;
   Bool oldPktProto;
   VSockProtoVersion version;

   vsk = vsock_sk(sk);
   handle = VMCI_INVALID_HANDLE;
   attachSubId = VMCI_INVALID_ID;
   detachSubId = VMCI_INVALID_ID;

   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(pkt->u.size > 0);
   ASSERT(vsk->remoteAddr.svm_cid == VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src));
   ASSERT(vsk->remoteAddr.svm_port == pkt->srcPort);
   ASSERT(VMCI_HANDLE_INVALID(vsk->qpHandle));
   ASSERT(vsk->qpair == NULL);
   ASSERT(vsk->produceSize == 0);
   ASSERT(vsk->consumeSize == 0);
   ASSERT(vsk->attachSubId == VMCI_INVALID_ID);
   ASSERT(vsk->detachSubId == VMCI_INVALID_ID);

   /*
    * If we have gotten here then we should be past the point where old linux
    * vsock could have sent the bogus rst.
    */
   vsk->sentRequest = FALSE;
   vsk->ignoreConnectingRst = FALSE;

   /* Verify that we're OK with the proposed queue pair size */
   if (pkt->u.size < vsk->queuePairMinSize ||
       pkt->u.size > vsk->queuePairMaxSize) {
      err = -EINVAL;
      goto destroy;
   }

   /*
    * At this point we know the CID the peer is using to talk to us.
    */

   if (vsk->localAddr.svm_cid == VMADDR_CID_ANY) {
      vsk->localAddr.svm_cid = VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.dst);
   }

   /*
    * Setup the notify ops to be the highest supported version that both the
    * server and the client support.
    */

   if (VSockVmciOldProtoOverride(&oldPktProto)) {
      oldProto = oldPktProto;
   } else {
      if (pkt->type == VSOCK_PACKET_TYPE_NEGOTIATE) {
         oldProto = TRUE;
      } else if (pkt->type == VSOCK_PACKET_TYPE_NEGOTIATE2) {
         oldProto = FALSE;
      }
   }

   if (oldProto) {
      version = VSOCK_PROTO_INVALID;
   } else {
      version = pkt->proto;
   }

   if (!VSockVmciProtoToNotifyStruct(sk, &version, oldProto)) {
      err = -EINVAL;
      goto destroy;
   }

   /*
    * Subscribe to attach and detach events first.
    *
    * XXX We attach once for each queue pair created for now so it is easy
    * to find the socket (it's provided), but later we should only subscribe
    * once and add a way to lookup sockets by queue pair handle.
    */
   err = vmci_event_subscribe(VMCI_EVENT_QP_PEER_ATTACH,
                              VSockVmciPeerAttachCB,
                              sk,
                              &attachSubId);
   if (err < VMCI_SUCCESS) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   err = vmci_event_subscribe(VMCI_EVENT_QP_PEER_DETACH,
                              VSockVmciPeerDetachCB,
                              sk,
                              &detachSubId);
   if (err < VMCI_SUCCESS) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   /* Make VMCI select the handle for us. */
   handle = VMCI_INVALID_HANDLE;
   isLocal = vsk->remoteAddr.svm_cid == vsk->localAddr.svm_cid;
   flags = isLocal ? VMCI_QPFLAG_LOCAL : 0;

   err = VSockVmciQueuePairAlloc(&qpair,
                                 &handle,
                                 pkt->u.size,
                                 pkt->u.size,
                                 vsk->remoteAddr.svm_cid,
                                 flags,
                                 VSockVmciTrusted(vsk, vsk->remoteAddr.svm_cid));
   if (err < 0) {
      goto destroy;
   }

   err = VSOCK_SEND_QP_OFFER(sk, handle);
   if (err < 0) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   vsk->qpHandle = handle;
   vsk->qpair = qpair;

   vsk->produceSize = vsk->consumeSize = pkt->u.size;

   vsk->attachSubId = attachSubId;
   vsk->detachSubId = detachSubId;

   NOTIFYCALL(vsk, processNegotiate, sk);

   return 0;

destroy:
   if (attachSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(attachSubId);
      ASSERT(vsk->attachSubId == VMCI_INVALID_ID);
   }

   if (detachSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(detachSubId);
      ASSERT(vsk->detachSubId == VMCI_INVALID_ID);
   }

   if (!VMCI_HANDLE_INVALID(handle)) {
      ASSERT(vsk->qpair);
      vmci_qpair_detach(&qpair);
      ASSERT(VMCI_HANDLE_INVALID(vsk->qpHandle));
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvConnectingClientInvalid --
 *
 *    Handles an invalid packet for a client in the connecting state.
 *
 *    Note that this assumes the socket lock is held for both sk and pending.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvConnectingClientInvalid(struct sock *sk,   // IN: socket
                                     VSockPacket *pkt)  // IN: current packet
{
   int err = 0;
   VSockVmciSock *vsk;

   ASSERT(sk);
   ASSERT(pkt);

   vsk = vsock_sk(sk);

   if (vsk->sentRequest) {
      vsk->sentRequest = FALSE;
      vsk->ignoreConnectingRst = TRUE;

      err = VSOCK_SEND_CONN_REQUEST(sk, vsk->queuePairSize);
      if (err < 0) {
         err = VSockVmci_ErrorToVSockError(err);
      } else {
         err = 0;
      }
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRecvConnected --
 *
 *    Receives packets for sockets in the connected state.
 *
 *    Connected sockets should only ever receive detach, wrote, read, or reset
 *    control messages.  Others are treated as errors that are ignored.
 *
 *    Wrote and read signify that the peer has produced or consumed,
 *    respectively.
 *
 *    Detach messages signify that the connection is being closed cleanly and
 *    reset messages signify that the connection is being closed in error.
 *
 *    Note that this assumes the socket lock is held.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    A queue pair may be created, an offer control packet sent, and the socket
 *    may transition to the connecting state.
 *
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRecvConnected(struct sock *sk,      // IN
                       VSockPacket *pkt)     // IN
{
   VSockVmciSock *vsk;
   Bool pktProcessed = FALSE;

   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(sk->sk_state == SS_CONNECTED);

   /*
    * In cases where we are closing the connection, it's sufficient to mark
    * the state change (and maybe error) and wake up any waiting threads.
    * Since this is a connected socket, it's owned by a user process and will
    * be cleaned up when the failure is passed back on the current or next
    * system call.  Our system call implementations must therefore check for
    * error and state changes on entry and when being awoken.
    */
   switch (pkt->type) {
   case VSOCK_PACKET_TYPE_SHUTDOWN:
      if (pkt->u.mode) {
         vsk = vsock_sk(sk);

         vsk->peerShutdown |= pkt->u.mode;
         sk->sk_state_change(sk);
      }
      break;

   case VSOCK_PACKET_TYPE_RST:
      vsk = vsock_sk(sk);
      /*
       * It is possible that we sent our peer a message (e.g
       * a WAITING_READ) right before we got notified that the peer
       * had detached. If that happens then we can get a RST pkt back
       * from our peer even though there is data available for us
       * to read. In that case, don't shutdown the socket completely
       * but instead allow the local client to finish reading data
       * off the queuepair. Always treat a RST pkt in connected mode
       * like a clean shutdown.
       */
      sock_set_flag(sk, SOCK_DONE);
      vsk->peerShutdown = SHUTDOWN_MASK;
      if (VSockVmciStreamHasData(vsk) <= 0) {
         sk->sk_state = SS_DISCONNECTING;
      }
      sk->sk_state_change(sk);
      break;

   default:
      vsk = vsock_sk(sk);
      NOTIFYCALL(vsk, handleNotifyPkt, sk, pkt, FALSE, NULL, NULL,
                 &pktProcessed);
      if (!pktProcessed) {
         return -EINVAL;
      }
      break;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciSendControlPkt --
 *
 *    Common code to send a control packet.
 *
 * Results:
 *    Size of datagram sent on success, negative error code otherwise.
 *    If convertError is TRUE, error code is a vsock error, otherwise,
 *    result is a VMCI error code.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
__VSockVmciSendControlPkt(VSockPacket *pkt,           // IN
                          struct sockaddr_vm *src,    // IN
                          struct sockaddr_vm *dst,    // IN
                          VSockPacketType type,       // IN
                          uint64 size,                // IN
                          uint64 mode,                // IN
                          VSockWaitingInfo *wait,     // IN
                          VSockProtoVersion proto,    // IN
                          VMCIHandle handle,          // IN
                          Bool convertError)          // IN
{
   int err;

   ASSERT(pkt);
   /*
    * This function can be called in different contexts, so family value is not
    * necessarily consistent.
    */

   VSOCK_ADDR_NOFAMILY_ASSERT(src);
   VSOCK_ADDR_NOFAMILY_ASSERT(dst);

   VSockPacket_Init(pkt, src, dst, type, size, mode, wait, proto, handle);
   LOG_PACKET(pkt);
   VSOCK_STATS_CTLPKT_LOG(pkt->type);
   err = vmci_datagram_send(&pkt->dg);
   if (convertError && (err < 0)) {
      return VSockVmci_ErrorToVSockError(err);
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciReplyControlPktFast --
 *
 *    Sends a control packet back to the source of an incoming packet.
 *    The control packet is allocated in the stack.
 *
 * Results:
 *    Size of datagram sent on success, negative error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciReplyControlPktFast(VSockPacket *pkt,       // IN
                             VSockPacketType type,   // IN
                             uint64 size,            // IN
                             uint64 mode,            // IN
                             VSockWaitingInfo *wait, // IN
                             VMCIHandle handle)      // IN
{
   VSockPacket reply;
   struct sockaddr_vm src, dst;

   ASSERT(pkt);

   if (pkt->type == VSOCK_PACKET_TYPE_RST) {
      return 0;
   } else {
      VSockPacket_GetAddresses(pkt, &src, &dst);
      return __VSockVmciSendControlPkt(&reply, &src, &dst, type,
                                       size, mode, wait,
                                       VSOCK_PROTO_INVALID, handle, TRUE);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendControlPktBH --
 *
 *    Sends a control packet from bottom-half context. The control packet is
 *    static data to minimize the resource cost.
 *
 * Results:
 *    Size of datagram sent on success, negative error code otherwise.  Note
 *    that we return a VMCI error message since that's what callers will need
 *    to provide.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciSendControlPktBH(struct sockaddr_vm *src,      // IN
                          struct sockaddr_vm *dst,      // IN
                          VSockPacketType type,         // IN
                          uint64 size,                  // IN
                          uint64 mode,                  // IN
                          VSockWaitingInfo *wait,       // IN
                          VMCIHandle handle)            // IN
{
   /*
    * Note that it is safe to use a single packet across all CPUs since two
    * tasklets of the same type are guaranteed to not ever run simultaneously.
    * If that ever changes, or VMCI stops using tasklets, we can use per-cpu
    * packets.
    */
   static VSockPacket pkt;

   return __VSockVmciSendControlPkt(&pkt, src, dst, type,
                                    size, mode, wait, VSOCK_PROTO_INVALID,
                                    handle, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendControlPkt --
 *
 *      Sends a control packet.
 *
 * Results:
 *      Size of datagram sent on success, negative error on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciSendControlPkt(struct sock *sk,         // IN
                        VSockPacketType type,    // IN
                        uint64 size,             // IN
                        uint64 mode,             // IN
                        VSockWaitingInfo *wait,  // IN
                        VSockProtoVersion proto, // IN
                        VMCIHandle handle)       // IN
{
   VSockPacket *pkt;
   VSockVmciSock *vsk;
   int err;

   ASSERT(sk);
   /*
    * New sockets for connection establishment won't have socket structures
    * yet; if one exists, ensure it is of the proper type.
    */
   ASSERT(sk->sk_socket ?
             sk->sk_socket->type == SOCK_STREAM :
             1);

   vsk = vsock_sk(sk);

   if (!VSockAddr_Bound(&vsk->localAddr)) {
      return -EINVAL;
   }

   if (!VSockAddr_Bound(&vsk->remoteAddr)) {
      return -EINVAL;
   }

   pkt = kmalloc(sizeof *pkt, GFP_KERNEL);
   if (!pkt) {
      return -ENOMEM;
   }

   err = __VSockVmciSendControlPkt(pkt, &vsk->localAddr, &vsk->remoteAddr,
                                   type, size, mode, wait, proto, handle,
                                   TRUE);
   kfree(pkt);

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciBind --
 *
 *    Common functionality needed to bind the specified address to the
 *    VSocket.  If VMADDR_CID_ANY or VMADDR_PORT_ANY are specified, the context
 *    ID or port are selected automatically.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    On success, a new datagram handle is created.
 *
 *----------------------------------------------------------------------------
 */

static int
__VSockVmciBind(struct sock *sk,          // IN/OUT
                struct sockaddr_vm *addr) // IN
{
   static unsigned int port = LAST_RESERVED_PORT + 1;
   struct sockaddr_vm newAddr;
   VSockVmciSock *vsk;
   VMCIId cid;
   int err;

   ASSERT(sk);
   ASSERT(sk->sk_socket);
   ASSERT(addr);

   vsk = vsock_sk(sk);

   /* First ensure this socket isn't already bound. */
   if (VSockAddr_Bound(&vsk->localAddr)) {
      return -EINVAL;
   }

   /*
    * Now bind to the provided address or select appropriate values if none are
    * provided (VMADDR_CID_ANY and VMADDR_PORT_ANY).  Note that like AF_INET
    * prevents binding to a non-local IP address (in most cases), we only allow
    * binding to the local CID.
    */
   VSockAddr_Init(&newAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);

   cid = vmci_get_context_id();
   if (addr->svm_cid != cid &&
       addr->svm_cid != VMADDR_CID_ANY) {
      return -EADDRNOTAVAIL;
   }

   newAddr.svm_cid = addr->svm_cid;

   switch (sk->sk_socket->type) {
   case SOCK_STREAM: {
      spin_lock_bh(&vsockTableLock);

      if (addr->svm_port == VMADDR_PORT_ANY) {
         Bool found = FALSE;
         unsigned int i;

         for (i = 0; i < MAX_PORT_RETRIES; i++) {
            if (port <= LAST_RESERVED_PORT) {
               port = LAST_RESERVED_PORT + 1;
            }

            newAddr.svm_port = port++;

            if (!__VSockVmciFindBoundSocket(&newAddr)) {
               found = TRUE;
               break;
            }
         }

         if (!found) {
            err = -EADDRNOTAVAIL;
            goto out;
         }
      } else {
         /* If port is in reserved range, ensure caller has necessary privileges. */
         if (addr->svm_port <= LAST_RESERVED_PORT &&
             !capable(CAP_NET_BIND_SERVICE)) {
            err = -EACCES;
            goto out;
         }

         newAddr.svm_port = addr->svm_port;
         if (__VSockVmciFindBoundSocket(&newAddr)) {
            err = -EADDRINUSE;
            goto out;
         }

      }
      break;
   }
   case SOCK_DGRAM: {
      uint32 flags = 0;

      /* VMCI will select a resource ID for us if we provide VMCI_INVALID_ID. */
      newAddr.svm_port = addr->svm_port == VMADDR_PORT_ANY ?
                            VMCI_INVALID_ID :
                            addr->svm_port;

      if (newAddr.svm_port <= LAST_RESERVED_PORT &&
          !capable(CAP_NET_BIND_SERVICE)) {
         err = -EACCES;
         goto out;
      }

      if (newAddr.svm_cid == VMADDR_CID_ANY) {
         flags = VMCI_FLAG_ANYCID_DG_HND;
      }

      err = VSockVmciDatagramCreateHnd(newAddr.svm_port, flags,
                                       VSockVmciRecvDgramCB,
                                       sk, &vsk->dgHandle);
      if (err < VMCI_SUCCESS) {
         err = VSockVmci_ErrorToVSockError(err);
         goto out;
      }

      newAddr.svm_port = VMCI_HANDLE_TO_RESOURCE_ID(vsk->dgHandle);
      break;
   }
   default: {
      err = -EINVAL;
      goto out;
   }
   }
   /*
    * VSockVmci_GetAFValue() acquires a mutex and may sleep, so fill the
    * field after unlocking socket tables.
    */
   VSockAddr_InitNoFamily(&vsk->localAddr, newAddr.svm_cid, newAddr.svm_port);

   /*
    * Remove stream sockets from the unbound list and add them to the hash
    * table for easy lookup by its address.  The unbound list is simply an
    * extra entry at the end of the hash table, a trick used by AF_UNIX.
    */
   if (sk->sk_socket->type == SOCK_STREAM) {
      __VSockVmciRemoveBound(sk);
      __VSockVmciInsertBound(vsockBoundSockets(&vsk->localAddr), sk);
      spin_unlock_bh(&vsockTableLock);
   }
   vsk->localAddr.svm_family = VSockVmci_GetAFValue();
   VSOCK_ADDR_ASSERT(&vsk->localAddr);

   return 0;

out:
   if (sk->sk_socket->type == SOCK_STREAM) {
      spin_unlock_bh(&vsockTableLock);
   }
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciCreate --
 *
 *    Does the work to create the sock structure.
 *    Note: If sock is NULL then the type field must be non-zero.
 *          Otherwise, sock is non-NULL and the type of sock is used in the
 *          newly created socket.
 *
 * Results:
 *    sock structure on success, NULL on failure.
 *
 * Side effects:
 *    Allocated sk is added to the unbound sockets list iff it is owned by
 *    a struct socket.
 *
 *----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
static struct sock *
__VSockVmciCreate(struct socket *sock,   // IN: Owning socket, may be NULL
                  struct sock *parent,   // IN: Parent socket, may be NULL
                  unsigned int priority, // IN: Allocation flags
                  unsigned short type)   // IN: Socket type if sock is NULL
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static struct sock *
__VSockVmciCreate(struct socket *sock,   // IN: Owning socket, may be NULL
                  struct sock *parent,   // IN: Parent socket, may be NULL
                  gfp_t priority,        // IN: Allocation flags
                  unsigned short type)   // IN: Socket type if sock is NULL
#else
static struct sock *
__VSockVmciCreate(struct net *net,       // IN: Network namespace
                  struct socket *sock,   // IN: Owning socket, may be NULL
                  struct sock *parent,   // IN: Parent socket, may be NULL
                  gfp_t priority,        // IN: Allocation flags
                  unsigned short type)   // IN: Socket type if sock is NULL

#endif
{
   struct sock *sk;
   VSockVmciSock *psk;
   VSockVmciSock *vsk;

   ASSERT((sock && !type) || (!sock && type));

   vsk = NULL;

   /*
    * From 2.6.9 to until 2.6.12 sk_alloc() used a cache in
    * the protocol structure, but you still had to specify the size and cache
    * yourself.
    * Most recently (in 2.6.24), sk_alloc() was changed to expect the
    * network namespace, and the option to zero the sock was dropped.
    *
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   sk = sk_alloc(vsockVmciFamilyOps.family, priority,
                 vsockVmciProto.slab_obj_size, vsockVmciProto.slab);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   sk = sk_alloc(vsockVmciFamilyOps.family, priority, &vsockVmciProto, 1);
#else
   sk = sk_alloc(net, vsockVmciFamilyOps.family, priority, &vsockVmciProto);
#endif
   if (!sk) {
      return NULL;
   }

   /*
    * If we go this far, we know the socket family is registered, so there's no
    * need to register it now.
    */
   compat_mutex_lock(&registrationMutex);
   vsockVmciSocketCount++;
   compat_mutex_unlock(&registrationMutex);

   sock_init_data(sock, sk);

   /*
    * sk->sk_type is normally set in sock_init_data, but only if
    * sock is non-NULL. We make sure that our sockets always have a type
    * by setting it here if needed.
    */
   if (!sock) {
      sk->sk_type = type;
   }

   vsk = vsock_sk(sk);
   VSockAddr_Init(&vsk->localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
   VSockAddr_Init(&vsk->remoteAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);

   sk->sk_destruct = VSockVmciSkDestruct;
   sk->sk_backlog_rcv = VSockVmciQueueRcvSkb;
   sk->sk_state = 0;
   sock_reset_flag(sk, SOCK_DONE);

   INIT_LIST_HEAD(&vsk->boundTable);
   INIT_LIST_HEAD(&vsk->connectedTable);
   vsk->dgHandle = VMCI_INVALID_HANDLE;
   vsk->qpHandle = VMCI_INVALID_HANDLE;
   vsk->qpair = NULL;
   vsk->produceSize = vsk->consumeSize = 0;
   vsk->listener = NULL;
   INIT_LIST_HEAD(&vsk->pendingLinks);
   INIT_LIST_HEAD(&vsk->acceptQueue);
   vsk->rejected = FALSE;
   vsk->sentRequest = FALSE;
   vsk->ignoreConnectingRst = FALSE;
   vsk->attachSubId = vsk->detachSubId = VMCI_INVALID_ID;
   vsk->peerShutdown = 0;

   if (parent) {
      psk = vsock_sk(parent);
      vsk->trusted = psk->trusted;
      vsk->owner = psk->owner;
      vsk->queuePairSize = psk->queuePairSize;
      vsk->queuePairMinSize = psk->queuePairMinSize;
      vsk->queuePairMaxSize = psk->queuePairMaxSize;
      vsk->connectTimeout = psk->connectTimeout;
   } else {
      vsk->trusted = capable(CAP_NET_ADMIN);
      vsk->owner = current_uid();
      vsk->queuePairSize = VSOCK_DEFAULT_QP_SIZE;
      vsk->queuePairMinSize = VSOCK_DEFAULT_QP_SIZE_MIN;
      vsk->queuePairMaxSize = VSOCK_DEFAULT_QP_SIZE_MAX;
      vsk->connectTimeout = VSOCK_DEFAULT_CONNECT_TIMEOUT;
   }

   vsk->notifyOps = NULL;

   if (sock) {
      VSockVmciInsertBound(vsockUnboundSockets, sk);
   }

   return sk;
}


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciRelease --
 *
 *      Releases the provided socket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Any pending sockets are also released.
 *
 *----------------------------------------------------------------------------
 */

static void
__VSockVmciRelease(struct sock *sk) // IN
{
   if (sk) {
      struct sk_buff *skb;
      struct sock *pending;
      struct VSockVmciSock *vsk;

      vsk = vsock_sk(sk);
      pending = NULL;  /* Compiler warning. */

      if (VSockVmciInBoundTable(sk)) {
         VSockVmciRemoveBound(sk);
      }

      if (VSockVmciInConnectedTable(sk)) {
         VSockVmciRemoveConnected(sk);
      }

      if (!VMCI_HANDLE_INVALID(vsk->dgHandle)) {
         vmci_datagram_destroy_handle(vsk->dgHandle);
         vsk->dgHandle = VMCI_INVALID_HANDLE;
      }

      lock_sock(sk);
      sock_orphan(sk);
      sk->sk_shutdown = SHUTDOWN_MASK;

      while ((skb = skb_dequeue(&sk->sk_receive_queue))) {
         kfree_skb(skb);
      }

      /* Clean up any sockets that never were accepted. */
      while ((pending = VSockVmciDequeueAccept(sk)) != NULL) {
         __VSockVmciRelease(pending);
         sock_put(pending);
      }

      release_sock(sk);
      sock_put(sk);
   }
}


/*
 * Sock operations.
 */

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSkDestruct --
 *
 *    Destroys the provided socket.  This is called by sk_free(), which is
 *    invoked when the reference count of the socket drops to zero.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Socket count is decremented.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciSkDestruct(struct sock *sk) // IN
{
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);

   if (vsk->attachSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(vsk->attachSubId);
      vsk->attachSubId = VMCI_INVALID_ID;
   }

   if (vsk->detachSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(vsk->detachSubId);
      vsk->detachSubId = VMCI_INVALID_ID;
   }

   if (!VMCI_HANDLE_INVALID(vsk->qpHandle)) {
      ASSERT(vsk->qpair);
      vmci_qpair_detach(&vsk->qpair);
      vsk->qpHandle = VMCI_INVALID_HANDLE;
      ASSERT(vsk->qpair == NULL);
      vsk->produceSize = vsk->consumeSize = 0;
   }

   /*
    * Each list entry holds a reference on the socket, so we should not even be
    * here if the socket is in one of our lists.  If we are we have a stray
    * sock_put() that needs to go away.
    */
   ASSERT(!VSockVmciInBoundTable(sk));
   ASSERT(!VSockVmciInConnectedTable(sk));
   ASSERT(!VSockVmciIsPending(sk));
   ASSERT(!VSockVmciInAcceptQueue(sk));

   /*
    * When clearing these addresses, there's no need to set the family and
    * possibly register the address family with the kernel.
    */
   VSockAddr_InitNoFamily(&vsk->localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
   VSockAddr_InitNoFamily(&vsk->remoteAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);

   NOTIFYCALL(vsk, socketDestruct, sk);

   compat_mutex_lock(&registrationMutex);
   vsockVmciSocketCount--;
   VSockVmciTestUnregister();
   compat_mutex_unlock(&registrationMutex);

   VSOCK_STATS_CTLPKT_DUMP_ALL();
   VSOCK_STATS_HIST_DUMP_ALL();
   VSOCK_STATS_TOTALS_DUMP_ALL();
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciQueueRcvSkb --
 *
 *    Receives skb on the socket's receive queue.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciQueueRcvSkb(struct sock *sk,     // IN
                     struct sk_buff *skb) // IN
{
   int err;

   err = sock_queue_rcv_skb(sk, skb);
   if (err) {
      kfree_skb(skb);
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRegisterProto --
 *
 *      Registers the vSockets protocol family.
 *
 * Results:
 *      Zero on success, error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRegisterProto(void)
{
   int err = 0;

   /*
    * From 2.6.9 until 2.6.11, these address families called sk_alloc_slab()
    * and the allocated slab was assigned to the slab variable in the proto
    * struct and was created of size slab_obj_size. As of 2.6.12 and later,
    * this slab allocation was moved
    * into proto_register() and only done if you specified a non-zero value
    * for the second argument (alloc_slab); the size of the slab element was
    * changed to obj_size.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   err = sk_alloc_slab(&vsockVmciProto, "vsock");
   if (err != 0) {
      sk_alloc_slab_error(&vsockVmciProto);
   }
#else
   /* Specify 1 as the second argument so the slab is created for us. */
   err = proto_register(&vsockVmciProto, 1);
#endif

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciUnregisterProto --
 *
 *      Unregisters the vSockets protocol family.
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
VSockVmciUnregisterProto(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   sk_free_slab(&vsockVmciProto);
#else
   proto_unregister(&vsockVmciProto);
#endif

   VSOCK_STATS_RESET();
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRegisterAddressFamily --
 *
 *      Registers our socket address family with the kernel.
 *
 *      Note that this assumes the registration lock is held.
 *
 * Results:
 *      The address family value on success, negative error code on failure.
 *
 * Side effects:
 *      Callers of socket operations with the returned value, on success, will
 *      be able to use our socket implementation.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRegisterAddressFamily(void)
{
   int err = 0;
   int i;

   /*
    * Linux will not allocate an address family to code that is not part of the
    * kernel proper, so until that time comes we need a workaround.  Here we
    * loop through the allowed values and claim the first one that's not
    * currently used.  Users will then make an ioctl(2) into our module to
    * retrieve this value before calling socket(2).
    *
    * This is undesirable, but it's better than having users' programs break
    * when a hard-coded, currently-available value gets assigned to someone
    * else in the future.
    */
   for (i = NPROTO - 1; i >= 0; i--) {
      vsockVmciFamilyOps.family = i;
      err = sock_register(&vsockVmciFamilyOps);
      if (err) {
         vsockVmciFamilyOps.family = VSOCK_INVALID_FAMILY;
      } else {
         vsockVmciDgramOps.family = i;
         vsockVmciStreamOps.family = i;
         err = i;
         break;
      }
   }

   if (VSOCK_INVALID_FAMILY == vsockVmciFamilyOps.family) {
      Warning("Could not register address family.\n");
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciUnregisterAddressFamily --
 *
 *      Unregisters the address family with the kernel.
 *
 *      Note that this assumes the registration lock is held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Our socket implementation is no longer accessible.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciUnregisterAddressFamily(void)
{
   if (vsockVmciFamilyOps.family != VSOCK_INVALID_FAMILY) {
      sock_unregister(vsockVmciFamilyOps.family);
   }

   vsockVmciDgramOps.family = vsockVmciFamilyOps.family = VSOCK_INVALID_FAMILY;
   vsockVmciStreamOps.family = vsockVmciFamilyOps.family;
}




/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRegisterWithVmci --
 *
 *      Registers with the VMCI device, and creates control message
 *      and event handlers.
 *
 * Results:
 *      Zero on success, error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRegisterWithVmci(void)
{
   int err = 0;
   uint32 apiVersion;

   /*
    * We don't call into the vmci module if the vmci device isn't
    * present.
    */
   apiVersion = VMCI_KERNEL_API_VERSION_1;
   vmciDevicePresent = vmci_device_get(&apiVersion, NULL, NULL, NULL);
   if (!vmciDevicePresent) {
      Warning("VMCI device not present.\n");
      return -1;
   }

   /*
    * Create the datagram handle that we will use to send and receive all
    * VSocket control messages for this context.
    */
    err = VSockVmciDatagramCreateHnd(VSOCK_PACKET_RID,
                                     VMCI_FLAG_ANYCID_DG_HND,
                                     VSockVmciRecvStreamCB, NULL,
                                     &vmciStreamHandle);
    if (err < VMCI_SUCCESS) {
      Warning("Unable to create datagram handle. (%d)\n", err);
      err = VSockVmci_ErrorToVSockError(err);
      goto out;
   }

   err = vmci_event_subscribe(VMCI_EVENT_QP_RESUMED,
                              VSockVmciQPResumedCB,
                              NULL,
                              &qpResumedSubId);
   if (err < VMCI_SUCCESS) {
      Warning("Unable to subscribe to QP resumed event. (%d)\n", err);
      err = VSockVmci_ErrorToVSockError(err);
      qpResumedSubId = VMCI_INVALID_ID;
      goto out;
   }

out:
   if (err != 0) {
      VSockVmciUnregisterWithVmci();
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciUnregisterWithVmci --
 *
 *      Destroys control message and event handlers, and unregisters
 *      with the VMCI device
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Our socket implementation is no longer accessible.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciUnregisterWithVmci(void)
{
   if (!vmciDevicePresent) {
      /* Nothing was registered. */
      return;
   }

   if (!VMCI_HANDLE_INVALID(vmciStreamHandle)) {
      if (vmci_datagram_destroy_handle(vmciStreamHandle) != VMCI_SUCCESS) {
         Warning("Could not destroy VMCI datagram handle.\n");
      }
      vmciStreamHandle = VMCI_INVALID_HANDLE;
   }

   if (qpResumedSubId != VMCI_INVALID_ID) {
      vmci_event_unsubscribe(qpResumedSubId);
      qpResumedSubId = VMCI_INVALID_ID;
   }

   vmci_device_release(NULL);
   vmciDevicePresent = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamHasData --
 *
 *      Gets the amount of data available for a given stream socket's consume
 *      queue.
 *
 *      Note that this assumes the socket lock is held.
 *
 * Results:
 *      The amount of data available or a VMCI error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int64
VSockVmciStreamHasData(VSockVmciSock *vsk) // IN
{
   ASSERT(vsk);

   return vmci_qpair_consume_buf_ready(vsk->qpair);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamHasSpace --
 *
 *      Gets the amount of space available for a give stream socket's produce
 *      queue.
 *
 *      Note that this assumes the socket lock is held.
 *
 * Results:
 *      The amount of space available or a VMCI error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int64
VSockVmciStreamHasSpace(VSockVmciSock *vsk) // IN
{
   ASSERT(vsk);

   return vmci_qpair_produce_free_space(vsk->qpair);
}


/*
 * Socket operations.
 */

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciRelease --
 *
 *    Releases the provided socket by freeing the contents of its queue.  This
 *    is called when a user process calls close(2) on the socket.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciRelease(struct socket *sock) // IN
{
   __VSockVmciRelease(sock->sk);
   sock->sk = NULL;
   sock->state = SS_FREE;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciBind --
 *
 *    Binds the provided address to the provided socket.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciBind(struct socket *sock,    // IN
              struct sockaddr *addr,  // IN
              int addrLen)            // IN
{
   int err;
   struct sock *sk;
   struct sockaddr_vm *vmciAddr;

   sk = sock->sk;

   if (VSockAddr_Cast(addr, addrLen, &vmciAddr) != 0) {
      return -EINVAL;
   }

   lock_sock(sk);
   err = __VSockVmciBind(sk, vmciAddr);
   release_sock(sk);

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDgramConnect --
 *
 *    Connects a datagram socket.  This can be called multiple times to change
 *    the socket's association and can be called with a sockaddr whose family
 *    is set to AF_UNSPEC to dissolve any existing association.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciDgramConnect(struct socket *sock,   // IN
                      struct sockaddr *addr, // IN
                      int addrLen,           // IN
                      int flags)             // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   struct sockaddr_vm *remoteAddr;

   sk = sock->sk;
   vsk = vsock_sk(sk);

   err = VSockAddr_Cast(addr, addrLen, &remoteAddr);
   if (err == -EAFNOSUPPORT && remoteAddr->svm_family == AF_UNSPEC) {
      lock_sock(sk);
      VSockAddr_Init(&vsk->remoteAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
      sock->state = SS_UNCONNECTED;
      release_sock(sk);
      return 0;
   } else if (err != 0) {
      return -EINVAL;
   }

   lock_sock(sk);

   if (!VSockAddr_Bound(&vsk->localAddr)) {
      struct sockaddr_vm localAddr;

      VSockAddr_Init(&localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
      if ((err = __VSockVmciBind(sk, &localAddr))) {
         goto out;
      }
   }

   if (!VSockAddr_SocketContextDgram(remoteAddr->svm_cid,
                                     remoteAddr->svm_port)) {
      err = -EINVAL;
      goto out;
   }

   memcpy(&vsk->remoteAddr, remoteAddr, sizeof vsk->remoteAddr);
   sock->state = SS_CONNECTED;

out:
   release_sock(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciConnectTimeout --
 *
 *    Asynchronous connection attempts schedule this timeout function
 *    to notify the connector of an unsuccessfull connection
 *    attempt. If the socket is still in the connecting state and
 *    hasn't been closed, we mark the socket as timed out. Otherwise,
 *    we do nothing.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May destroy the socket.
 *
 *----------------------------------------------------------------------------
 */

static void
VSockVmciConnectTimeout(compat_delayed_work_arg work)    // IN
{
   struct sock *sk;
   VSockVmciSock *vsk;

   vsk = COMPAT_DELAYED_WORK_GET_DATA(work, VSockVmciSock, dwork);
   ASSERT(vsk);

   sk = sk_vsock(vsk);

   lock_sock(sk);
   if (sk->sk_state == SS_CONNECTING && (sk->sk_shutdown != SHUTDOWN_MASK)) {
      sk->sk_state = SS_UNCONNECTED;
      sk->sk_err = ETIMEDOUT;
      sk->sk_error_report(sk);
   }
   release_sock(sk);

   sock_put(sk);
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamConnect --
 *
 *    Connects a stream socket.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciStreamConnect(struct socket *sock,   // IN
                       struct sockaddr *addr, // IN
                       int addrLen,           // IN
                       int flags)             // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   struct sockaddr_vm *remoteAddr;
   long timeout;
   Bool oldPktProto = FALSE;
   DEFINE_WAIT(wait);

   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   lock_sock(sk);

   /* XXX AF_UNSPEC should make us disconnect like AF_INET. */
   switch (sock->state) {
   case SS_CONNECTED:
      err = -EISCONN;
      goto out;
   case SS_DISCONNECTING:
      err = -EINVAL;
      goto out;
   case SS_CONNECTING:
      /*
       * This continues on so we can move sock into the SS_CONNECTED state once
       * the connection has completed (at which point err will be set to zero
       * also).  Otherwise, we will either wait for the connection or return
       * -EALREADY should this be a non-blocking call.
       */
      err = -EALREADY;
      break;
   default:
      ASSERT(sk->sk_state == SS_FREE ||
             sk->sk_state == SS_UNCONNECTED ||
             sk->sk_state == SS_LISTEN);
      if ((sk->sk_state == SS_LISTEN) ||
          VSockAddr_Cast(addr, addrLen, &remoteAddr) != 0) {
         err = -EINVAL;
         goto out;
      }

      /* The hypervisor and well-known contexts do not have socket endpoints. */
      if (!VSockAddr_SocketContextStream(remoteAddr->svm_cid)) {
         err = -ENETUNREACH;
         goto out;
      }

      /* Set the remote address that we are connecting to. */
      memcpy(&vsk->remoteAddr, remoteAddr, sizeof vsk->remoteAddr);

      /* Autobind this socket to the local address if necessary. */
      if (!VSockAddr_Bound(&vsk->localAddr)) {
         struct sockaddr_vm localAddr;

         VSockAddr_Init(&localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
         if ((err = __VSockVmciBind(sk, &localAddr))) {
            goto out;
         }
      }

      sk->sk_state = SS_CONNECTING;

      if (VSockVmciOldProtoOverride(&oldPktProto) && oldPktProto) {
         err = VSOCK_SEND_CONN_REQUEST(sk, vsk->queuePairSize);
         if (err < 0) {
            sk->sk_state = SS_UNCONNECTED;
            goto out;
         }
      } else {
         int supportedProtoVersions = VSockVmciNewProtoSupportedVersions();
         err = VSOCK_SEND_CONN_REQUEST2(sk, vsk->queuePairSize,
                                        supportedProtoVersions);
         if (err < 0) {
            sk->sk_state = SS_UNCONNECTED;
            goto out;
         }

         vsk->sentRequest = TRUE;
      }

      /*
       * Mark sock as connecting and set the error code to in progress in case
       * this is a non-blocking connect.
       */
      sock->state = SS_CONNECTING;
      err = -EINPROGRESS;
   }

   /*
    * The receive path will handle all communication until we are able to enter
    * the connected state.  Here we wait for the connection to be completed or
    * a notification of an error.
    */
   timeout = vsk->connectTimeout;
   prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

   while (sk->sk_state != SS_CONNECTED && sk->sk_err == 0) {
      if (flags & O_NONBLOCK) {
         /*
          * If we're not going to block, we schedule a timeout
          * function to generate a timeout on the connection attempt,
          * in case the peer doesn't respond in a timely manner. We
          * hold on to the socket until the timeout fires.
          */
         sock_hold(sk);
         COMPAT_INIT_DELAYED_WORK(&vsk->dwork, VSockVmciConnectTimeout, vsk);
         compat_schedule_delayed_work(&vsk->dwork, timeout);

         /* Skip ahead to preserve error code set above. */
         goto outWait;
      }

      release_sock(sk);
      timeout = schedule_timeout(timeout);
      lock_sock(sk);

      if (signal_pending(current)) {
         err = sock_intr_errno(timeout);
         goto outWaitError;
      } else if (timeout == 0) {
         err = -ETIMEDOUT;
         goto outWaitError;
      }

      prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
   }

   if (sk->sk_err) {
      err = -sk->sk_err;
      goto outWaitError;
   } else {
      ASSERT(sk->sk_state == SS_CONNECTED);
      err = 0;
   }

outWait:
   finish_wait(sk_sleep(sk), &wait);
out:
   release_sock(sk);
   return err;

outWaitError:
   sk->sk_state = SS_UNCONNECTED;
   sock->state = SS_UNCONNECTED;
   goto outWait;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciAccept --
 *
 *      Accepts next available connection request for this socket.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciAccept(struct socket *sock,     // IN
                struct socket *newsock,  // IN/OUT
                int flags)               // IN
{
   struct sock *listener;
   int err;
   struct sock *connected;
   VSockVmciSock *vconnected;
   long timeout;
   DEFINE_WAIT(wait);

   err = 0;
   listener = sock->sk;

   lock_sock(listener);

   if (sock->type != SOCK_STREAM) {
      err = -EOPNOTSUPP;
      goto out;
   }

   if (listener->sk_state != SS_LISTEN) {
      err = -EINVAL;
      goto out;
   }

   /*
    * Wait for children sockets to appear; these are the new sockets created
    * upon connection establishment.
    */
   timeout = sock_sndtimeo(listener, flags & O_NONBLOCK);
   prepare_to_wait(sk_sleep(listener), &wait, TASK_INTERRUPTIBLE);

   while ((connected = VSockVmciDequeueAccept(listener)) == NULL &&
          listener->sk_err == 0) {
      release_sock(listener);
      timeout = schedule_timeout(timeout);
      lock_sock(listener);

      if (signal_pending(current)) {
         err = sock_intr_errno(timeout);
         goto outWait;
      } else if (timeout == 0) {
         err = -EAGAIN;
         goto outWait;
      }

      prepare_to_wait(sk_sleep(listener), &wait, TASK_INTERRUPTIBLE);
   }

   if (listener->sk_err) {
      err = -listener->sk_err;
   }

   if (connected) {
      listener->sk_ack_backlog--;

      lock_sock(connected);
      vconnected = vsock_sk(connected);

      /*
       * If the listener socket has received an error, then we should reject
       * this socket and return.  Note that we simply mark the socket rejected,
       * drop our reference, and let the cleanup function handle the cleanup;
       * the fact that we found it in the listener's accept queue guarantees
       * that the cleanup function hasn't run yet.
       */
      if (err) {
         vconnected->rejected = TRUE;
         release_sock(connected);
         sock_put(connected);
         goto outWait;
      }

      newsock->state = SS_CONNECTED;
      sock_graft(connected, newsock);
      release_sock(connected);
      sock_put(connected);
   }

outWait:
   finish_wait(sk_sleep(listener), &wait);
out:
   release_sock(listener);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciGetname --
 *
 *    Provides the local or remote address for the socket.
 *
 * Results:
 *    Zero on success, negative error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciGetname(struct socket *sock,    // IN
                 struct sockaddr *addr,  // OUT
                 int *addrLen,           // OUT
                 int peer)               // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   struct sockaddr_vm *vmciAddr;

   sk = sock->sk;
   vsk = vsock_sk(sk);
   err = 0;

   lock_sock(sk);

   if (peer) {
      if (sock->state != SS_CONNECTED) {
         err = -ENOTCONN;
         goto out;
      }
      vmciAddr = &vsk->remoteAddr;
   } else {
      vmciAddr = &vsk->localAddr;
   }

   if (!vmciAddr) {
      err = -EINVAL;
      goto out;
   }

   /*
    * sys_getsockname() and sys_getpeername() pass us a MAX_SOCK_ADDR-sized
    * buffer and don't set addrLen.  Unfortunately that macro is defined in
    * socket.c instead of .h, so we hardcode its value here.
    */
   ASSERT_ON_COMPILE(sizeof *vmciAddr <= 128);
   memcpy(addr, vmciAddr, sizeof *vmciAddr);
   *addrLen = sizeof *vmciAddr;

out:
   release_sock(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciPoll --
 *
 *    Waits on file for activity then provides mask indicating state of socket.
 *
 * Results:
 *    Mask of flags containing socket state.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
VSockVmciPoll(struct file *file,    // IN
              struct socket *sock,  // IN
              poll_table *wait)     // IN
{
   struct sock *sk;
   unsigned int mask;
   VSockVmciSock *vsk;

   sk = sock->sk;
   vsk = vsock_sk(sk);

   poll_wait(file, sk_sleep(sk), wait);
   mask = 0;

   if (sk->sk_err) {
      /* Signify that there has been an error on this socket. */
      mask |= POLLERR;
   }

   /*
    * INET sockets treat local write shutdown and peer write shutdown
    * as a case of POLLHUP set.
    */
   if ((sk->sk_shutdown == SHUTDOWN_MASK) ||
       ((sk->sk_shutdown & SEND_SHUTDOWN) &&
        (vsk->peerShutdown & SEND_SHUTDOWN))) {
      mask |= POLLHUP;
   }

   /* POLLRDHUP wasn't added until 2.6.17. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
   if (sk->sk_shutdown & RCV_SHUTDOWN ||
       vsk->peerShutdown & SEND_SHUTDOWN) {
      mask |= POLLRDHUP;
   }
#endif

   if (sock->type == SOCK_DGRAM) {
      /*
       * For datagram sockets we can read if there is something in the queue
       * and write as long as the socket isn't shutdown for sending.
       */
      if (!skb_queue_empty(&sk->sk_receive_queue) ||
          (sk->sk_shutdown & RCV_SHUTDOWN)) {
         mask |= POLLIN | POLLRDNORM;
      }

      if (!(sk->sk_shutdown & SEND_SHUTDOWN)) {
         mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
      }
   } else if (sock->type == SOCK_STREAM) {
      lock_sock(sk);

      /*
       * Listening sockets that have connections in their accept queue can be read.
       */
      if (sk->sk_state == SS_LISTEN && !VSockVmciIsAcceptQueueEmpty(sk)) {
         mask |= POLLIN | POLLRDNORM;
      }

      /*
       * If there is something in the queue then we can read.
       */
      if (!VMCI_HANDLE_INVALID(vsk->qpHandle) &&
          !(sk->sk_shutdown & RCV_SHUTDOWN)) {
         Bool dataReadyNow = FALSE;
         int32 ret = 0;
         NOTIFYCALLRET(vsk, ret, pollIn, sk, 1, &dataReadyNow);
         if (ret < 0) {
            mask |= POLLERR;
         } else {
            if (dataReadyNow) {
               mask |= POLLIN | POLLRDNORM;
            }
         }
      }

      /*
       * Sockets whose connections have been closed, reset, or terminated
       * should also be considered read, and we check the shutdown flag for
       * that.
       */
      if (sk->sk_shutdown & RCV_SHUTDOWN ||
          vsk->peerShutdown & SEND_SHUTDOWN) {
          mask |= POLLIN | POLLRDNORM;
      }

      /*
       * Connected sockets that can produce data can be written.
       */
      if (sk->sk_state == SS_CONNECTED) {
         if (!(sk->sk_shutdown & SEND_SHUTDOWN)) {
            Bool spaceAvailNow = FALSE;
            int32 ret = 0;

            NOTIFYCALLRET(vsk, ret, pollOut, sk, 1, &spaceAvailNow);
            if (ret < 0) {
               mask |= POLLERR;
            } else {
               if (spaceAvailNow) {
                  /* Remove POLLWRBAND since INET sockets are not setting it.*/
                  mask |= POLLOUT | POLLWRNORM;
               }
            }
         }
      }

      /*
       * Simulate INET socket poll behaviors, which sets POLLOUT|POLLWRNORM when
       * peer is closed and nothing to read, but local send is not shutdown.
       */
      if (sk->sk_state == SS_UNCONNECTED) {
         if (!(sk->sk_shutdown & SEND_SHUTDOWN)) {
            mask |= POLLOUT | POLLWRNORM;
         }
      }

      release_sock(sk);
   }

   return mask;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciListen --
 *
 *      Signify that this socket is listening for connection requests.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciListen(struct socket *sock,    // IN
                int backlog)            // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;

   sk = sock->sk;

   lock_sock(sk);

   if (sock->type != SOCK_STREAM) {
      err = -EOPNOTSUPP;
      goto out;
   }

   if (sock->state != SS_UNCONNECTED) {
      err = -EINVAL;
      goto out;
   }

   vsk = vsock_sk(sk);

   if (!VSockAddr_Bound(&vsk->localAddr)) {
      err = -EINVAL;
      goto out;
   }

   sk->sk_max_ack_backlog = backlog;
   sk->sk_state = SS_LISTEN;

   err = 0;

out:
   release_sock(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciShutdown --
 *
 *    Shuts down the provided socket in the provided method.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciShutdown(struct socket *sock,  // IN
                  int mode)             // IN
{
   int32 err;
   struct sock *sk;

   /*
    * User level uses SHUT_RD (0) and SHUT_WR (1), but the kernel uses
    * RCV_SHUTDOWN (1) and SEND_SHUTDOWN (2), so we must increment mode here
    * like the other address families do.  Note also that the increment makes
    * SHUT_RDWR (2) into RCV_SHUTDOWN | SEND_SHUTDOWN (3), which is what we
    * want.
    */
   mode++;

   if ((mode & ~SHUTDOWN_MASK) || !mode) {
      return -EINVAL;
   }

   /*
    * If this is a STREAM socket and it is not connected then bail out
    * immediately.  If it is a DGRAM socket then we must first kick the socket
    * so that it wakes up from any sleeping calls, for example recv(), and then
    * afterwards return the error.
    */

   sk = sock->sk;
   if (sock->state == SS_UNCONNECTED) {
      err = -ENOTCONN;
      if (sk->sk_type == SOCK_STREAM) {
         return err;
      }
   } else {
      sock->state = SS_DISCONNECTING;
      err = 0;
   }

   /* Receive and send shutdowns are treated alike. */
   mode = mode & (RCV_SHUTDOWN | SEND_SHUTDOWN);
   if (mode) {
      lock_sock(sk);
      sk->sk_shutdown |= mode;
      sk->sk_state_change(sk);
      release_sock(sk);

      if (sk->sk_type == SOCK_STREAM) {
         sock_reset_flag(sk, SOCK_DONE);
         VSOCK_SEND_SHUTDOWN(sk, mode);
      }
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDgramSendmsg --
 *
 *    Sends a datagram.
 *
 * Results:
 *    Number of bytes sent on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciDgramSendmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to send on
                      struct msghdr *msg,           // IN: message to send
                      size_t len)                   // IN: length of message
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   struct sockaddr_vm *remoteAddr;
   VMCIDatagram *dg;

   if (msg->msg_flags & MSG_OOB) {
      return -EOPNOTSUPP;
   }

   if (len > VMCI_MAX_DG_PAYLOAD_SIZE) {
      return -EMSGSIZE;
   }

   /* For now, MSG_DONTWAIT is always assumed... */
   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   lock_sock(sk);

   if (!VSockAddr_Bound(&vsk->localAddr)) {
      struct sockaddr_vm localAddr;

      VSockAddr_Init(&localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
      if ((err = __VSockVmciBind(sk, &localAddr))) {
         goto out;
      }
   }

   /*
    * If the provided message contains an address, use that.  Otherwise fall
    * back on the socket's remote handle (if it has been connected).
    */
   if (msg->msg_name &&
       VSockAddr_Cast(msg->msg_name, msg->msg_namelen, &remoteAddr) == 0) {
      /* Ensure this address is of the right type and is a valid destination. */
      // XXXAB Temporary to handle test program
      if (remoteAddr->svm_cid == VMADDR_CID_ANY) {
         remoteAddr->svm_cid = vmci_get_context_id();
      }

      if (!VSockAddr_Bound(remoteAddr)) {
         err = -EINVAL;
         goto out;
      }
   } else if (sock->state == SS_CONNECTED) {
      remoteAddr = &vsk->remoteAddr;
      // XXXAB Temporary to handle test program
      if (remoteAddr->svm_cid == VMADDR_CID_ANY) {
         remoteAddr->svm_cid = vmci_get_context_id();
      }

      /* XXX Should connect() or this function ensure remoteAddr is bound? */
      if (!VSockAddr_Bound(&vsk->remoteAddr)) {
         err = -EINVAL;
         goto out;
      }
   } else {
      err = -EINVAL;
      goto out;
   }

   /*
    * Make sure that we don't allow a userlevel app to send datagrams
    * to the hypervisor that modify VMCI device state.
    */
   if (!VSockAddr_SocketContextDgram(remoteAddr->svm_cid,
                                     remoteAddr->svm_port)) {
      err = -EINVAL;
      goto out;
   }

   if (!VSockVmciAllowDgram(vsk, remoteAddr->svm_cid)) {
      err = -EPERM;
      goto out;
   }

   /*
    * Allocate a buffer for the user's message and our packet header.
    */
   dg = kmalloc(len + sizeof *dg, GFP_KERNEL);
   if (!dg) {
      err = -ENOMEM;
      goto out;
   }

   memcpy_fromiovec(VMCI_DG_PAYLOAD(dg), msg->msg_iov, len);

   dg->dst = VMCI_MAKE_HANDLE(remoteAddr->svm_cid, remoteAddr->svm_port);
   dg->src = VMCI_MAKE_HANDLE(vsk->localAddr.svm_cid, vsk->localAddr.svm_port);

   dg->payloadSize = len;

   err = vmci_datagram_send(dg);
   kfree(dg);
   if (err < 0) {
      err = VSockVmci_ErrorToVSockError(err);
      goto out;
   }

   err -= sizeof *dg;

out:
   release_sock(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamSetsockopt --
 *
 *    Set a socket option on a stream socket
 *
 * Results:
 *    0 on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciStreamSetsockopt(struct socket *sock,           // IN/OUT
                          int level,                     // IN
                          int optname,                   // IN
                          char __user *optval,           // IN
                          VSockSetsockoptLenType optlen) // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   uint64 val;

   if (level != VSockVmci_GetAFValue()) {
      return -ENOPROTOOPT;
   }

#  define COPY_IN(_v)                                              \
   do {                                                            \
      if (optlen < sizeof _v) {                                    \
         err = -EINVAL;                                            \
         goto exit;                                                \
      }                                                            \
      if (copy_from_user(&_v, optval, sizeof _v) != 0) {           \
         err = -EFAULT;                                            \
         goto exit;                                                \
      }                                                            \
   } while (0)

   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   ASSERT(vsk->queuePairMinSize <= vsk->queuePairSize &&
          vsk->queuePairSize <= vsk->queuePairMaxSize);

   lock_sock(sk);

   switch (optname) {
   case SO_VMCI_BUFFER_SIZE:
      COPY_IN(val);
      if (val < vsk->queuePairMinSize) {
         vsk->queuePairMinSize = val;
      }

      if (val > vsk->queuePairMaxSize) {
         vsk->queuePairMaxSize = val;
      }

      vsk->queuePairSize = val;
      break;

   case SO_VMCI_BUFFER_MAX_SIZE:
      COPY_IN(val);
      if (val < vsk->queuePairSize) {
         vsk->queuePairSize = val;
      }
      vsk->queuePairMaxSize = val;
      break;

   case SO_VMCI_BUFFER_MIN_SIZE:
      COPY_IN(val);
      if (val > vsk->queuePairSize) {
         vsk->queuePairSize = val;
      }
      vsk->queuePairMinSize = val;
      break;

   case SO_VMCI_CONNECT_TIMEOUT: {
      struct timeval tv;
      COPY_IN(tv);
      if (tv.tv_sec >= 0 && tv.tv_usec < USEC_PER_SEC &&
          tv.tv_sec < (MAX_SCHEDULE_TIMEOUT/HZ - 1)) {
         vsk->connectTimeout = tv.tv_sec * HZ +
                               CEILING(tv.tv_usec, (1000000 / HZ));
         if (vsk->connectTimeout == 0) {
            vsk->connectTimeout = VSOCK_DEFAULT_CONNECT_TIMEOUT;
         }
      } else {
         err = -ERANGE;
      }
      break;
   }

   default:
      err = -ENOPROTOOPT;
      break;
   }

#  undef COPY_IN

   ASSERT(vsk->queuePairMinSize <= vsk->queuePairSize &&
          vsk->queuePairSize <= vsk->queuePairMaxSize);
exit:
   release_sock(sk);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamGetsockopt --
 *
 *    Get a socket option for a stream socket
 *
 * Results:
 *    0 on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciStreamGetsockopt(struct socket *sock,          // IN
                          int level,                    // IN
                          int optname,                  // IN
                          char __user *optval,          // OUT
                          int __user * optlen)          // IN/OUT
{
   int err;
   int len;
   struct sock *sk;
   VSockVmciSock *vsk;

   if (level != VSockVmci_GetAFValue()) {
      return -ENOPROTOOPT;
   }

   if ((err = get_user(len, optlen)) != 0) {
      return err;
   }

#  define COPY_OUT(_v)                                             \
   do {                                                            \
      if (len < sizeof _v) {                                       \
         return -EINVAL;                                           \
      }                                                            \
      len = sizeof _v;                                             \
      if (copy_to_user(optval, &_v, len) != 0) {                   \
         return -EFAULT;                                           \
      }                                                            \
   } while (0)

   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   switch (optname) {
   case SO_VMCI_BUFFER_SIZE:
      COPY_OUT(vsk->queuePairSize);
      break;

   case SO_VMCI_BUFFER_MAX_SIZE:
      COPY_OUT(vsk->queuePairMaxSize);
      break;

   case SO_VMCI_BUFFER_MIN_SIZE:
      COPY_OUT(vsk->queuePairMinSize);
      break;

   case SO_VMCI_CONNECT_TIMEOUT: {
      struct timeval tv;
      tv.tv_sec = vsk->connectTimeout / HZ;
      tv.tv_usec = (vsk->connectTimeout - tv.tv_sec * HZ) * (1000000 / HZ);
      COPY_OUT(tv);
      break;
   }
   default:
      return -ENOPROTOOPT;
   }

   if ((err = put_user(len, optlen)) != 0) {
      return -EFAULT;
   }

#  undef COPY_OUT

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamSendmsg --
 *
 *    Sends a message on the socket.
 *
 * Results:
 *    Number of bytes sent on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciStreamSendmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to send on
                       struct msghdr *msg,           // IN: message to send
                       size_t len)                   // IN: length of message
{
   struct sock *sk;
   VSockVmciSock *vsk;
   ssize_t totalWritten;
   long timeout;
   int err;
   VSockVmciSendNotifyData sendData;

   DEFINE_WAIT(wait);

   sk = sock->sk;
   vsk = vsock_sk(sk);
   totalWritten = 0;
   err = 0;

   if (msg->msg_flags & MSG_OOB) {
      return -EOPNOTSUPP;
   }

   lock_sock(sk);

   /* Callers should not provide a destination with stream sockets. */
   if (msg->msg_namelen) {
      err = sk->sk_state == SS_CONNECTED ? -EISCONN : -EOPNOTSUPP;
      goto out;
   }

   /* Send data only if both sides are not shutdown in the direction. */
   if (sk->sk_shutdown & SEND_SHUTDOWN ||
       vsk->peerShutdown & RCV_SHUTDOWN) {
      err = -EPIPE;
      goto out;
   }

   if (sk->sk_state != SS_CONNECTED ||
       !VSockAddr_Bound(&vsk->localAddr)) {
      err = -ENOTCONN;
      goto out;
   }

   if (!VSockAddr_Bound(&vsk->remoteAddr)) {
      err = -EDESTADDRREQ;
      goto out;
   }

   /*
    * Wait for room in the produce queue to enqueue our user's data.
    */
   timeout = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);

   NOTIFYCALLRET(vsk, err, sendInit, sk, &sendData);
   if (err < 0) {
      goto out;
   }

   prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

   while (totalWritten < len) {
      ssize_t written;

      while (VSockVmciStreamHasSpace(vsk) == 0 &&
             sk->sk_err == 0 &&
             !(sk->sk_shutdown & SEND_SHUTDOWN) &&
             !(vsk->peerShutdown & RCV_SHUTDOWN)) {

         /* Don't wait for non-blocking sockets. */
         if (timeout == 0) {
            err = -EAGAIN;
            goto outWait;
         }

         NOTIFYCALLRET(vsk, err, sendPreBlock, sk, &sendData);

         if (err < 0) {
            goto outWait;
         }

         release_sock(sk);
         timeout = schedule_timeout(timeout);
         lock_sock(sk);
         if (signal_pending(current)) {
            err = sock_intr_errno(timeout);
            goto outWait;
         } else if (timeout == 0) {
            err = -EAGAIN;
            goto outWait;
         }

         prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
      }

      /*
       * These checks occur both as part of and after the loop conditional
       * since we need to check before and after sleeping.
       */
      if (sk->sk_err) {
         err = -sk->sk_err;
         goto outWait;
      } else if ((sk->sk_shutdown & SEND_SHUTDOWN) ||
                 (vsk->peerShutdown & RCV_SHUTDOWN)) {
         err = -EPIPE;
         goto outWait;
      }

      VSOCK_STATS_STREAM_PRODUCE_HIST(vsk);

      NOTIFYCALLRET(vsk, err, sendPreEnqueue, sk, &sendData);
      if (err < 0) {
         goto outWait;
      }

      /*
       * Note that enqueue will only write as many bytes as are free in the
       * produce queue, so we don't need to ensure len is smaller than the queue
       * size.  It is the caller's responsibility to check how many bytes we were
       * able to send.
       */

      written = vmci_qpair_enquev(vsk->qpair, msg->msg_iov,
                                  len - totalWritten, 0);
      if (written < 0) {
         err = -ENOMEM;
         goto outWait;
      }

      totalWritten += written;

      NOTIFYCALLRET(vsk, err, sendPostEnqueue, sk, written, &sendData);
      if (err < 0) {
         goto outWait;
      }
   }

   ASSERT(totalWritten <= INT_MAX);

outWait:
   if (totalWritten > 0) {
      VSOCK_STATS_STREAM_PRODUCE(totalWritten);
      err = totalWritten;
   }
   finish_wait(sk_sleep(sk), &wait);
out:
   release_sock(sk);
   return err;
}



/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDgramRecvmsg --
 *
 *    Receives a datagram and places it in the caller's msg.
 *
 * Results:
 *    The size of the payload on success, negative value on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciDgramRecvmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to receive from
                      struct msghdr *msg,           // IN/OUT: message to receive into
                      size_t len,                   // IN: length of receive buffer
                      int flags)                    // IN: receive flags
{
   int err;
   int noblock;
   struct sock *sk;
   VMCIDatagram *dg;
   size_t payloadLen;
   struct sk_buff *skb;

   sk = sock->sk;
   noblock = flags & MSG_DONTWAIT;

   if (flags & MSG_OOB || flags & MSG_ERRQUEUE) {
      return -EOPNOTSUPP;
   }

   /* Retrieve the head sk_buff from the socket's receive queue. */
   err = 0;
   skb = skb_recv_datagram(sk, flags, noblock, &err);
   if (err) {
      return err;
   }

   if (!skb) {
      return -EAGAIN;
   }

   dg = (VMCIDatagram *)skb->data;
   if (!dg) {
      /* err is 0, meaning we read zero bytes. */
      goto out;
   }

   payloadLen = dg->payloadSize;
   /* Ensure the sk_buff matches the payload size claimed in the packet. */
   if (payloadLen != skb->len - sizeof *dg) {
      err = -EINVAL;
      goto out;
   }

   if (payloadLen > len) {
      payloadLen = len;
      msg->msg_flags |= MSG_TRUNC;
   }

   /* Place the datagram payload in the user's iovec. */
   err = skb_copy_datagram_iovec(skb, sizeof *dg, msg->msg_iov, payloadLen);
   if (err) {
      goto out;
   }

   msg->msg_namelen = 0;
   if (msg->msg_name) {
      struct sockaddr_vm *vmciAddr;

      /* Provide the address of the sender. */
      vmciAddr = (struct sockaddr_vm *)msg->msg_name;
      VSockAddr_Init(vmciAddr,
                     VMCI_HANDLE_TO_CONTEXT_ID(dg->src),
                     VMCI_HANDLE_TO_RESOURCE_ID(dg->src));
      msg->msg_namelen = sizeof *vmciAddr;
   }
   err = payloadLen;

out:
   skb_free_datagram(sk, skb);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStreamRecvmsg --
 *
 *    Receives a datagram and places it in the caller's msg.
 *
 * Results:
 *    The size of the payload on success, negative value on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciStreamRecvmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to receive from
                       struct msghdr *msg,           // IN/OUT: message to receive into
                       size_t len,                   // IN: length of receive buffer
                       int flags)                    // IN: receive flags
{
   struct sock *sk;
   VSockVmciSock *vsk;
   int err;
   size_t target;
   ssize_t copied;
   long timeout;

   VSockVmciRecvNotifyData recvData;

   DEFINE_WAIT(wait);

   sk = sock->sk;
   vsk = vsock_sk(sk);
   err = 0;

   lock_sock(sk);

   if (sk->sk_state != SS_CONNECTED) {
      /*
       * Recvmsg is supposed to return 0 if a peer performs an orderly shutdown.
       * Differentiate between that case and when a peer has not connected or a
       * local shutdown occured with the SOCK_DONE flag.
       */
      if (sock_flag(sk, SOCK_DONE)) {
         err = 0;
      } else {
         err = -ENOTCONN;
      }
      goto out;
   }

   if (flags & MSG_OOB) {
      err = -EOPNOTSUPP;
      goto out;
   }

   /*
    * We don't check peerShutdown flag here since peer may actually shut down,
    * but there can be data in the VMCI queue that local socket can receive.
    */
   if (sk->sk_shutdown & RCV_SHUTDOWN) {
      err = 0;
      goto out;
   }

   /*
    * It is valid on Linux to pass in a zero-length receive buffer.  This
    * is not an error.  We may as well bail out now.  Note that if we don't,
    * we will fail "ASSERT(copied >= target)" after we dequeue, because the
    * minimum target is always 1 byte.
    */
   if (!len) {
      err = 0;
      goto out;
   }

   /*
    * We must not copy less than target bytes into the user's buffer before
    * returning successfully, so we wait for the consume queue to have that
    * much data to consume before dequeueing.  Note that this makes it
    * impossible to handle cases where target is greater than the queue size.
    */
   target = sock_rcvlowat(sk, flags & MSG_WAITALL, len);
   if (target >= vsk->consumeSize) {
      err = -ENOMEM;
      goto out;
   }
   timeout = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
   copied = 0;

   NOTIFYCALLRET(vsk, err, recvInit, sk, target, &recvData);
   if (err < 0) {
      goto out;
   }

   prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

   while (1) {
      int64 ready = VSockVmciStreamHasData(vsk);

      if (ready < 0) {
         /*
          * Invalid queue pair content. XXX This should be changed to
          * a connection reset in a later change.
          */

         err = -ENOMEM;
         goto outWait;
      } else if (ready > 0) {
         ssize_t read;

         VSOCK_STATS_STREAM_CONSUME_HIST(vsk);

         NOTIFYCALLRET(vsk, err, recvPreDequeue, sk, target, &recvData);
         if (err < 0) {
            break;
         }

         if (flags & MSG_PEEK) {
            read = vmci_qpair_peekv(vsk->qpair, msg->msg_iov, len - copied, 0);
         } else {
            read = vmci_qpair_dequev(vsk->qpair, msg->msg_iov, len - copied, 0);
         }

         if (read < 0) {
            err = -ENOMEM;
            break;
         }

         ASSERT(read <= INT_MAX);
         copied += read;

         NOTIFYCALLRET(vsk, err, recvPostDequeue, sk, target, read,
                       !(flags & MSG_PEEK), &recvData);
         if (err < 0) {
            goto outWait;
         }

         if (read >= target || flags & MSG_PEEK) {
            break;
         }
         target -= read;
      } else {
         if (sk->sk_err != 0 || (sk->sk_shutdown & RCV_SHUTDOWN) ||
             (vsk->peerShutdown & SEND_SHUTDOWN)) {
            break;
         }
         /* Don't wait for non-blocking sockets. */
         if (timeout == 0) {
            err = -EAGAIN;
            break;
         }

         NOTIFYCALLRET(vsk, err, recvPreBlock, sk, target, &recvData);
         if (err < 0) {
            break;
         }

         release_sock(sk);
         timeout = schedule_timeout(timeout);
         lock_sock(sk);

         if (signal_pending(current)) {
            err = sock_intr_errno(timeout);
            break;
         } else if (timeout == 0) {
            err = -EAGAIN;
            break;
         }

         prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
      }
   }

   if (sk->sk_err) {
      err = -sk->sk_err;
   } else if (sk->sk_shutdown & RCV_SHUTDOWN) {
      err = 0;
   }

   if (copied > 0) {
      /*
       * We only do these additional bookkeeping/notification steps if we
       * actually copied something out of the queue pair instead of just peeking
       * ahead.
       */

      if (!(flags & MSG_PEEK)) {
         VSOCK_STATS_STREAM_CONSUME(copied);

         /*
          * If the other side has shutdown for sending and there is nothing more
          * to read, then modify the socket state.
          */
         if (vsk->peerShutdown & SEND_SHUTDOWN) {
            if (VSockVmciStreamHasData(vsk) <= 0) {
               sk->sk_state = SS_UNCONNECTED;
               sock_set_flag(sk, SOCK_DONE);
               sk->sk_state_change(sk);
            }
         }
      }
      err = copied;
   }

outWait:
   finish_wait(sk_sleep(sk), &wait);
out:
   release_sock(sk);
   return err;
}


/*
 * Protocol operation.
 */

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciCreate --
 *
 *    Creates a VSocket socket.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    Socket count is incremented.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciCreate(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
                struct net *net,      // IN
#endif
                struct socket *sock,  // IN
                int protocol          // IN
#ifdef VMW_NETCREATE_KERNARG
                , int kern            // IN
#endif
               )
{
   if (!sock) {
      return -EINVAL;
   }

   if (protocol) {
      return -EPROTONOSUPPORT;
   }

   switch (sock->type) {
   case SOCK_DGRAM:
      sock->ops = &vsockVmciDgramOps;
      break;
   case SOCK_STREAM:
      sock->ops = &vsockVmciStreamOps;
      break;
   default:
      return -ESOCKTNOSUPPORT;
   }

   sock->state = SS_UNCONNECTED;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   return __VSockVmciCreate(sock, NULL, GFP_KERNEL, 0) ? 0 : -ENOMEM;
#else
   return __VSockVmciCreate(net, sock, NULL, GFP_KERNEL, 0) ? 0 : -ENOMEM;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciIoctl32Handler --
 *
 *      Handler for 32-bit ioctl(2) on 64-bit.
 *
 * Results:
 *      Same as VsockVmciDevIoctl().
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VM_X86_64
#ifndef HAVE_COMPAT_IOCTL
static int
VSockVmciIoctl32Handler(unsigned int fd,        // IN
                        unsigned int iocmd,     // IN
                        unsigned long ioarg,    // IN/OUT
                        struct file * filp)     // IN
{
   int ret;
   ret = -ENOTTY;
   if (filp && filp->f_op && filp->f_op->ioctl == VSockVmciDevIoctl) {
      ret = VSockVmciDevIoctl(filp->f_dentry->d_inode, filp, iocmd, ioarg);
   }
   return ret;
}
#endif /* !HAVE_COMPAT_IOCTL */


/*
 *----------------------------------------------------------------------------
 *
 * register_ioctl32_handlers --
 *
 *      Registers the ioctl conversion handler.
 *
 * Results:
 *      Zero on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
register_ioctl32_handlers(void)
{
#ifndef HAVE_COMPAT_IOCTL
   {
      int i;
      for (i = IOCTL_VMCI_SOCKETS_FIRST; i < IOCTL_VMCI_SOCKETS_LAST; i++) {
         int retval = register_ioctl32_conversion(i, VSockVmciIoctl32Handler);
         if (retval) {
            Warning("Fail to register ioctl32 conversion for cmd %d\n", i);
            return retval;
         }
      }
   }
#endif /* !HAVE_COMPAT_IOCTL */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * unregister_ioctl32_handlers --
 *
 *      Unregisters the ioctl converstion handler.
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
unregister_ioctl32_handlers(void)
{
#ifndef HAVE_COMPAT_IOCTL
   {
      int i;
      for (i = IOCTL_VMCI_SOCKETS_FIRST; i < IOCTL_VMCI_SOCKETS_LAST; i++) {
         int retval = unregister_ioctl32_conversion(i);
         if (retval) {
            Warning("Fail to unregister ioctl32 conversion for cmd %d\n", i);
         }
      }
   }
#endif /* !HAVE_COMPAT_IOCTL */
}
#else /* VM_X86_64 */
#define register_ioctl32_handlers() (0)
#define unregister_ioctl32_handlers() do { } while (0)
#endif /* VM_X86_64 */


/*
 * Device operations.
 */


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDevOpen --
 *
 *      Invoked when the device is opened.  Simply maintains a count of open
 *      instances.
 *
 * Results:
 *      Zero on success, negative value otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciDevOpen(struct inode *inode,  // IN
                 struct file *file)    // IN
{
   compat_mutex_lock(&registrationMutex);
   devOpenCount++;
   compat_mutex_unlock(&registrationMutex);
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDevRelease --
 *
 *      Invoked when the device is closed.  Updates the open instance count and
 *      unregisters the socket family if this is the last user.
 *
 * Results:
 *      Zero on success, negative value otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VSockVmciDevRelease(struct inode *inode,  // IN
                    struct file *file)    // IN
{
   compat_mutex_lock(&registrationMutex);
   devOpenCount--;
   VSockVmciTestUnregister();
   compat_mutex_unlock(&registrationMutex);
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciDevIoctl --
 *
 *      ioctl(2) handler.
 *
 * Results:
 *      Zero on success, negative error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VSockVmciDevIoctl(struct inode *inode,     // IN
                  struct file *filp,       // IN
                  u_int iocmd,             // IN
                  unsigned long ioarg)     // IN/OUT
{
   int retval;

   retval = 0;

   switch (iocmd) {
   case IOCTL_VMCI_SOCKETS_VERSION: {
      uint16 parts[4] = { VSOCK_DRIVER_VERSION_COMMAS };
      uint32 version = VMCI_SOCKETS_MAKE_VERSION(parts);
      if (copy_to_user((void*)ioarg, &version, sizeof version) != 0) {
         retval = -EFAULT;
      }
      break;
   }

   case IOCTL_VMCI_SOCKETS_GET_AF_VALUE: {
      int family;

      family = VSockVmci_GetAFValue();
      if (family < 0) {
         Warning("AF_VSOCK is not registered\n");
      }
      if (copy_to_user((void *)ioarg, &family, sizeof family) != 0) {
         retval = -EFAULT;
      }
      break;
   }

   case IOCTL_VMCI_SOCKETS_GET_LOCAL_CID: {
      VMCIId cid = vmci_get_context_id();
      if (copy_to_user((void *)ioarg, &cid, sizeof cid) != 0) {
         retval = -EFAULT;
      }
      break;
   }

   default:
      Warning("Unknown ioctl %d\n", iocmd);
      retval = -EINVAL;
   }

   return retval;
}


#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
/*
 *-----------------------------------------------------------------------------
 *
 * VSockVmciDevUnlockedIoctl --
 *
 *      Wrapper for VSockVmciDevIoctl() supporting the compat_ioctl and
 *      unlocked_ioctl methods that have signatures different from the
 *      old ioctl. Used as compat_ioctl method for 32bit apps running
 *      on 64bit kernel and for unlocked_ioctl on systems supporting
 *      those.  VSockVmciDevIoctl() may safely be called without holding
 *      the BKL.
 *
 * Results:
 *      Same as VSockVmciDevIoctl().
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static long
VSockVmciDevUnlockedIoctl(struct file *filp,       // IN
                          u_int iocmd,             // IN
                          unsigned long ioarg)     // IN/OUT
{
   return VSockVmciDevIoctl(NULL, filp, iocmd, ioarg);
}
#endif

/*
 * Module operations.
 */

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciInit --
 *
 *    Initialization routine for the VSockets module.
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    The VSocket protocol family and socket operations are registered.
 *
 *----------------------------------------------------------------------------
 */

static int __init
VSockVmciInit(void)
{
   int err;

   DriverLog_Init("VSock");

   request_module("vmci");

   err = misc_register(&vsockVmciDevice);
   if (err) {
      return -ENOENT;
   }

   err = register_ioctl32_handlers();
   if (err) {
      misc_deregister(&vsockVmciDevice);
      return err;
   }

   err = VSockVmciRegisterWithVmci();
   if (err) {
      Warning("Cannot register with VMCI device.\n");
      unregister_ioctl32_handlers();
      misc_deregister(&vsockVmciDevice);
      return err;
   }

   err = VSockVmciRegisterProto();
   if (err) {
      Warning("Cannot register vsock protocol.\n");
      VSockVmciUnregisterWithVmci();
      unregister_ioctl32_handlers();
      misc_deregister(&vsockVmciDevice);
      return err;
   }

   VSockVmciInitTables();
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSocketVmciExit --
 *
 *    VSockets module exit routine.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Unregisters VSocket protocol family and socket operations.
 *
 *----------------------------------------------------------------------------
 */

static void __exit
VSockVmciExit(void)
{
   unregister_ioctl32_handlers();
   misc_deregister(&vsockVmciDevice);
   compat_mutex_lock(&registrationMutex);
   VSockVmciUnregisterAddressFamily();
   compat_mutex_unlock(&registrationMutex);

   VSockVmciUnregisterProto();
   VSockVmciUnregisterWithVmci();
}


module_init(VSockVmciInit);
module_exit(VSockVmciExit);

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Socket Family");
MODULE_VERSION(VSOCK_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("vmware_vsock");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");

#ifdef VMX86_DEVEL
/* We only support protocol negotiation overrides on devel builds. */
module_param(PROTOCOL_OVERRIDE, int, 0444);
MODULE_PARM_DESC(PROTOCOL_OVERRIDE, "Specify a vsock protocol (auto negotiated by default");

int LOGLEVEL_THRESHOLD = 4;
module_param(LOGLEVEL_THRESHOLD, int, 0444);
MODULE_PARM_DESC(LOGLEVEL_THRESHOLD, "Set verbosity (0 means no log, 10 means very verbose, 4 is default)");
#endif

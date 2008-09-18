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
 * af_vsock.c --
 *
 *      Linux socket module for the VMCI Sockets protocol family.
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

#include <linux/kmod.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <asm/io.h>
#if defined(__x86_64__) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#      include <asm/ioctl32.h>
#   else
#      include <linux/ioctl32.h>
#   endif
/* Use weak: not all kernels export sys_ioctl for use by modules */
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 66)
asmlinkage __attribute__((weak)) long
sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
#   else
asmlinkage __attribute__((weak)) int
sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
#   endif
#endif

#include "compat_module.h"
#include "compat_kernel.h"
#include "compat_init.h"
#include "compat_sock.h"
#include "compat_wait.h"
#include "compat_version.h"
#include "compat_workqueue.h"
#include "compat_list.h"
#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
# include "compat_semaphore.h"
#endif

#include "vmware.h"

#include "vsockCommon.h"
#include "vsockPacket.h"
#include "vsockVmci.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmci_iocontrols.h"
#ifdef VMX86_TOOLS
# include "vmciGuestKernelAPI.h"
#else
# include "vmciDatagram.h"
#endif

#include "af_vsock.h"
#include "util.h"
#include "vsock_version.h"
#include "driverLog.h"


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
static int VSockVmciRecvDgramCB(void *data, VMCIDatagram *dg);
#ifdef VMX86_TOOLS
static int VSockVmciRecvStreamCB(void *data, VMCIDatagram *dg);
static void VSockVmciPeerAttachCB(VMCIId subId,
                                  VMCI_EventData *ed, void *clientData);
static void VSockVmciPeerDetachCB(VMCIId subId,
                                  VMCI_EventData *ed, void *clientData);
static int VSockVmciSendControlPktBH(struct sockaddr_vm *src,
                                     struct sockaddr_vm *dst,
                                     VSockPacketType type,
                                     uint64 size,
                                     uint64 mode,
                                     VSockWaitingInfo *wait,
                                     VMCIHandle handle);
static int VSockVmciSendControlPkt(struct sock *sk, VSockPacketType type,
                                   uint64 size, uint64 mode,
                                   VSockWaitingInfo *wait, VMCIHandle handle);
static void VSockVmciRecvPktWork(compat_work_arg work);
static int VSockVmciRecvListen(struct sock *sk, VSockPacket *pkt);
static int VSockVmciRecvConnectingServer(struct sock *sk,
                                         struct sock *pending, VSockPacket *pkt);
static int VSockVmciRecvConnectingClient(struct sock *sk, VSockPacket *pkt);
static int VSockVmciRecvConnectingClientNegotiate(struct sock *sk,
                                                  VSockPacket *pkt);
static int VSockVmciRecvConnected(struct sock *sk, VSockPacket *pkt);
#endif
static int __VSockVmciBind(struct sock *sk, struct sockaddr_vm *addr);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
static struct sock *__VSockVmciCreate(struct socket *sock, unsigned int priority);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static struct sock *__VSockVmciCreate(struct socket *sock, gfp_t priority);
#else
static struct sock *__VSockVmciCreate(struct net *net,
                                      struct socket *sock, gfp_t priority);
#endif
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
#ifdef VMX86_TOOLS
static int VSockVmciStreamConnect(struct socket *sock,
                                  struct sockaddr *addr, int addrLen, int flags);
static int VSockVmciAccept(struct socket *sock, struct socket *newsock, int flags);
#endif
static int VSockVmciGetname(struct socket *sock,
                            struct sockaddr *addr, int *addrLen, int peer);
static unsigned int VSockVmciPoll(struct file *file,
                                  struct socket *sock, poll_table *wait);
#ifdef VMX86_TOOLS
static int VSockVmciListen(struct socket *sock, int backlog);
#endif
static int VSockVmciShutdown(struct socket *sock, int mode);

#ifdef VMX86_TOOLS
static int VSockVmciStreamSetsockopt(struct socket *sock, int level, int optname,
                                     char __user *optval, int optlen);
static int VSockVmciStreamGetsockopt(struct socket *sock, int level, int optname,
                                     char __user *optval, int __user * optlen);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 43)
static int VSockVmciDgramSendmsg(struct socket *sock, struct msghdr *msg,
                                 int len, struct scm_cookie *scm);
static int VSockVmciDgramRecvmsg(struct socket *sock, struct msghdr *msg,
                                 int len, int flags, struct scm_cookie *scm);
# ifdef VMX86_TOOLS
static int VSockVmciStreamSendmsg(struct socket *sock, struct msghdr *msg,
                                  int len, struct scm_cookie *scm);
static int VSockVmciStreamRecvmsg(struct socket *sock, struct msghdr *msg,
                                  int len, int flags, struct scm_cookie *scm);
# endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 65)
static int VSockVmciDgramSendmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, int len,
                                 struct scm_cookie *scm);
static int VSockVmciDgramRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, int len,
                                 int flags, struct scm_cookie *scm);
# ifdef VMX86_TOOLS
static int VSockVmciStreamSendmsg(struct kiocb *kiocb, struct socket *sock,
                                  struct msghdr *msg, int len,
                                  struct scm_cookie *scm);
static int VSockVmciStreamRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                  struct msghdr *msg, int len,
                                  int flags, struct scm_cookie *scm);
# endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
static int VSockVmciDgramSendmsg(struct kiocb *kiocb,
                                 struct socket *sock, struct msghdr *msg, int len);
static int VSockVmciDgramRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, int len, int flags);
# ifdef VMX86_TOOLS
static int VSockVmciStreamSendmsg(struct kiocb *kiocb,
                                  struct socket *sock, struct msghdr *msg, int len);
static int VSockVmciStreamRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                  struct msghdr *msg, int len, int flags);
# endif
#else
static int VSockVmciDgramSendmsg(struct kiocb *kiocb,
                                 struct socket *sock, struct msghdr *msg, size_t len);
static int VSockVmciDgramRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, size_t len, int flags);
# ifdef VMX86_TOOLS
static int VSockVmciStreamSendmsg(struct kiocb *kiocb,
                                 struct socket *sock, struct msghdr *msg, size_t len);
static int VSockVmciStreamRecvmsg(struct kiocb *kiocb, struct socket *sock,
                                 struct msghdr *msg, size_t len, int flags);
# endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int VSockVmciCreate(struct socket *sock, int protocol);
#else
static int VSockVmciCreate(struct net *net, struct socket *sock, int protocol);
#endif

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

/* Protocol family.  We only use this for builds against 2.6.9 and later. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
static struct proto vsockVmciProto = {
   .name     = "AF_VMCI",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
   /* Added in 2.6.10. */
   .owner    = THIS_MODULE,
#endif
   /*
    * Before 2.6.9, each address family created their own slab (by calling
    * kmem_cache_create() directly).  From 2.6.9 until 2.6.11, these address
    * families instead called sk_alloc_slab() and the allocated slab was
    * assigned to the slab variable in the proto struct and was created of size
    * slab_obj_size.  As of 2.6.12 and later, this slab allocation was moved
    * into proto_register() and only done if you specified a non-zero value for
    * the second argument (alloc_slab); the size of the slab element was
    * changed to obj_size.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   .slab_obj_size = sizeof (VSockVmciSock),
#else
   .obj_size = sizeof (VSockVmciSock),
#endif
};
#endif

static struct net_proto_family vsockVmciFamilyOps = {
   .family = VSOCK_INVALID_FAMILY,
   .create = VSockVmciCreate,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 69)
   .owner  = THIS_MODULE,
#endif
};

/* Socket operations, split for DGRAM and STREAM sockets. */
static struct proto_ops vsockVmciDgramOps = {
   .family     = VSOCK_INVALID_FAMILY,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 69)
   .owner      = THIS_MODULE,
#endif
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 4)
   .sendpage   = sock_no_sendpage,
#endif
};

#ifdef VMX86_TOOLS
static struct proto_ops vsockVmciStreamOps = {
   .family     = VSOCK_INVALID_FAMILY,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 69)
   .owner      = THIS_MODULE,
#endif
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 4)
   .sendpage   = sock_no_sendpage,
#endif
};
#endif

static struct file_operations vsockVmciDeviceOps = {
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

static DECLARE_MUTEX(registrationMutex);
static int devOpenCount = 0;
static int vsockVmciSocketCount = 0;
#ifdef VMX86_TOOLS
static VMCIHandle vmciStreamHandle = { VMCI_INVALID_ID, VMCI_INVALID_ID };
static Bool vmciDevicePresent = FALSE;
static VMCIId qpResumedSubId = VMCI_INVALID_ID;
#endif

/* Comment this out to compare with old protocol. */
#define VSOCK_OPTIMIZATION_WAITING_NOTIFY 1
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
/* Comment this out to remove flow control for "new" protocol */
#  define VSOCK_OPTIMIZATION_FLOW_CONTROL 1
#endif

/* Comment this out to turn off datagram counting. */
//#define VSOCK_CONTROL_PACKET_COUNT 1
#ifdef VSOCK_CONTROL_PACKET_COUNT
uint64 controlPacketCount[VSOCK_PACKET_TYPE_MAX];
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 5)
kmem_cache_t *vsockCachep;
#endif
#endif

#define VSOCK_MAX_DGRAM_RESENDS       10

/*
 * 64k is hopefully a reasonable default, but we should do some real
 * benchmarks. There are also some issues with resource limits on ESX.
 */
#define VSOCK_DEFAULT_QP_SIZE_MIN   128
#define VSOCK_DEFAULT_QP_SIZE       65536
#define VSOCK_DEFAULT_QP_SIZE_MAX   262144

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
                              0, 0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_NEGOTIATE(_sk, _size)                                \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_NEGOTIATE,            \
                           _size, 0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_QP_OFFER(_sk, _handle)                               \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_OFFER,                \
                           0, 0, NULL, _handle)
#define VSOCK_SEND_CONN_REQUEST(_sk, _size)                             \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_REQUEST,              \
                           _size, 0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_ATTACH(_sk, _handle)                                 \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_ATTACH,               \
                           0, 0, NULL, _handle)
#define VSOCK_SEND_WROTE(_sk)                                           \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WROTE,                \
                           0, 0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_READ(_sk)                                            \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_READ,                 \
                           0, 0, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_SHUTDOWN(_sk, _mode)                                 \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_SHUTDOWN,             \
                           0, _mode, NULL, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_WAITING_WRITE(_sk, _waitInfo)                        \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WAITING_WRITE,        \
                           0, 0, _waitInfo, VMCI_INVALID_HANDLE)
#define VSOCK_SEND_WAITING_READ(_sk, _waitInfo)                         \
   VSockVmciSendControlPkt(_sk, VSOCK_PACKET_TYPE_WAITING_READ,         \
                           0, 0, _waitInfo, VMCI_INVALID_HANDLE)


#ifdef VMX86_LOG
# define LOG_PACKET(_pkt)  VSockVmciLogPkt(__FUNCTION__, __LINE__, _pkt)
#else
# define LOG_PACKET(_pkt)
#endif


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

   down(&registrationMutex);

   afvalue = vsockVmciFamilyOps.family;
   if (!VSOCK_AF_IS_REGISTERED(afvalue)) {
      afvalue = VSockVmciRegisterAddressFamily();
   }

   up(&registrationMutex);
   return afvalue;
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

static inline void
VSockVmciTestUnregister(void)
{
   if (devOpenCount <= 0 && vsockVmciSocketCount <= 0) {
      if (VSOCK_AF_IS_REGISTERED(vsockVmciFamilyOps.family)) {
         VSockVmciUnregisterAddressFamily();
      }
   }
}


/*
 * Helper functions.
 */


#ifdef VMX86_TOOLS
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   Bool retval;
   uint64 notifyLimit;
  
   if (!vsk->peerWaitingWrite) {
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

   if (!vsk->peerWaitingWriteDetected) {
      vsk->peerWaitingWriteDetected = TRUE;
      vsk->writeNotifyWindow -= PAGE_SIZE;
      if (vsk->writeNotifyWindow < vsk->writeNotifyMinWindow) {
         vsk->writeNotifyWindow = vsk->writeNotifyMinWindow;
      }
   }
   notifyLimit = vsk->consumeSize - vsk->writeNotifyWindow;
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
   retval = VMCIQueue_FreeSpace(vsk->consumeQ, vsk->produceQ, vsk->consumeSize) >
            notifyLimit;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   if (retval) {
      /*
       * Once we notify the peer, we reset the detected flag so the
       * next wait will again cause a decrease in the window size.
       */

      vsk->peerWaitingWriteDetected = FALSE;
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
 *      Determines if the conditions have been met to notify a waiting reader.
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   if (!vsk->peerWaitingRead) {
      return FALSE;
   }

   /*
    * For now we ignore the wait information and just see if there is any data
    * to read.  Note that improving this function to be more intelligent will
    * not require a protocol change and will retain compatibility between
    * endpoints with mixed versions of this function.
    */
   return VMCIQueue_BufReady(vsk->produceQ,
                             vsk->consumeQ, vsk->produceSize) > 0;
#else
   return TRUE;
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);

   vsk->peerWaitingWrite = TRUE;
   memcpy(&vsk->peerWaitingWriteInfo, &pkt->u.wait,
          sizeof vsk->peerWaitingWriteInfo);

   if (VSockVmciNotifyWaitingWrite(vsk)) {
      Bool sent;

      if (bottomHalf) {
         sent = VSOCK_SEND_READ_BH(dst, src) > 0;
      } else {
         sent = VSOCK_SEND_READ(sk) > 0;
      }

      if (sent) {
         vsk->peerWaitingWrite = FALSE;
      }
   }
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   VSockVmciSock *vsk;

   vsk = vsock_sk(sk);

   vsk->peerWaitingRead = TRUE;
   memcpy(&vsk->peerWaitingReadInfo, &pkt->u.wait,
          sizeof vsk->peerWaitingReadInfo);

   if (VSockVmciNotifyWaitingRead(vsk)) {
      Bool sent;

      if (bottomHalf) {
         sent = VSOCK_SEND_WROTE_BH(dst, src) > 0;
      } else {
         sent = VSOCK_SEND_WROTE(sk) > 0;
      }

      if (sent) {
         vsk->peerWaitingRead = FALSE;
      }
   }
#endif
}
#endif


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
VSockVmciRecvDgramCB(void *data,           // IN
                     VMCIDatagram *dg)     // IN
{
   struct sock *sk;
   size_t size;
   struct sk_buff *skb;

   ASSERT(dg);
   ASSERT(dg->payloadSize <= VMCI_MAX_DG_PAYLOAD_SIZE);

   sk = (struct sock *)data;

   ASSERT(sk);
   /* XXX Figure out why sk->compat_sk_socket can be NULL. */
   ASSERT(sk->compat_sk_socket ? sk->compat_sk_socket->type == SOCK_DGRAM : 1);

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

   return 0;
}


#ifdef VMX86_TOOLS
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
   Bool processPkt;
   int err;

   ASSERT(dg);
   ASSERT(dg->payloadSize <= VMCI_MAX_DG_PAYLOAD_SIZE);

   sk = NULL;
   err = VMCI_SUCCESS;
   processPkt = TRUE;

   /*
    * Ignore incoming packets from contexts without sockets, or resources that
    * aren't vsock implementations.
    */
   if (!VSockAddr_SocketContext(VMCI_HANDLE_TO_CONTEXT_ID(dg->src)) ||
       VSOCK_PACKET_RID != VMCI_HANDLE_TO_RESOURCE_ID(dg->src)) {
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
      if (VSOCK_SEND_INVALID_BH(&dst, &src) < 0) {
         Warning("unable to send reply for invalid packet.\n");
         err = VMCI_ERROR_INVALID_ARGS;
         goto out;
      }
   }

   /*
    * We do most everything in a work queue, but let's fast path the
    * notification of reads and writes to help data transfer performance.  We
    * can only do this if there is no process context code executing for this
    * socket since that may change the state.
    */
   bh_lock_sock(sk);

   if (!compat_sock_owned_by_user(sk) && sk->compat_sk_state == SS_CONNECTED) {
      switch (pkt->type) {
      case VSOCK_PACKET_TYPE_WROTE:
         sk->compat_sk_data_ready(sk, 0);
         processPkt = FALSE;
         break;
      case VSOCK_PACKET_TYPE_READ:
         sk->compat_sk_write_space(sk);
         processPkt = FALSE;
         break;
      case VSOCK_PACKET_TYPE_WAITING_WRITE:
         VSockVmciHandleWaitingWrite(sk, pkt, TRUE, &dst, &src);
         processPkt = FALSE;
         break;

      case VSOCK_PACKET_TYPE_WAITING_READ:
         VSockVmciHandleWaitingRead(sk, pkt, TRUE, &dst, &src);
         processPkt = FALSE;
         break;
      }
   }

   bh_unlock_sock(sk);

   if (processPkt) {
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

static INLINE void
VSockVmciHandleDetach(struct sock *sk) // IN
{
   VSockVmciSock *vsk;

   ASSERT(sk);

   vsk = vsock_sk(sk);
   if (!VMCI_HANDLE_INVALID(vsk->qpHandle)) {
      ASSERT(vsk->produceQ);
      ASSERT(vsk->consumeQ);

      /* On a detach the peer will not be sending or receiving anymore. */
      vsk->peerShutdown = SHUTDOWN_MASK;

      /*
       * We should not be sending anymore since the peer won't be there to
       * receive, but we can still receive if there is data left in our consume
       * queue.
       */
      sk->compat_sk_shutdown |= SEND_SHUTDOWN;
      if (VMCIQueue_BufReady(vsk->consumeQ,
                             vsk->produceQ, vsk->consumeSize) <= 0) {
         sk->compat_sk_shutdown |= RCV_SHUTDOWN;
         sk->compat_sk_state = SS_UNCONNECTED;
      }
      sk->compat_sk_state_change(sk);
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

   /*
    * XXX This is lame, we should provide a way to lookup sockets by qpHandle.
    */
   bh_lock_sock(sk);

   if (VMCI_HANDLE_EQUAL(vsk->qpHandle, ePayload->handle)) {
      VSockVmciHandleDetach(sk);
   }

   bh_unlock_sock(sk);
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

   listener->compat_sk_ack_backlog--;

   /*
    * We need to remove ourself from the global connected sockets list so
    * incoming packets can't find this socket, and to reduce the reference
    * count.
    */
   if (VSockVmciInConnectedTable(sk)) {
      VSockVmciRemoveConnected(sk);
   }

   sk->compat_sk_state = SS_FREE;

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
   int err;
   VSockRecvPktInfo *recvPktInfo;
   VSockPacket *pkt;
   VSockVmciSock *vsk;
   struct sock *sk;

   recvPktInfo = COMPAT_WORK_GET_DATA(work, VSockRecvPktInfo);
   ASSERT(recvPktInfo);

   err = 0;
   sk = recvPktInfo->sk;
   pkt = &recvPktInfo->pkt;
   vsk = vsock_sk(sk);

   ASSERT(vsk);
   ASSERT(pkt);
   ASSERT(pkt->type < VSOCK_PACKET_TYPE_MAX);

   lock_sock(sk);

   switch (sk->compat_sk_state) {
   case SS_LISTEN:
      err = VSockVmciRecvListen(sk, pkt);
      break;
   case SS_UNCONNECTED:
      Log("packet received for socket in unconnected state; dropping.\n");
      goto out;
   case SS_CONNECTING:
      /*
       * Processing of pending connections for servers goes through the
       * listening socket, so see VSockVmciRecvListen() for that path.
       */
      err = VSockVmciRecvConnectingClient(sk, pkt);
      break;
   case SS_CONNECTED:
      err = VSockVmciRecvConnected(sk, pkt);
      break;
   case SS_DISCONNECTING:
      Log("packet receieved for socket in disconnecting state; dropping.\n");
      goto out;
   case SS_FREE:
      Log("packet receieved for socket in free state; dropping.\n");
      goto out;
   default:
      Log("socket is in invalid state; dropping packet.\n");
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
   VSockVmciSock *vsk;
   struct sock *pending;
   VSockVmciSock *vpending;
   int err;
   uint64 qpSize;

   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(sk->compat_sk_state == SS_LISTEN);

   vsk = vsock_sk(sk);
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
      switch (pending->compat_sk_state) {
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
   if (pkt->type != VSOCK_PACKET_TYPE_REQUEST ||
       pkt->u.size == 0) {
      VSOCK_SEND_RESET(sk, pkt);
      return -EINVAL;
   }

   /*
    * If this socket can't accommodate this connection request, we send
    * a reset.  Otherwise we create and initialize a child socket and reply
    * with a connection negotiation.
    */
   if (sk->compat_sk_ack_backlog >= sk->compat_sk_max_ack_backlog) {
      VSOCK_SEND_RESET(sk, pkt);
      return -ECONNREFUSED;
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   pending = __VSockVmciCreate(NULL, GFP_KERNEL);
#else
   pending = __VSockVmciCreate(compat_sock_net(sk), NULL, GFP_KERNEL);
#endif
   if (!pending) {
      VSOCK_SEND_RESET(sk, pkt);
      return -ENOMEM;
   }

   vpending = vsock_sk(pending);
   ASSERT(vpending);
   ASSERT(vsk->localAddr.svm_port == pkt->dstPort);

   VSockAddr_Init(&vpending->localAddr,
                  VMCI_GetContextID(),
                  pkt->dstPort);
   VSockAddr_Init(&vpending->remoteAddr,
                  VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                  pkt->srcPort);

   /*
    * If the proposed size fits within our min/max, accept
    * it. Otherwise propose our own size.
    */
   if (pkt->u.size >= vsk->queuePairMinSize &&
      pkt->u.size <= vsk->queuePairMaxSize) {
      qpSize = pkt->u.size;
   } else {
      qpSize = vsk->queuePairSize;
   }

   err = VSOCK_SEND_NEGOTIATE(pending, qpSize);
   if (err < 0) {
      VSOCK_SEND_RESET(sk, pkt);
      sock_put(pending);
      err = VSockVmci_ErrorToVSockError(err);
      goto out;
   }

   VSockVmciAddPending(sk, pending);
   sk->compat_sk_ack_backlog++;

   pending->compat_sk_state = SS_CONNECTING;
   vpending->produceSize = vpending->consumeSize =
                           vpending->writeNotifyWindow = pkt->u.size;
   

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
   VMCIQueue *produceQ;
   VMCIQueue *consumeQ;
   Bool isLocal;
   uint32 flags;
   VMCIId detachSubId;
   int err;
   int skerr;

   ASSERT(listener);
   ASSERT(pkt);
   ASSERT(listener->compat_sk_state == SS_LISTEN);
   ASSERT(pending->compat_sk_state == SS_CONNECTING);

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
   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_PEER_DETACH,
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
   isLocal = vpending->remoteAddr.svm_cid == vpending->localAddr.svm_cid;
   flags = VMCI_QPFLAG_ATTACH_ONLY;
   flags |= isLocal ? VMCI_QPFLAG_LOCAL : 0;

   err = VMCIQueuePair_Alloc(&handle,
                             &produceQ, vpending->produceSize,
                             &consumeQ, vpending->consumeSize,
                             VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src),
                             flags);
   if (err < 0) {
      /* We cannot complete this connection: send a reset and close. */
      Log("Could not attach to queue pair with %d\n", err);
      VSOCK_SEND_RESET(pending, pkt);
      err = VSockVmci_ErrorToVSockError(err);
      skerr = -err;
      goto destroy;
   }

   VMCIQueue_Init(handle, produceQ);

   ASSERT(VMCI_HANDLE_EQUAL(handle, pkt->u.handle));
   vpending->qpHandle = handle;
   vpending->produceQ = produceQ;
   vpending->consumeQ = consumeQ;

   /* Notify our peer of our attach. */
   err = VSOCK_SEND_ATTACH(pending, handle);
   if (err < 0) {
      Log("Could not send attach\n");
      VSOCK_SEND_RESET(pending, pkt);
      err = VSockVmci_ErrorToVSockError(err);
      skerr = -err;
      goto destroy;
   }

   /*
    * We have a connection.  Add our connection to the connected list so it no
    * longer goes through the listening socket, move it from the listener's
    * pending list to the accept queue so callers of accept() can find it.
    * Note that enqueueing the socket increments the reference count, so even
    * if a reset comes before the connection is accepted, the socket will be
    * valid until it is removed from the queue.
    */
   pending->compat_sk_state = SS_CONNECTED;

   VSockVmciInsertConnected(vsockConnectedSocketsVsk(vpending), pending);

   VSockVmciRemovePending(listener, pending);
   VSockVmciEnqueueAccept(listener, pending);

   /*
    * Callers of accept() will be be waiting on the listening socket, not the
    * pending socket.
    */
   listener->compat_sk_state_change(listener);

   return 0;

destroy:
   pending->compat_sk_err = skerr;
   pending->compat_sk_state = SS_UNCONNECTED;
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
   ASSERT(sk->compat_sk_state == SS_CONNECTING);

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
      sk->compat_sk_state = SS_CONNECTED;
      sk->compat_sk_socket->state = SS_CONNECTED;
      VSockVmciInsertConnected(vsockConnectedSocketsVsk(vsk), sk);
      sk->compat_sk_state_change(sk);
      break;
   case VSOCK_PACKET_TYPE_NEGOTIATE:
      if (pkt->u.size == 0 ||
          VMCI_HANDLE_TO_CONTEXT_ID(pkt->dg.src) != vsk->remoteAddr.svm_cid ||
          pkt->srcPort != vsk->remoteAddr.svm_port ||
          !VMCI_HANDLE_INVALID(vsk->qpHandle) ||
          vsk->produceQ ||
          vsk->consumeQ ||
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
   case VSOCK_PACKET_TYPE_RST:
      skerr = ECONNRESET;
      err = 0;
      goto destroy;
   default:
      /* Close and cleanup the connection. */
      skerr = EPROTO;
      err = -EINVAL;
      goto destroy;
   }

   ASSERT(pkt->type == VSOCK_PACKET_TYPE_ATTACH ||
          pkt->type == VSOCK_PACKET_TYPE_NEGOTIATE);

   return 0;

destroy:
   VSOCK_SEND_RESET(sk, pkt);

   sk->compat_sk_state = SS_UNCONNECTED;
   sk->compat_sk_err = skerr;
   sk->compat_sk_error_report(sk);
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
   VMCIQueue *produceQ;
   VMCIQueue *consumeQ;
   VMCIId attachSubId;
   VMCIId detachSubId;
   Bool isLocal;

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
   ASSERT(vsk->produceQ == NULL);
   ASSERT(vsk->consumeQ == NULL);
   ASSERT(vsk->produceSize == 0);
   ASSERT(vsk->consumeSize == 0);
   ASSERT(vsk->attachSubId == VMCI_INVALID_ID);
   ASSERT(vsk->detachSubId == VMCI_INVALID_ID);

   /* Verify that we're OK with the proposed queue pair size */
   if (pkt->u.size < vsk->queuePairMinSize ||
       pkt->u.size > vsk->queuePairMaxSize) {
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
   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_PEER_ATTACH,
                             VSockVmciPeerAttachCB,
                             sk,
                             &attachSubId);
   if (err < VMCI_SUCCESS) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_PEER_DETACH,
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

   err = VMCIQueuePair_Alloc(&handle,
                             &produceQ, pkt->u.size,
                             &consumeQ, pkt->u.size,
                             vsk->remoteAddr.svm_cid,
                             isLocal ? VMCI_QPFLAG_LOCAL : 0);
   if (err < VMCI_SUCCESS) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   VMCIQueue_Init(handle, produceQ);

   err = VSOCK_SEND_QP_OFFER(sk, handle);
   if (err < 0) {
      err = VSockVmci_ErrorToVSockError(err);
      goto destroy;
   }

   vsk->qpHandle = handle;
   vsk->produceQ = produceQ;
   vsk->consumeQ = consumeQ;
   vsk->produceSize = vsk->consumeSize = vsk->writeNotifyWindow = pkt->u.size;
   vsk->attachSubId = attachSubId;
   vsk->detachSubId = detachSubId;

   return 0;

destroy:
   if (attachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(attachSubId);
      ASSERT(vsk->attachSubId == VMCI_INVALID_ID);
   }

   if (detachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(detachSubId);
      ASSERT(vsk->detachSubId == VMCI_INVALID_ID);
   }

   if (!VMCI_HANDLE_INVALID(handle)) {
      VMCIQueuePair_Detach(handle);
      ASSERT(VMCI_HANDLE_INVALID(vsk->qpHandle));
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
   ASSERT(sk);
   ASSERT(pkt);
   ASSERT(sk->compat_sk_state == SS_CONNECTED);

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
         VSockVmciSock *vsk = vsock_sk(sk);

         vsk->peerShutdown |= pkt->u.mode;
         sk->compat_sk_state_change(sk);
      }
      break;

   case VSOCK_PACKET_TYPE_RST:
      sk->compat_sk_state = SS_DISCONNECTING;
      sk->compat_sk_shutdown = SHUTDOWN_MASK;
      sk->compat_sk_err = ECONNRESET;
      sk->compat_sk_error_report(sk);
      break;

   case VSOCK_PACKET_TYPE_WROTE:
      sk->compat_sk_data_ready(sk, 0);
      break;

   case VSOCK_PACKET_TYPE_READ:
      sk->compat_sk_write_space(sk);
      break;

   case VSOCK_PACKET_TYPE_WAITING_WRITE:
      VSockVmciHandleWaitingWrite(sk, pkt, FALSE, NULL, NULL);
      break;

   case VSOCK_PACKET_TYPE_WAITING_READ:
      VSockVmciHandleWaitingRead(sk, pkt, FALSE, NULL, NULL);
      break;

   default:
      return -EINVAL;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendControlPktBH --
 *
 *    Sends a control packet from bottom-half context.
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

static int
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

   VSockPacket_Init(&pkt, src, dst, type, size, mode, wait, handle);

   LOG_PACKET(&pkt);
#ifdef VSOCK_CONTROL_PACKET_COUNT
   controlPacketCount[pkt.type]++;
#endif
   return VMCIDatagram_Send(&pkt.dg);
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

static int
VSockVmciSendControlPkt(struct sock *sk,        // IN
                        VSockPacketType type,   // IN
                        uint64 size,            // IN
                        uint64 mode,            // IN
                        VSockWaitingInfo *wait, // IN
                        VMCIHandle handle)      // IN
{
   VSockPacket *pkt;
   VSockVmciSock *vsk;
   int err;

   ASSERT(sk);
   /*
    * New sockets for connection establishment won't have socket structures
    * yet; if one exists, ensure it is of the proper type.
    */
   ASSERT(sk->compat_sk_socket ?
             sk->compat_sk_socket->type == SOCK_STREAM :
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

   VSockPacket_Init(pkt, &vsk->localAddr, &vsk->remoteAddr,
                    type, size, mode, wait, handle);

   LOG_PACKET(pkt);
   err = VMCIDatagram_Send(&pkt->dg);
   kfree(pkt);
   if (err < 0) {
      return VSockVmci_ErrorToVSockError(err);
   }

#ifdef VSOCK_CONTROL_PACKET_COUNT
   controlPacketCount[pkt->type]++;
#endif

   return err;
}
#endif


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
   ASSERT(sk->compat_sk_socket);
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

   cid = VMCI_GetContextID();
   if (addr->svm_cid != cid &&
       addr->svm_cid != VMADDR_CID_ANY) {
      return -EADDRNOTAVAIL;
   }

   newAddr.svm_cid = cid;

   switch (sk->compat_sk_socket->type) {
   case SOCK_STREAM:
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
   case SOCK_DGRAM:
      /* VMCI will select a resource ID for us if we provide VMCI_INVALID_ID. */
      newAddr.svm_port = addr->svm_port == VMADDR_PORT_ANY ?
                            VMCI_INVALID_ID :
                            addr->svm_port;

      if (newAddr.svm_port <= LAST_RESERVED_PORT &&
          !capable(CAP_NET_BIND_SERVICE)) {
         err = -EACCES;
         goto out;
      }

      err = VMCIDatagram_CreateHnd(newAddr.svm_port, 0,
                                   VSockVmciRecvDgramCB, sk,
                                   &vsk->dgHandle);
      if (err != VMCI_SUCCESS ||
          vsk->dgHandle.context == VMCI_INVALID_ID ||
          vsk->dgHandle.resource == VMCI_INVALID_ID) {
         err = VSockVmci_ErrorToVSockError(err);
         goto out;
      }

      newAddr.svm_port = VMCI_HANDLE_TO_RESOURCE_ID(vsk->dgHandle);
      break;
   default:
      err = -EINVAL;
      goto out;
   }

   VSockAddr_Init(&vsk->localAddr, newAddr.svm_cid, newAddr.svm_port);

   /*
    * Remove stream sockets from the unbound list and add them to the hash
    * table for easy lookup by its address.  The unbound list is simply an
    * extra entry at the end of the hash table, a trick used by AF_UNIX.
    */
   if (sk->compat_sk_socket->type == SOCK_STREAM) {
      __VSockVmciRemoveBound(sk);
      __VSockVmciInsertBound(vsockBoundSockets(&vsk->localAddr), sk);
   }

   err = 0;

out:
   if (sk->compat_sk_socket->type == SOCK_STREAM) {
      spin_unlock_bh(&vsockTableLock);
   }
   return err;
}


#ifdef VMX86_TOOLS
/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciSendWaitingWrite --
 *
 *      Sends a waiting write notification to this socket's peer.
 *
 * Results:
 *      TRUE if the datagram is sent successfully, FALSE otherwise.
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   VSockVmciSock *vsk;
   VSockWaitingInfo waitingInfo;
   uint64 tail;
   uint64 head;
   uint64 roomLeft;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   VMCIQueue_GetPointers(vsk->produceQ, vsk->consumeQ, &tail, &head);
   roomLeft = vsk->produceSize - tail;
   if (roomNeeded + 1 >= roomLeft) {
      /* Wraps around to current generation. */
      waitingInfo.offset = roomNeeded + 1 - roomLeft;
      waitingInfo.generation = vsk->produceQGeneration;
   } else {
      waitingInfo.offset = tail + roomNeeded + 1;
      waitingInfo.generation = vsk->produceQGeneration - 1;
   }

   return VSOCK_SEND_WAITING_WRITE(sk, &waitingInfo) > 0;
#else
   return TRUE;
#endif
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
#ifdef VSOCK_OPTIMIZATION_WAITING_NOTIFY
   VSockVmciSock *vsk;
   VSockWaitingInfo waitingInfo;
   uint64 tail;
   uint64 head;
   uint64 roomLeft;

   ASSERT(sk);

   vsk = vsock_sk(sk);

   if (vsk->writeNotifyWindow < vsk->consumeSize) {
      vsk->writeNotifyWindow = MIN(vsk->writeNotifyWindow + PAGE_SIZE,
                                   vsk->consumeSize);
   }

   VMCIQueue_GetPointers(vsk->consumeQ, vsk->produceQ, &tail, &head);
   roomLeft = vsk->consumeSize - head;
   if (roomNeeded >= roomLeft) {
      waitingInfo.offset = roomNeeded - roomLeft;
      waitingInfo.generation = vsk->consumeQGeneration + 1;
   } else {
      waitingInfo.offset = head + roomNeeded;
      waitingInfo.generation = vsk->consumeQGeneration;
   }

   return VSOCK_SEND_WAITING_READ(sk, &waitingInfo) > 0;
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
         vsk->peerWaitingWrite = FALSE;
#endif
      }
   }
   return err;
}
#endif // VMX86_TOOLS


/*
 *----------------------------------------------------------------------------
 *
 * __VSockVmciCreate --
 *
 *    Does the work to create the sock structure.
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
                  unsigned int priority) // IN: Allocation flags
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static struct sock *
__VSockVmciCreate(struct socket *sock,   // IN: Owning socket, may be NULL
                  gfp_t priority)        // IN: Allocation flags
#else
static struct sock *
__VSockVmciCreate(struct net *net,       // IN: Network namespace
                  struct socket *sock,   // IN: Owning socket, may be NULL
                  gfp_t priority)        // IN: Allocation flags
#endif
{
   struct sock *sk;
   VSockVmciSock *vsk;

   vsk = NULL;

   /*
    * Before 2.5.5, sk_alloc() always used its own cache and protocol-specific
    * data was contained in the protinfo union.  We cannot use those other
    * structures so we allocate our own structure and attach it to the
    * user_data pointer that we don't otherwise need.  We must be sure to free
    * it later in our destruct routine.
    *
    * From 2.5.5 until 2.6.8, sk_alloc() offerred to use a cache that the
    * caller provided.  After this, the cache was moved into the proto
    * structure, but you still had to specify the size and cache yourself until
    * 2.6.12. Most recently (in 2.6.24), sk_alloc() was changed to expect the
    * network namespace, and the option to zero the sock was dropped.
    *
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   sk = sk_alloc(vsockVmciFamilyOps.family, priority, 1);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
   sk = sk_alloc(vsockVmciFamilyOps.family, priority,
                 sizeof (VSockVmciSock), vsockCachep);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   vsock_sk(sk) = kmalloc(sizeof *vsk, priority);
   if (!vsock_sk(sk)) {
      sk_free(sk);
      return NULL;
   }
   sk_vsock(vsock_sk(sk)) = sk;
#endif

   /*
    * If we go this far, we know the socket family is registered, so there's no
    * need to register it now.
    */
   down(&registrationMutex);
   vsockVmciSocketCount++;
   up(&registrationMutex);

   sock_init_data(sock, sk);

   vsk = vsock_sk(sk);
   VSockAddr_Init(&vsk->localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
   VSockAddr_Init(&vsk->remoteAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);

   sk->compat_sk_destruct = VSockVmciSkDestruct;
   sk->compat_sk_backlog_rcv = VSockVmciQueueRcvSkb;
   sk->compat_sk_state = SS_UNCONNECTED;

   INIT_LIST_HEAD(&vsk->boundTable);
   INIT_LIST_HEAD(&vsk->connectedTable);
   vsk->dgHandle = VMCI_INVALID_HANDLE;
#ifdef VMX86_TOOLS
   vsk->qpHandle = VMCI_INVALID_HANDLE;
   vsk->produceQ = vsk->consumeQ = NULL;
   vsk->produceQGeneration = vsk->consumeQGeneration = 0;
   vsk->produceSize = vsk->consumeSize = 0;
   vsk->writeNotifyWindow = 0;
   vsk->writeNotifyMinWindow = PAGE_SIZE;
   vsk->queuePairSize = VSOCK_DEFAULT_QP_SIZE;
   vsk->queuePairMinSize = VSOCK_DEFAULT_QP_SIZE_MIN;
   vsk->queuePairMaxSize = VSOCK_DEFAULT_QP_SIZE_MAX;
   vsk->peerWaitingRead = vsk->peerWaitingWrite = FALSE;
   vsk->peerWaitingWriteDetected = FALSE;
   memset(&vsk->peerWaitingReadInfo, 0, sizeof vsk->peerWaitingReadInfo);
   memset(&vsk->peerWaitingWriteInfo, 0, sizeof vsk->peerWaitingWriteInfo);
   vsk->listener = NULL;
   INIT_LIST_HEAD(&vsk->pendingLinks);
   INIT_LIST_HEAD(&vsk->acceptQueue);
   vsk->rejected = FALSE;
   vsk->attachSubId = vsk->detachSubId = VMCI_INVALID_ID;
   vsk->peerShutdown = 0;
#endif

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
         VMCIDatagram_DestroyHnd(vsk->dgHandle);
         vsk->dgHandle = VMCI_INVALID_HANDLE;
      }

      lock_sock(sk);
      sock_orphan(sk);
      sk->compat_sk_shutdown = SHUTDOWN_MASK;

      while ((skb = skb_dequeue(&sk->compat_sk_receive_queue))) {
         kfree_skb(skb);
      }

      /* Clean up any sockets that never were accepted. */
#ifdef VMX86_TOOLS
      while ((pending = VSockVmciDequeueAccept(sk)) != NULL) {
         __VSockVmciRelease(pending);
         sock_put(pending);
      }
#endif

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

#ifdef VMX86_TOOLS
   if (vsk->attachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(vsk->attachSubId);
      vsk->attachSubId = VMCI_INVALID_ID;
   }

   if (vsk->detachSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(vsk->detachSubId);
      vsk->detachSubId = VMCI_INVALID_ID;
   }

   if (!VMCI_HANDLE_INVALID(vsk->qpHandle)) {
      VMCIQueuePair_Detach(vsk->qpHandle);
      vsk->qpHandle = VMCI_INVALID_HANDLE;
      vsk->produceQ = vsk->consumeQ = NULL;
      vsk->produceSize = vsk->consumeSize = 0;
   }
#endif

   /*
    * Each list entry holds a reference on the socket, so we should not even be
    * here if the socket is in one of our lists.  If we are we have a stray
    * sock_put() that needs to go away.
    */
   ASSERT(!VSockVmciInBoundTable(sk));
   ASSERT(!VSockVmciInConnectedTable(sk));
#ifdef VMX86_TOOLS
   ASSERT(!VSockVmciIsPending(sk));
   ASSERT(!VSockVmciInAcceptQueue(sk));
#endif

   /*
    * When clearing these addresses, there's no need to set the family and
    * possibly register the address family with the kernel.
    */
   VSockAddr_InitNoFamily(&vsk->localAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);
   VSockAddr_InitNoFamily(&vsk->remoteAddr, VMADDR_CID_ANY, VMADDR_PORT_ANY);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   ASSERT(vsock_sk(sk) == vsk);
   kfree(vsock_sk(sk));
#endif

   down(&registrationMutex);
   vsockVmciSocketCount--;
   VSockVmciTestUnregister();
   up(&registrationMutex);

#ifdef VSOCK_CONTROL_PACKET_COUNT
   {
      uint32 index;
      for (index = 0; index < ARRAYSIZE(controlPacketCount); index++) {
         Warning("Control packet count: Type = %u, Count = %"FMT64"u\n",
                 index, controlPacketCount[index]);
      }
   }
#endif
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
 *      Registers the vmci sockets protocol family.
 *
 * Results:
 *      Zero on success, error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE int
VSockVmciRegisterProto(void)
{
   int err;

   err = 0;

   /*
    * Before 2.6.9, each address family created their own slab (by calling
    * kmem_cache_create() directly).  From 2.6.9 until 2.6.11, these address
    * families instead called sk_alloc_slab() and the allocated slab was
    * assigned to the slab variable in the proto struct and was created of size
    * slab_obj_size.  As of 2.6.12 and later, this slab allocation was moved
    * into proto_register() and only done if you specified a non-zero value for
    * the second argument (alloc_slab); the size of the slab element was
    * changed to obj_size.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   /* Simply here for clarity and so else case at end implies > rest. */
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
   vsockCachep = kmem_cache_create("vsock", sizeof (VSockVmciSock),
                                   0, SLAB_HWCACHE_ALIGN, NULL, NULL);
   if (!vsockCachep) {
      err = -ENOMEM;
   }
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
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
 *      Unregisters the vmci sockets protocol family.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciUnregisterProto(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 5)
   /* Simply here for clarity and so else case at end implies > rest. */
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
   kmem_cache_destroy(vsockCachep);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
   sk_free_slab(&vsockVmciProto);
#else
   proto_unregister(&vsockVmciProto);
#endif

#ifdef VSOCK_CONTROL_PACKET_COUNT
   {
      uint32 index;
      for (index = 0; index < ARRAYSIZE(controlPacketCount); index++) {
         controlPacketCount[index] = 0;
      }
   }
#endif
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

#ifdef VMX86_TOOLS
   /*
    * We don't call into the vmci module or register our socket family if the
    * vmci device isn't present.
    */
   vmciDevicePresent = VMCI_DeviceGet();
   if (!vmciDevicePresent) {
      Log("Could not register VMCI Sockets because VMCI device is not present.\n");
      return -1;
   }

   /*
    * Create the datagram handle that we will use to send and receive all
    * VSocket control messages for this context.
    */
   err = VMCIDatagram_CreateHnd(VSOCK_PACKET_RID, 0,
                                VSockVmciRecvStreamCB, NULL, &vmciStreamHandle);
   if (err != VMCI_SUCCESS ||
       vmciStreamHandle.context == VMCI_INVALID_ID ||
       vmciStreamHandle.resource == VMCI_INVALID_ID) {
      Warning("Unable to create datagram handle. (%d)\n", err);
      return -ENOMEM;
   }

   err = VMCIEvent_Subscribe(VMCI_EVENT_QP_RESUMED,
                             VSockVmciQPResumedCB,
                             NULL,
                             &qpResumedSubId);
   if (err < VMCI_SUCCESS) {
      Warning("Unable to subscribe to QP resumed event. (%d)\n", err);
      err = -ENOMEM;
      qpResumedSubId = VMCI_INVALID_ID;
      goto error;
   }
#endif

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
         Warning("Could not register address family %d.\n", i);
         vsockVmciFamilyOps.family = VSOCK_INVALID_FAMILY;
      } else {
         vsockVmciDgramOps.family = i;
#ifdef VMX86_TOOLS
         vsockVmciStreamOps.family = i;
#endif
         break;
      }
   }

   if (err) {
      goto error;
   }

   return vsockVmciFamilyOps.family;

error:
#ifdef VMX86_TOOLS
   if (qpResumedSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(qpResumedSubId);
      qpResumedSubId = VMCI_INVALID_ID;
   }
   VMCIDatagram_DestroyHnd(vmciStreamHandle);
#endif
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
#ifdef VMX86_TOOLS
   if (!vmciDevicePresent) {
      /* Nothing was registered. */
      return;
   }

   if (!VMCI_HANDLE_INVALID(vmciStreamHandle)) {
      if (VMCIDatagram_DestroyHnd(vmciStreamHandle) != VMCI_SUCCESS) {
         Warning("Could not destroy VMCI datagram handle.\n");
      }
   }

   if (qpResumedSubId != VMCI_INVALID_ID) {
      VMCIEvent_Unsubscribe(qpResumedSubId);
      qpResumedSubId = VMCI_INVALID_ID;
   }
#endif

   if (vsockVmciFamilyOps.family != VSOCK_INVALID_FAMILY) {
      sock_unregister(vsockVmciFamilyOps.family);
   }

   vsockVmciDgramOps.family = vsockVmciFamilyOps.family = VSOCK_INVALID_FAMILY;
#ifdef VMX86_TOOLS
   vsockVmciStreamOps.family = vsockVmciFamilyOps.family;
#endif

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

   memcpy(&vsk->remoteAddr, remoteAddr, sizeof vsk->remoteAddr);
   sock->state = SS_CONNECTED;

out:
   release_sock(sk);
   return err;
}


#ifdef VMX86_TOOLS
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
   COMPAT_DEFINE_WAIT(wait);

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
   case SS_LISTEN:
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
      ASSERT(sk->compat_sk_state == SS_FREE ||
             sk->compat_sk_state == SS_UNCONNECTED);
      if (VSockAddr_Cast(addr, addrLen, &remoteAddr) != 0) {
         err = -EINVAL;
         goto out;
      }

      /* The hypervisor and well-known contexts do not have socket endpoints. */
      if (!VSockAddr_SocketContext(remoteAddr->svm_cid)) {
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

      sk->compat_sk_state = SS_CONNECTING;

      err = VSOCK_SEND_CONN_REQUEST(sk, vsk->queuePairSize);
      if (err < 0) {
         sk->compat_sk_state = SS_UNCONNECTED;
         goto out;
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
   timeout = sock_sndtimeo(sk, flags & O_NONBLOCK);
   compat_init_prepare_to_wait(sk->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);

   while (sk->compat_sk_state != SS_CONNECTED && sk->compat_sk_err == 0) {
      if (timeout == 0) {
         /*
          * If we're not going to block, skip ahead to preserve error code set
          * above.
          */
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

      compat_cont_prepare_to_wait(sk->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);
   }

   if (sk->compat_sk_err) {
      err = -sk->compat_sk_err;
      goto outWaitError;
   } else {
      ASSERT(sk->compat_sk_state == SS_CONNECTED);
      err = 0;
   }

outWait:
   compat_finish_wait(sk->compat_sk_sleep, &wait, TASK_RUNNING);
out:
   release_sock(sk);
   return err;

outWaitError:
   sk->compat_sk_state = SS_UNCONNECTED;
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
   COMPAT_DEFINE_WAIT(wait);

   err = 0;
   listener = sock->sk;

   lock_sock(listener);

   if (sock->type != SOCK_STREAM) {
      err = -EOPNOTSUPP;
      goto out;
   }

   if (listener->compat_sk_state != SS_LISTEN) {
      err = -EINVAL;
      goto out;
   }

   /*
    * Wait for children sockets to appear; these are the new sockets created
    * upon connection establishment.
    */
   timeout = sock_sndtimeo(listener, flags & O_NONBLOCK);
   compat_init_prepare_to_wait(listener->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);

   while ((connected = VSockVmciDequeueAccept(listener)) == NULL &&
          listener->compat_sk_err == 0) {
      release_sock(listener);
      timeout = schedule_timeout(timeout);
      lock_sock(listener);

      if (signal_pending(current)) {
         err = sock_intr_errno(timeout);
         goto outWait;
      } else if (timeout == 0) {
         err = -ETIMEDOUT;
         goto outWait;
      }

      compat_cont_prepare_to_wait(listener->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);
   }

   if (listener->compat_sk_err) {
      err = -listener->compat_sk_err;
   }

   if (connected) {
      listener->compat_sk_ack_backlog--;

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
   compat_finish_wait(listener->compat_sk_sleep, &wait, TASK_RUNNING);
out:
   release_sock(listener);
   return err;
}
#endif


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

   sk = sock->sk;

   poll_wait(file, sk->compat_sk_sleep, wait);
   mask = 0;

   if (sk->compat_sk_err) {
      mask |= POLLERR;
   }

   if (sk->compat_sk_shutdown == SHUTDOWN_MASK) {
      mask |= POLLHUP;
   }

   /* POLLRDHUP wasn't added until 2.6.17. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
   if (sk->compat_sk_shutdown & RCV_SHUTDOWN) {
      mask |= POLLRDHUP;
   }
#endif

   if (sock->type == SOCK_DGRAM) {
      /*
       * For datagram sockets we can read if there is something in the queue
       * and write as long as the socket isn't shutdown for sending.
       */
      if (!skb_queue_empty(&sk->compat_sk_receive_queue) ||
          (sk->compat_sk_shutdown & RCV_SHUTDOWN)) {
         mask |= POLLIN | POLLRDNORM;
      }

      if (!(sk->compat_sk_shutdown & SEND_SHUTDOWN)) {
         mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
      }
#ifdef VMX86_TOOLS
   } else if (sock->type == SOCK_STREAM) {
      VSockVmciSock *vsk;

      lock_sock(sk);

      vsk = vsock_sk(sk);

      /*
       * Listening sockets that have connections in their accept queue and
       * connected sockets that have consumable data can be read.  Sockets
       * whose connections have been close, reset, or terminated should also be
       * considered read, and we check the shutdown flag for that.
       */
      if ((sk->compat_sk_state == SS_LISTEN &&
           !VSockVmciIsAcceptQueueEmpty(sk)) ||
          (!VMCI_HANDLE_INVALID(vsk->qpHandle) &&
           !(sk->compat_sk_shutdown & RCV_SHUTDOWN) &&
           VMCIQueue_BufReady(vsk->consumeQ,
                              vsk->produceQ, vsk->consumeSize)) ||
           sk->compat_sk_shutdown) {
          mask |= POLLIN | POLLRDNORM;
      }

      /*
       * Connected sockets that can produce data can be written.
       */
      if (sk->compat_sk_state == SS_CONNECTED &&
          !(sk->compat_sk_shutdown & SEND_SHUTDOWN) &&
          VMCIQueue_FreeSpace(vsk->produceQ,
                              vsk->consumeQ, vsk->produceSize) > 0) {
         mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
      }

      /*
       * Connected sockets also need to notify their peer that they are
       * waiting.  Optimally these calls would happen in the code that decides
       * whether the caller will wait or not, but that's core kernel code and
       * this is the best we can do.  If the caller doesn't sleep, the worst
       * that happens is a few extra datagrams are sent.
       */
      if (sk->compat_sk_state == SS_CONNECTED) {
         VSockVmciSendWaitingWrite(sk, 1);
         VSockVmciSendWaitingRead(sk, 1);
      }

      release_sock(sk);
#endif
   }

   return mask;
}


#ifdef VMX86_TOOLS
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

   sk->compat_sk_max_ack_backlog = backlog;
   sk->compat_sk_state = SS_LISTEN;

   err = 0;

out:
   release_sock(sk);
   return err;
}
#endif


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

   if (sock->state == SS_UNCONNECTED) {
      return -ENOTCONN;
   }

   sk = sock->sk;
   sock->state = SS_DISCONNECTING;

   /* Receive and send shutdowns are treated alike. */
   mode = mode & (RCV_SHUTDOWN | SEND_SHUTDOWN);
   if (mode) {
      lock_sock(sk);
      sk->compat_sk_shutdown |= mode;
      sk->compat_sk_state_change(sk);
      release_sock(sk);
   }

#ifdef VMX86_TOOLS
   if (sk->compat_sk_type == SOCK_STREAM && mode) {
      VSOCK_SEND_SHUTDOWN(sk, mode);
   }
#endif

   return 0;
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 43)
static int
VSockVmciDgramSendmsg(struct socket *sock,          // IN: socket to send on
                      struct msghdr *msg,           // IN: message to send
                      int len,                      // IN: length of message
                      struct scm_cookie *scm)       // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 65)
static int
VSockVmciDgramSendmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to send on
                      struct msghdr *msg,           // IN: message to send
                      int len,                      // IN: length of message
                      struct scm_cookie *scm);      // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
static int
VSockVmciDgramSendmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to send on
                      struct msghdr *msg,           // IN: message to send
                      int len)                      // IN: length of message
#else
static int
VSockVmciDgramSendmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to send on
                      struct msghdr *msg,           // IN: message to send
                      size_t len)                   // IN: length of message
#endif
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
         remoteAddr->svm_cid = VMCI_GetContextID();
      }

      if (!VSockAddr_Bound(remoteAddr)) {
         err = -EINVAL;
         goto out;
      }
   } else if (sock->state == SS_CONNECTED) {
      remoteAddr = &vsk->remoteAddr;
      // XXXAB Temporary to handle test program
      if (remoteAddr->svm_cid == VMADDR_CID_ANY) {
         remoteAddr->svm_cid = VMCI_GetContextID();
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

   err = VMCIDatagram_Send(dg);
   kfree(dg);
   if (err < 0) {
      err = VSockVmci_ErrorToVSockError(err);
      goto out;
   }

   /*
    * err is the number of bytes sent on success.  We need to subtract the
    * VSock-specific header portions of what we've sent.
    */
   err -= sizeof *dg;

out:
   release_sock(sk);
   return err;
}

#ifdef VMX86_TOOLS
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
VSockVmciStreamSetsockopt(struct socket *sock,       // IN/OUT
                          int level,                 // IN
                          int optname,               // IN
                          char __user *optval,       // IN
                          int optlen)                // IN
{
   int err;
   struct sock *sk;
   VSockVmciSock *vsk;
   uint64 val;

   if (level != VSockVmci_GetAFValue()) {
      return -ENOPROTOOPT;
   }

   if (optlen < sizeof val) {
      return -EINVAL;
   }

   if (copy_from_user(&val, optval, sizeof val) != 0) {
      return -EFAULT;
   }

   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   ASSERT(vsk->queuePairMinSize <= vsk->queuePairSize &&
          vsk->queuePairSize <= vsk->queuePairMaxSize);

   lock_sock(sk);

   switch (optname) {
   case SO_VMCI_BUFFER_SIZE:
      if (val < vsk->queuePairMinSize || val > vsk->queuePairMaxSize) {
         err = -EINVAL;
         goto out;
      }
      vsk->queuePairSize = val;
      break;

   case SO_VMCI_BUFFER_MAX_SIZE:
      if (val < vsk->queuePairSize) {
         err = -EINVAL;
         goto out;
      }
      vsk->queuePairMaxSize = val;
      break;

   case SO_VMCI_BUFFER_MIN_SIZE:
      if (val > vsk->queuePairSize) {
         err = -EINVAL;
         goto out;
      }
      vsk->queuePairMinSize = val;
      break;

   default:
      err = -ENOPROTOOPT;
      break;
   }

out:

   ASSERT(vsk->queuePairMinSize <= vsk->queuePairSize &&
          vsk->queuePairSize <= vsk->queuePairMaxSize);

   release_sock(sk);
   return err;
}

#endif

#ifdef VMX86_TOOLS
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
   uint64 val;

   if (level != VSockVmci_GetAFValue()) {
      return -ENOPROTOOPT;
   }

   if ((err = get_user(len, optlen)) != 0) {
      return err;
   }
   if (len < sizeof val) {
      return -EINVAL;
   }

   len = sizeof val;

   err = 0;
   sk = sock->sk;
   vsk = vsock_sk(sk);

   switch (optname) {
   case SO_VMCI_BUFFER_SIZE:
      val = vsk->queuePairSize;
      break;

   case SO_VMCI_BUFFER_MAX_SIZE:
      val = vsk->queuePairMaxSize;
      break;

   case SO_VMCI_BUFFER_MIN_SIZE:
      val = vsk->queuePairMinSize;
      break;

   default:
      return -ENOPROTOOPT;
   }

   if ((err = put_user(val, (uint64 __user *)optval)) != 0) {
      return err;
   }
   if ((err = put_user(len, optlen)) != 0) {
      return err;
   }
   return 0;
}
#endif


#ifdef VMX86_TOOLS
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 43)
static int
VSockVmciStreamSendmsg(struct socket *sock,          // IN: socket to send on
                       struct msghdr *msg,           // IN: message to send
                       int len,                      // IN: length of message
                       struct scm_cookie *scm)       // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 65)
static int
VSockVmciStreamSendmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to send on
                       struct msghdr *msg,           // IN: message to send
                       int len,                      // IN: length of message
                       struct scm_cookie *scm);      // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
static int
VSockVmciStreamSendmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to send on
                       struct msghdr *msg,           // IN: message to send
                       int len)                      // IN: length of message
#else
static int
VSockVmciStreamSendmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to send on
                       struct msghdr *msg,           // IN: message to send
                       size_t len)                   // IN: length of message
#endif
{
   struct sock *sk;
   VSockVmciSock *vsk;
   ssize_t totalWritten;
   long timeout;
   int err;
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   uint64 produceTail;
   uint64 consumeHead;
#endif
   COMPAT_DEFINE_WAIT(wait);

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
      err = sk->compat_sk_state == SS_CONNECTED ? -EISCONN : -EOPNOTSUPP;
      goto out;
   }

   if (sk->compat_sk_shutdown & SEND_SHUTDOWN) {
      err = -EPIPE;
      goto out;
   }

   if (sk->compat_sk_state != SS_CONNECTED ||
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
   compat_init_prepare_to_wait(sk->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);

   while (totalWritten < len) {
      Bool sentWrote;
      unsigned int retries;
      ssize_t written;

      sentWrote = FALSE;
      retries = 0;

      while (VMCIQueue_FreeSpace(vsk->produceQ,
                                 vsk->consumeQ, vsk->produceSize) == 0 &&
             sk->compat_sk_err == 0 &&
             !(sk->compat_sk_shutdown & SEND_SHUTDOWN) &&
             !(vsk->peerShutdown & RCV_SHUTDOWN)) {

         /* Don't wait for non-blocking sockets. */
         if (timeout == 0) {
            err = -EAGAIN;
            goto outWait;
         }

         /* Notify our peer that we are waiting for room to write. */
         if (!VSockVmciSendWaitingWrite(sk, 1)) {
            err = -EHOSTUNREACH;
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

         compat_cont_prepare_to_wait(sk->compat_sk_sleep,
                                     &wait, TASK_INTERRUPTIBLE);
      }

      /*
       * These checks occur both as part of and after the loop conditional
       * since we need to check before and after sleeping.
       */
      if (sk->compat_sk_err) {
         err = -sk->compat_sk_err;
         goto outWait;
      } else if ((sk->compat_sk_shutdown & SEND_SHUTDOWN) ||
                 (vsk->peerShutdown & RCV_SHUTDOWN)) {
         err = -EPIPE;
         goto outWait;
      }

      /*
       * Note that enqueue will only write as many bytes as are free in the
       * produce queue, so we don't need to ensure len is smaller than the queue
       * size.  It is the caller's responsibility to check how many bytes we were
       * able to send.
       */
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
      VMCIQueue_GetPointers(vsk->produceQ, vsk->consumeQ,
                            &produceTail, &consumeHead);
#endif

      written = VMCIQueue_EnqueueV(vsk->produceQ, vsk->consumeQ,
                                   vsk->produceSize, msg->msg_iov,
                                   len - totalWritten);
      if (written < 0) {
         err = -ENOMEM;
         goto outWait;
      }

#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
      /*
       * Detect a wrap-around to maintain queue generation.  Note that this is
       * safe since we hold the socket lock across the two queue pair
       * operations.
       */
      if (written >= vsk->produceSize - produceTail) {
         vsk->produceQGeneration++;
      }
#endif

      totalWritten += written;

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
            goto outWait;
         } else {
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
            vsk->peerWaitingRead = FALSE;
#endif
         }
      }
   }

   ASSERT(totalWritten <= INT_MAX);

outWait:
   if (totalWritten > 0) {
      err = totalWritten;
   }
   compat_finish_wait(sk->compat_sk_sleep, &wait, TASK_RUNNING);
out:
   release_sock(sk);
   return err;
}
#endif


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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 43)
static int
VSockVmciDgramRecvmsg(struct socket *sock,           // IN: socket to receive from
                      struct msghdr *msg,            // IN/OUT: message to receive into
                      int len,                       // IN: length of receive buffer
                      int flags,                     // IN: receive flags
                      struct scm_cookie *scm)        // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 65)
static int
VSockVmciDgramRecvmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to receive from
                      struct msghdr *msg,           // IN/OUT: message to receive into
                      int len,                      // IN: length of receive buffer
                      int flags,                    // IN: receive flags
                      struct scm_cookie *scm)       // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
static int
VSockVmciDgramRecvmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to receive from
                      struct msghdr *msg,           // IN/OUT: message to receive into
                      int len,                      // IN: length of receive buffer
                      int flags)                    // IN: receive flags
#else
static int
VSockVmciDgramRecvmsg(struct kiocb *kiocb,          // UNUSED
                      struct socket *sock,          // IN: socket to receive from
                      struct msghdr *msg,           // IN/OUT: message to receive into
                      size_t len,                   // IN: length of receive buffer
                      int flags)                    // IN: receive flags
#endif
{
   int err;
   int noblock;
   struct sock *sk;
   VMCIDatagram *dg;
   size_t payloadLen;
   struct sk_buff *skb;
   struct sockaddr_vm *vmciAddr;

   err = 0;
   sk = sock->sk;
   payloadLen = 0;
   noblock = flags & MSG_DONTWAIT;
   vmciAddr = (struct sockaddr_vm *)msg->msg_name;

   if (flags & MSG_OOB || flags & MSG_ERRQUEUE) {
      return -EOPNOTSUPP;
   }

   /* Retrieve the head sk_buff from the socket's receive queue. */
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
   if (vmciAddr) {
      /* Provide the address of the sender. */
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


#ifdef VMX86_TOOLS
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 43)
static int
VSockVmciStreamRecvmsg(struct socket *sock,           // IN: socket to receive from
                       struct msghdr *msg,            // IN/OUT: message to receive into
                       int len,                       // IN: length of receive buffer
                       int flags,                     // IN: receive flags
                       struct scm_cookie *scm)        // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 65)
static int
VSockVmciStreamRecvmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to receive from
                       struct msghdr *msg,           // IN/OUT: message to receive into
                       int len,                      // IN: length of receive buffer
                       int flags,                    // IN: receive flags
                       struct scm_cookie *scm)       // UNUSED
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
static int
VSockVmciStreamRecvmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to receive from
                       struct msghdr *msg,           // IN/OUT: message to receive into
                       int len,                      // IN: length of receive buffer
                       int flags)                    // IN: receive flags
#else
static int
VSockVmciStreamRecvmsg(struct kiocb *kiocb,          // UNUSED
                       struct socket *sock,          // IN: socket to receive from
                       struct msghdr *msg,           // IN/OUT: message to receive into
                       size_t len,                   // IN: length of receive buffer
                       int flags)                    // IN: receive flags
#endif
{
   struct sock *sk;
   VSockVmciSock *vsk;
   int err;
   int target;
   int64 ready;
   long timeout;
   ssize_t copied;
   Bool sentRead;
   unsigned int retries;
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   uint64 consumeHead;
   uint64 produceTail;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   Bool notifyOnBlock;
#endif
#endif

   COMPAT_DEFINE_WAIT(wait);

   sk = sock->sk;
   vsk = vsock_sk(sk);
   err = 0;
   retries = 0;
   sentRead = FALSE;
#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   notifyOnBlock = FALSE;
#endif

   lock_sock(sk);

   if (sk->compat_sk_state != SS_CONNECTED) {
      err = -ENOTCONN;
      goto out;
   }

   if (flags & MSG_OOB) {
      err = -EOPNOTSUPP;
      goto out;
   }

   if (sk->compat_sk_shutdown & RCV_SHUTDOWN) {
      err = -EPIPE;
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

#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
   if (vsk->writeNotifyMinWindow < target + 1) {
      ASSERT(target < vsk->consumeSize);
      vsk->writeNotifyMinWindow = target + 1;
      if (vsk->writeNotifyWindow < vsk->writeNotifyMinWindow) {
         /*
          * If the current window is smaller than the new minimal
          * window size, we need to reevaluate whether we need to
          * notify the sender. If the number of ready bytes are
          * smaller than the new window, we need to send a
          * notification to the sender before we block.
          */

         vsk->writeNotifyWindow = vsk->writeNotifyMinWindow;
         notifyOnBlock = TRUE;
      }
   }
#endif

   compat_init_prepare_to_wait(sk->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);

   while ((ready = VMCIQueue_BufReady(vsk->consumeQ,
                                      vsk->produceQ,
                                      vsk->consumeSize)) < target &&
          sk->compat_sk_err == 0 &&
          !(sk->compat_sk_shutdown & RCV_SHUTDOWN) &&
          !(vsk->peerShutdown & SEND_SHUTDOWN)) {

      if (ready < 0) {
         /* 
          * Invalid queue pair content. XXX This should be changed to
          * a connection reset in a later change.
          */

         err = -ENOMEM;
         goto out;
      }

      /* Don't wait for non-blocking sockets. */
      if (timeout == 0) {
         err = -EAGAIN;
         goto outWait;
      }

      /* Notify our peer that we are waiting for data to read. */
      if (!VSockVmciSendWaitingRead(sk, target)) {
         err = -EHOSTUNREACH;
         goto outWait;
      }

#ifdef VSOCK_OPTIMIZATION_FLOW_CONTROL
      if (notifyOnBlock) {
         err = VSockVmciSendReadNotification(sk);
         if (err < 0) {
            goto outWait;
         }
         notifyOnBlock = FALSE;
      }
#endif

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

      compat_cont_prepare_to_wait(sk->compat_sk_sleep, &wait, TASK_INTERRUPTIBLE);
   }

   if (sk->compat_sk_err) {
      err = -sk->compat_sk_err;
      goto outWait;
   } else if (sk->compat_sk_shutdown & RCV_SHUTDOWN) {
      err = 0;
      goto outWait;
   } else if ((vsk->peerShutdown & SEND_SHUTDOWN) &&
              VMCIQueue_BufReady(vsk->consumeQ,
                                 vsk->produceQ, vsk->consumeSize) < target) {
      err = -EPIPE;
      goto outWait;
   }

   /*
    * Now consume up to len bytes from the queue.  Note that since we have the
    * socket locked we should copy at least ready bytes.
    */
#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   VMCIQueue_GetPointers(vsk->consumeQ, vsk->produceQ,
                         &produceTail, &consumeHead);
#endif

   copied = VMCIQueue_DequeueV(vsk->produceQ, vsk->consumeQ,
                               vsk->consumeSize, msg->msg_iov, len);
   if (copied < 0) {
      err = -ENOMEM;
      goto outWait;
   }

#if defined(VMX86_TOOLS) && defined(VSOCK_OPTIMIZATION_WAITING_NOTIFY)
   /*
    * Detect a wrap-around to maintain queue generation.  Note that this is
    * safe since we hold the socket lock across the two queue pair
    * operations.
    */
   if (copied >= vsk->consumeSize - consumeHead) {
      vsk->consumeQGeneration++;
   }
#endif

   ASSERT(copied >= target);

   /*
    * If the other side has shutdown for sending and there is nothing more to
    * read, then set our socket's RCV_SHUTDOWN flag and modify the socket
    * state.
    */
   if (vsk->peerShutdown & SEND_SHUTDOWN) {
      if (VMCIQueue_BufReady(vsk->consumeQ,
                             vsk->produceQ, vsk->consumeSize) <= 0) {
         sk->compat_sk_shutdown |= RCV_SHUTDOWN;
         sk->compat_sk_state = SS_UNCONNECTED;
         sk->compat_sk_state_change(sk);
      }
   }

   err = VSockVmciSendReadNotification(sk);
   if (err < 0) {
      goto outWait;
   }

   ASSERT(copied <= INT_MAX);
   err = copied;

outWait:
   compat_finish_wait(sk->compat_sk_sleep, &wait, TASK_RUNNING);
out:
   release_sock(sk);
   return err;
}
#endif


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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int
VSockVmciCreate(struct socket *sock,  // IN
                int protocol)         // IN
#else
static int
VSockVmciCreate(struct net *net,      // IN
                struct socket *sock,  // IN
                int protocol)         // IN
#endif
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
#  ifdef VMX86_TOOLS
   /*
    * Queue pairs are /currently/ only supported within guests, so stream
    * sockets are only supported within guests.
    */
   case SOCK_STREAM:
      sock->ops = &vsockVmciStreamOps;
      break;
#  endif
   default:
      return -ESOCKTNOSUPPORT;
   }

   sock->state = SS_UNCONNECTED;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   return __VSockVmciCreate(sock, GFP_KERNEL) ? 0 : -ENOMEM;
#else
   return __VSockVmciCreate(net, sock, GFP_KERNEL) ? 0 : -ENOMEM;
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 26) || \
   (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 3))
   lock_kernel();
#endif
   ret = -ENOTTY;
   if (filp && filp->f_op && filp->f_op->ioctl == VSockVmciDevIoctl) {
      ret = VSockVmciDevIoctl(filp->f_dentry->d_inode, filp, iocmd, ioarg);
   }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 26) || \
   (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 3))
   unlock_kernel();
#endif
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
   down(&registrationMutex);
   devOpenCount++;
   up(&registrationMutex);
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
   down(&registrationMutex);
   devOpenCount--;
   VSockVmciTestUnregister();
   up(&registrationMutex);
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
      VMCIId cid = VMCI_GetContextID();
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

   err = VSockVmciRegisterProto();
   if (err) {
      Warning("Cannot register vsock protocol.\n");
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
   down(&registrationMutex);
   VSockVmciUnregisterAddressFamily();
   up(&registrationMutex);

   VSockVmciUnregisterProto();
}


module_init(VSockVmciInit);
module_exit(VSockVmciExit);

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Socket Family");
MODULE_VERSION(VSOCK_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");

/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * poll.c -- management of the event callback queues, selects, ...
 */


#include "vmware.h"
#include "pollImpl.h"
#ifdef _WIN32
   #include "vmci_sockets.h"
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include "err.h"
#endif

/*
 * Maximum time (us.) to sleep when there is nothing else to do
 * before this time elapses. It has an impact on how often the
 * POLL_MAIN_LOOP events are fired.  --hpreg
 */

#define MAX_SLEEP_TIME (1 * 1000 * 1000) /* 1 s. */


static const PollImpl *pollImpl = NULL;


/*
 *----------------------------------------------------------------------
 *
 * PollSanitizeFlags --
 *
 *      For historical reasons we translate POLL_DEVICE with zero flags
 *      to POLL_FLAG_READ.  No-one knows why anymore.
 *
 * Results:
 *      Updated flags value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
PollSanitizeFlags(PollEventType type,   // IN: Event type
                  int flags)            // IN: Flags
{
   if (type == POLL_DEVICE) {
      /*
       * Either read or write must be requested for devices.
       * On Windows use POLL_FLAG_READ for waiting on events.
       */
      ASSERT((flags & (POLL_FLAG_READ | POLL_FLAG_WRITE)) != 0);
   }
   return flags;
}


/*
 *----------------------------------------------------------------------
 *
 * Poll_InitWithImpl --
 *
 *      Module initialization. An implementation of Poll should call
 *      this is initialize the function table and then start Poll.
 *
 * Results: void
 *
 * Side effects: poll is alive
 *
 *----------------------------------------------------------------------
 */

void
Poll_InitWithImpl(const PollImpl *impl)
{
   ASSERT(pollImpl == NULL);

   pollImpl = impl;

   pollImpl->Init();
}

/*
 *----------------------------------------------------------------------
 *
 * Poll_Exit --
 *
 *      module de-initalization
 *
 * Warning:
 *
 *      This function is intended to be called from vmxScsiLib or
 *      nbdScsiLib only. It has *not* been used, nor tested, in the
 *      context of the VMX product.
 *
 *----------------------------------------------------------------------
 */
void
Poll_Exit(void)
{
   pollImpl->Exit();

   pollImpl = NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * Poll_LockingEnabled --
 *
 *      Determine if locking is enabled in the underlying poll implementation.
 *
 * Results:
 *      TRUE if locking is enabled.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Poll_LockingEnabled(void)
{
   return pollImpl->LockingEnabled();
}


/*
 *----------------------------------------------------------------------
 *
 * Poll_LoopTimeout --
 *
 *      The poll loop.
 *      This is supposed to be the main loop for most programs.
 *
 * Result:
 *      Void.
 *
 * Side effects:
 *      Fiat lux!
 *
 *----------------------------------------------------------------------
 */

void
Poll_LoopTimeout(Bool loop,          // IN: loop forever if TRUE, else do one pass.
                 Bool *exit,         // IN: NULL or set to TRUE to end loop.
                 PollClass class,    // IN: class of events (POLL_CLASS_*)
                 int timeout)        // IN: maximum time to sleep
{
   pollImpl->LoopTimeout(loop, exit, class, timeout);
}



/*
 *----------------------------------------------------------------------
 *
 * Poll_Loop --
 *
 *      Run Poll_LoopTimeout with the default timeout of
 *      MAX_SLEEP_TIME (1 second)
 *
 *----------------------------------------------------------------------
 */

void
Poll_Loop(Bool loop,          // IN: loop forever if TRUE, else do one pass
          Bool *exit,         // IN: NULL or set to TRUE to end loop.
          PollClass class)    // IN: class of events (POLL_CLASS_*)
{
   Poll_LoopTimeout(loop, exit, class, MAX_SLEEP_TIME);
}


/*
 *----------------------------------------------------------------------
 *
 * Poll_CallbackRemove --
 *
 *      remove a callback from the real-time queue, the virtual time
 *      queue, the file descriptor select set, or the main loop queue.
 *
 * Results:
 *      TRUE if entry found and removed, FALSE otherwise
 *
 * Side effects:
 *      queues modified
 *
 *----------------------------------------------------------------------
 */

Bool
Poll_CallbackRemove(PollClassSet classSet,
                    int flags,
                    PollerFunction f,
                    void *clientData,
                    PollEventType type)
{
   ASSERT(f);
   flags = PollSanitizeFlags(type, flags);
   return pollImpl->CallbackRemove(classSet, flags, f, clientData, type);
}


/*
 *----------------------------------------------------------------------
 *
 * Poll_Callback --
 *
 *      Insert a callback into one of the queues (e.g., the real-time
 *      queue, the virtual time queue, the file descriptor select
 *      set, or the main loop queue).
 *
 *      For the POLL_REALTIME or POLL_DEVICE queues, entries can be
 *      inserted for good, to fire on a periodic basis (by setting the
 *      POLL_FLAG_PERIODIC flag).
 *
 *      Otherwise, the callback fires only once.
 *
 *      For periodic POLL_REALTIME callbacks, "info" is the time in
 *      microseconds between execution of the callback.  For
 *      POLL_DEVICE callbacks, info is a file descriptor.
 *
 *
 *----------------------------------------------------------------------
 */

VMwareStatus
Poll_Callback(PollClassSet classSet,
              int flags,
              PollerFunction f,
              void *clientData,
              PollEventType type,
              PollDevHandle info,
              MXUserRecLock *lock)
{
   ASSERT(f);
   flags = PollSanitizeFlags(type, flags);
   return pollImpl->Callback(classSet, flags, f, clientData, type, info, lock);
}


/*
 *----------------------------------------------------------------------
 *
 * Poll_CallbackRemoveOneByCB --
 *
 *      Remove poll entry previously added by Poll_Callback.  If there
 *      are multiple entries queued specifying same callback, it is
 *      indeterminate which one will be removed and returned.
 *
 * Results:
 *      TRUE if entry found and removed, *clientData is set to entry's
 *           client data
 *      FALSE if entry was not found
 *
 * Side effects:
 *      queues modified
 *
 *----------------------------------------------------------------------
 */

Bool
Poll_CallbackRemoveOneByCB(PollClassSet classSet,
                           int flags,
                           PollerFunction f,
                           PollEventType type,
                           void **clientData)
{
   ASSERT(f);
   ASSERT(clientData);
   flags = PollSanitizeFlags(type, flags);
   return pollImpl->CallbackRemoveOneByCB(classSet, flags, f, type, clientData);
}


/*
 *----------------------------------------------------------------------------
 *
 * Poll_NotifyChange --
 *
 *      Wake up a sleeping Poll_LoopTimeout() when there is a change
 *      it should notice, and no normal event can be expected to wake
 *      it up in a timely manner.
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
Poll_NotifyChange(PollClassSet classSet)  // IN
{
   pollImpl->NotifyChange(classSet);
}


/*
 *----------------------------------------------------------------------
 *
 * Wrappers for Poll_Callback and Poll_CallbackRemove -- special cases
 * with fewer arguments.
 *
 *----------------------------------------------------------------------
 */

VMwareStatus
Poll_CB_Device(PollerFunction f,
               void *clientData,
               PollDevHandle info,
               Bool periodic)
{
   return
   Poll_Callback(POLL_CS_MAIN,
                 POLL_FLAG_READ |
                 POLL_FLAG_REMOVE_AT_POWEROFF |
                 (periodic ? POLL_FLAG_PERIODIC : 0),
                 f,
                 clientData,
                 POLL_DEVICE,
                 info, NULL);
}


Bool
Poll_CB_DeviceRemove(PollerFunction f,
                     void *clientData,
                     Bool periodic)
{
   return
      Poll_CallbackRemove(POLL_CS_MAIN,
                          POLL_FLAG_READ |
                          POLL_FLAG_REMOVE_AT_POWEROFF |
                          (periodic ? POLL_FLAG_PERIODIC : 0),
                          f,
                          clientData,
                          POLL_DEVICE);
}


VMwareStatus
Poll_CB_RTime(PollerFunction f,
              void *clientData,
              int64 info, //microsecs
              Bool periodic,
              MXUserRecLock *lock)
{
   return
   Poll_Callback(POLL_CS_MAIN,
                 POLL_FLAG_REMOVE_AT_POWEROFF |
                 (periodic ? POLL_FLAG_PERIODIC : 0),
                 f,
                 clientData,
                 POLL_REALTIME,
                 info, lock);
}


Bool
Poll_CB_RTimeRemove(PollerFunction f,
                    void *clientData,
                    Bool periodic)
{
   return
      Poll_CallbackRemove(POLL_CS_MAIN,
                          POLL_FLAG_REMOVE_AT_POWEROFF |
                          (periodic ? POLL_FLAG_PERIODIC : 0),
                          f,
                          clientData,
                          POLL_REALTIME);
}

#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * PollSocketPairPrepare --
 *
 *      Do miscellaneous preparetion for the socket pair before connecting
 *
 * Results:
 *      Socket bound to a local address, and another set properly.
 *      TRUE if all preparetion succeed, otherwise FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
PollSocketPairPrepare(Bool blocking,           // IN: blocking socket?
                      SOCKET *src,             // IN: client side socket
                      SOCKET dst,              // IN: server side socket
                      struct sockaddr *addr,   // IN: the address connected to
                      int addrlen,             // IN: length of struct sockaddr
                      int socketCommType)      // IN: SOCK_STREAM or SOCK_DGRAM?
{
   if (bind(dst, addr, addrlen) == SOCKET_ERROR) {
      Log("%s: Could not bind socket.\n", __FUNCTION__);
      return FALSE;
   }

   if (!blocking) {
      unsigned long a = 1;
      if (ioctlsocket(*src, FIONBIO, &a) == SOCKET_ERROR) {
         Log("%s: Could not make socket non-blocking.\n", __FUNCTION__);
         return FALSE;
      }
   }

   if (socketCommType == SOCK_STREAM && listen(dst, 1) == SOCKET_ERROR) {
      Log("%s: Could not listen on a socket.\n", __FUNCTION__);
      return FALSE;
   }

   if (getsockname(dst, addr, &addrlen) == SOCKET_ERROR) {
      Log("%s: getsockname() failed.\n", __FUNCTION__);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollSocketPairConnect --
 *
 *      Connects a socket to a given address.
 *
 * Results:
 *      TRUE if connecting successfully, otherwise FALSE is returned.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
PollSocketPairConnect(Bool blocking,           // IN: blocking socket?
                      struct sockaddr *addr,   // IN: the address connected to
                      int addrlen,             // IN: length of struct sockaddr
                      SOCKET *s)               // IN: connecting socket
{
   if (connect(*s, addr, addrlen) == SOCKET_ERROR) {
      if (blocking || WSAGetLastError() != WSAEWOULDBLOCK) {
         Log("%s: Could not connect to a local socket.\n", __FUNCTION__);
         return FALSE;
      }
   } else if (!blocking) {
      Log("%s: non-blocking socket connected immediately!\n", __FUNCTION__);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollSocketClose --
 *
 *      Close the socket, and restore the original last error.
 *
 * Results:
 *      Socket is closed, original last error is restored.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
PollSocketClose(SOCKET sock) {  // IN: the socket is being closed
   int savedError = GetLastError();
   closesocket(sock);
   SetLastError(savedError);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollSocketPairConnecting --
 *
 *      Given necessary information, like socket family type, communication
 *      type, socket address and socket type, this function initialize a socket
 *      pair and make them connect to each other.
 *
 * Results:
 *      Socket bound to a given address, and another connecting
 *      to that address.
 *      INVALID_SOCKET on error.  Use WSAGetLastError() for detail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
PollSocketPairConnecting(sa_family_t sa_family,    // IN: socket family type
                         int socketCommType,       // IN: SOCK_STREAM or SOCK_DGRAM?
                         struct sockaddr *addr,    // IN: the address connected to
                         int addrlen,              // IN: length of struct sockaddr
                         Bool blocking,            // IN: blocking socket?
                         SOCKET *s)                // OUT: connecting socket
{
   SOCKET temp = INVALID_SOCKET;

   *s = socket(sa_family, socketCommType, 0);
   if (*s == INVALID_SOCKET) {
      Log("%s: Could not create socket, socket family: %d.\n", __FUNCTION__,
          sa_family);
      goto out;
   }

   temp = socket(sa_family, socketCommType, 0);
   if (temp == INVALID_SOCKET) {
      PollSocketClose(*s);
      *s = INVALID_SOCKET;
      Log("%s: Could not create second socket, socket family: %d.\n",
          __FUNCTION__, sa_family);
      goto out;
   }

   if (!PollSocketPairPrepare(blocking, s, temp, addr, addrlen, socketCommType)) {
      Log("%s: Could not prepare the socket pair for the following connecting,\
          socket type: %d\n", __FUNCTION__, sa_family);
      goto outCloseTemp;
   }

   if (!PollSocketPairConnect(blocking, addr, addrlen, s)) {
      Log("%s: Could not make socket pair connected, socket type: %d",
          __FUNCTION__, sa_family);
      goto outCloseTemp;
   }

   return temp;

outCloseTemp:
   PollSocketClose(temp);

out:
   return INVALID_SOCKET;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollIPv4SocketPairStartConnecting --
 *
 *      As one of the PollXXXSocketPairStartConnecting family, this function
 *      creates an *IPv4* socket pair.
 *
 * Results:
 *      Socket bound to a local address, and another connecting
 *      to that address.
 *      INVALID_SOCKET on error.  Use WSAGetLastError() for detail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
PollIPv4SocketPairStartConnecting(int socketCommType,  // IN: SOCK_STREAM or SOCK_DGRAM?
                                  Bool blocking,       // IN: blocking socket?
                                  SOCKET *s)           // OUT: connecting socket
{
   struct sockaddr_in iaddr;
   int addrlen;

   addrlen = sizeof iaddr;
   memset(&iaddr, 0, addrlen);
   iaddr.sin_family = AF_INET;
   iaddr.sin_addr = in4addr_loopback;
   iaddr.sin_port = 0;

   return PollSocketPairConnecting(iaddr.sin_family, socketCommType,
                                   (struct sockaddr*) &iaddr, addrlen, blocking, s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollIPv6SocketPairStartConnecting --
 *
 *      As one of the PollXXXSocketPairStartConnecting family, this function
 *      creates an *IPv6* socket pair.
 *
 * Results:
 *      Socket bound to a local address, and another connecting
 *      to that address.
 *      INVALID_SOCKET on error.  Use WSAGetLastError() for detail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
PollIPv6SocketPairStartConnecting(int socketCommType,  // IN: SOCK_STREAM or SOCK_DGRAM?
                                  Bool blocking,       // IN: blocking socket?
                                  SOCKET *s)           // OUT: connecting socket
{
   struct sockaddr_in6 iaddr6;
   int addrlen;

   addrlen = sizeof iaddr6;
   memset(&iaddr6, 0, addrlen);
   iaddr6.sin6_family = AF_INET6;
   iaddr6.sin6_addr = in6addr_loopback;
   iaddr6.sin6_port = 0;

   return PollSocketPairConnecting(iaddr6.sin6_family, socketCommType,
                                   (struct sockaddr*) &iaddr6, addrlen, blocking, s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollVMCISocketPairStartConnecting --
 *
 *      As one of the PollXXXSocketPairStartConnecting family, this function
 *      creates a *VMCI* socket pair.
 *
 * Results:
 *      Socket bound to a local address, and another connecting
 *      to that address.
 *      INVALID_SOCKET on error.  Use WSAGetLastError() for detail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
PollVMCISocketPairStartConnecting(int socketCommType,  // IN: SOCK_STREAM or SOCK_DGRAM?
                                  Bool blocking,       // IN: blocking socket?
                                  SOCKET *s)           // OUT: connecting socket
{
   struct sockaddr_vm vaddr;
   int addrlen;

   addrlen = sizeof vaddr;
   memset(&vaddr, 0, addrlen);
   vaddr.svm_family = VMCISock_GetAFValue();
   vaddr.svm_cid = VMADDR_CID_ANY;
   vaddr.svm_port = VMADDR_PORT_ANY;
   vaddr.svm_cid = VMCISock_GetLocalCID();

   return PollSocketPairConnecting(vaddr.svm_family, socketCommType,
                                   (struct sockaddr*) &vaddr, addrlen, blocking, s);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollSocketPairStartConnecting --
 *
 *      Helper function that does most of the work of creating
 *      a socket pair.
 *
 * Results:
 *      Socket bound to a local address, and another connecting
 *      to that address.
 *      INVALID_SOCKET on error.  Use WSAGetLastError() for detail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static SOCKET
PollSocketPairStartConnecting(Bool vmci,      // IN: vmci socket?
                              Bool stream,    // IN: stream socket?
                              Bool blocking,  // IN: blocking socket?
                              SOCKET *s)      // OUT: connecting socket
{
   SOCKET temp = INVALID_SOCKET;
   int socketCommType = stream ? SOCK_STREAM : SOCK_DGRAM;

   if (vmci) {
      temp = PollVMCISocketPairStartConnecting(socketCommType, blocking, s);
   } else {
      temp = PollIPv6SocketPairStartConnecting(socketCommType, blocking, s);

      if (temp == INVALID_SOCKET) {
         temp = PollIPv4SocketPairStartConnecting(socketCommType, blocking, s);
      }
   }

   return temp;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Poll_SocketPair --
 *
 *      Emulate basic socketpair() using windows API.
 *
 * Results:
 *      Two sockets connected to each other.
 *
 * Side effects:
 *      A TCP/IP or VMCI connection is created.
 *
 *-----------------------------------------------------------------------------
 */

int
Poll_SocketPair(Bool vmci,     // IN: create vmci pair?
                Bool stream,   // IN: stream socket?
                int fds[2])    // OUT: 2 sockets connected to each other
{
   SOCKET temp = INVALID_SOCKET;

   fds[0] = INVALID_SOCKET;
   fds[1] = INVALID_SOCKET;

   temp = PollSocketPairStartConnecting(vmci, stream, TRUE, (SOCKET *)&fds[0]);
   if (temp == INVALID_SOCKET) {
      goto out;
   }
   if (stream) {
      fds[1] = accept(temp, NULL, NULL);
      if (fds[1] == INVALID_SOCKET) {
         Log("%s: Could not accept on a local socket.\n", __FUNCTION__);
         goto out;
      }
      closesocket(temp);
   } else {
      fds[1] = temp;
   }
   return 0;

out:
   Warning("%s: Error creating a %s socket pair: %d/%s\n", __FUNCTION__,
           vmci ? "vmci" : "inet", WSAGetLastError(), Err_ErrString());
   closesocket(temp);
   closesocket(fds[0]);
   closesocket(fds[1]);
   return SOCKET_ERROR;
}
#endif // _WIN32

//#define POLL_UNITTEST 1
//#define POLL_TESTLOCK 1
//#define POLL_TESTVMCI 1

#if POLL_UNITTEST // All the way to EOF

#if _WIN32
   #include <winsock2.h>
   #include <time.h>
   #include "err.h"
   #include "random.h"
#else
   #include <sys/socket.h>
   #include <unistd.h>
#endif
#include "vmci_sockets.h"
#if POLL_TESTLOCK
   #include "vthread.h"
   #include "util.h"
   #include "../../vmx/public/mutexRankVMX.h"
   #define GRAB_LOCK(_lock)             \
      if (_lock) {                      \
         MXUser_AcquireRecLock(_lock);  \
      }
   #define DROP_LOCK(_lock)             \
      if (_lock) {                      \
         MXUser_ReleaseRecLock(_lock);  \
      }
   #define NUM_TEST_ITERS 10
#else
   #define GRAB_LOCK(_lock)
   #define DROP_LOCK(_lock)
#endif

/*
 * Make this queue length a little bit less than poll implementation's max
 * to allow for some sockets in the test program itself.
 */

#define MAX_QUEUE_LENGTH 4090
#define MAX_VMX_QUEUE_LENGTH 450

static char reinstallPoll[1];
static char removePoll[1];
static unsigned int realTimeCount;
static unsigned int mainLoopCount;
static int fds[2];
static unsigned int deviceRCount;
static unsigned int deviceWCount;
static unsigned int state;
static unsigned int successCount;
static unsigned int failureCount;
static unsigned int dummyCount;
static Bool isVMX;
static Bool useLocking;
static Bool testVMCI;
static MXUserRecLock *cbLock;
static unsigned int lockErrors;
#if POLL_TESTLOCK
static volatile Bool exitThread;
#endif
static volatile Bool rtDeleted;
static volatile Bool mlDeleted;
static volatile Bool drDeleted;
static volatile Bool dwDeleted;
static unsigned int rtCbRace;
static unsigned int mlCbRace;
static unsigned int drCbRace;
static unsigned int dwCbRace;

typedef struct SocketPair {
   int fds[2];
   int count;
} SocketPair;
static SocketPair socketPairs[MAX_QUEUE_LENGTH];

#ifdef _WIN32
static HANDLE events[2];
static unsigned int deviceEv0Count;
static unsigned int deviceEv1Count;
static SOCKET boundSocket;
#endif


/*
 *----------------------------------------------------------------------------
 *
 * CheckLockState --
 *
 *      If testing with lock, verify that lock is held by the calling thread.
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
CheckLockState(void)
{
   if (cbLock && !MXUser_IsCurThreadHoldingRecLock(cbLock)) {
      lockErrors++;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_RealTime --
 *
 *      Real time test Poll callback.
 *
 *      Increment the callback's counter, and if 'clientData' is not NULL,
 *      self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_RealTime(void *clientData) // IN
{
   realTimeCount++;
   CheckLockState();
   rtCbRace += (rtDeleted != FALSE);
   if (clientData == reinstallPoll) {
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_RealTime,
                    clientData,
                    POLL_REALTIME,
                    0,
                    cbLock);
   } else if (clientData == removePoll) {
      Bool ret;

      ret = Poll_CallbackRemove(POLL_CS_MAIN, POLL_FLAG_PERIODIC,
                                PollUnitTest_RealTime, clientData,
                                POLL_REALTIME);
      ASSERT(ret);
   } else {
      ASSERT(clientData == NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_MainLoop --
 *
 *      Main loop test Poll callback.
 *
 *      Increment the callback's counter, and if 'clientData' is not NULL,
 *      self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_MainLoop(void *clientData) // IN
{
   mainLoopCount++;
   CheckLockState();
   mlCbRace += (mlDeleted != FALSE);
   if (clientData == reinstallPoll) {
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_MainLoop,
                    clientData,
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
   } else if (clientData == removePoll) {
      Bool ret;

      ret = Poll_CallbackRemove(POLL_CS_MAIN, POLL_FLAG_PERIODIC,
                                PollUnitTest_MainLoop, clientData, POLL_MAIN_LOOP);
      ASSERT(ret);
   } else {
      ASSERT(clientData == NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_DeviceR --
 *
 *      Device read test Poll callback.
 *
 *      Increment the callback's counter, and if 'clientData' is not NULL,
 *      self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_DeviceR(void *clientData) // IN
{
#ifdef _WIN32
   /*
    * Windows won't tell us a socket is readable unless some kind of
    * read is performed.  Read, then re-fill the buffer.
    */

   char buf[sizeof fds];

   recv(fds[1], buf, sizeof fds, 0);
   send(fds[0], (const char *)fds, sizeof fds, 0);
   ASSERT(!memcmp(buf, fds, sizeof fds));
#endif
   deviceRCount++;

   CheckLockState();
   drCbRace += (drDeleted != FALSE);
   if (clientData == reinstallPoll) {
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ,
                    PollUnitTest_DeviceR,
                    clientData,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
   } else if (clientData == removePoll) {
      Bool ret;

      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceR, clientData, POLL_DEVICE);
      ASSERT(ret);
   } else {
      ASSERT(clientData == NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_DeviceW --
 *
 *      Device write test Poll callback.
 *
 *      Increment the callback's counter, and if 'clientData' is not NULL,
 *      self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_DeviceW(void *clientData) // IN
{
   deviceWCount++;
   CheckLockState();
   dwCbRace += (dwDeleted != FALSE);
   if (clientData == reinstallPoll) {
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                    PollUnitTest_DeviceW,
                    clientData,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
   } else if (clientData == removePoll) {
      Bool ret;

      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceW, clientData, POLL_DEVICE);
      ASSERT(ret);
   } else {
      ASSERT(clientData == NULL);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_DeviceRQ --
 *
 *      Device read test Poll callback, the queue test version.
 *
 *      Increment the callback's counter, and self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_DeviceRQ(void *clientData) // IN
{
   int queueIndex = (int)(intptr_t)clientData;
#ifdef _WIN32
   /*
    * Windows won't tell us a socket is readable unless some kind of
    * read is performed.  Read, then re-fill the buffer.
    */

   char buf[sizeof fds];

   recv(socketPairs[queueIndex].fds[1], buf, sizeof fds, 0);
   send(socketPairs[queueIndex].fds[0], (const char *)fds, sizeof fds, 0);
   ASSERT(!memcmp(buf, fds, sizeof fds));
#endif
   deviceRCount++;
   socketPairs[queueIndex].count++;

   CheckLockState();
   Poll_Callback(POLL_CS_MAIN,
                 POLL_FLAG_SOCKET | POLL_FLAG_READ,
                 PollUnitTest_DeviceRQ,
                 clientData,
                 POLL_DEVICE,
                 socketPairs[queueIndex].fds[1],
                 cbLock);
}


#if defined(POLL_TESTLOCK) && defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_DeviceEvent --
 *
 *      Device read test Poll callback for Event.
 *
 *      Increment the callback's counter, and if 'clientData' is not NULL,
 *      self reschedule.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_DeviceEvent(void *clientData) // IN
{
   CheckLockState();
   if (clientData == NULL) {
      deviceEv0Count++;
      SetEvent(events[0]);
   } else {
      ASSERT(clientData == reinstallPoll);
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_READ,
                    PollUnitTest_DeviceEvent
                    clientData,
                    POLL_DEVICE,
                    (PollDevHandle)events[1],
                    cbLock);
      deviceEv1Count++;
      SetEvent(events[1]);
   }
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_TestResult --
 *
 *      Log and count test result.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Outputs the result of the test with Warning().
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_TestResult(Bool success) // IN: TRUE on success, FALSE on failure
{
   if (success && lockErrors == 0) {
      successCount++;
      Warning("%s:   success\n", __FUNCTION__);
   } else {
      failureCount++;
      if (useLocking) {
         Warning("%s:   failure (lockErrors = %u)\n", __FUNCTION__, lockErrors);
      } else {
         Warning("%s:   failure\n", __FUNCTION__);
      }
   }
   lockErrors = 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * PollUnitTest_DummyCallback --
 *
 *      Used for tickling the poll loop periodically.
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
PollUnitTest_DummyCallback(void *clientData) // IN: unused
{
   dummyCount++;
}


#if POLL_TESTLOCK
/*
 *----------------------------------------------------------------------------
 *
 * PollAddRemoveCBThread --
 *
 *      This thread continuously creates a lock, adds a poll callback that
 *      will take the lock, removes the callback, and destroys the lock.  It
 *      is designed to race against the thread running Poll_Loop().
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
PollAddRemoveCBThread(void *clientData)  // IN: unused
{
   Bool ret;
   uint32 numIters = 0;
   int periodicFlag[2] = { 0, POLL_FLAG_PERIODIC };
   void *cbData[2] = { reinstallPoll, NULL };
   uint32 prevCounts[4] = { realTimeCount, mainLoopCount, deviceRCount,
                            deviceWCount };
   uint32 perIterError[4] = { 0 };
   uint32 prevDummyCount;
   uint32 dummyDiff;
   uint32 oddIter;

   while (!exitThread) {
      oddIter = numIters++ & 0x1;

      /* Add and remove real time and main loop callbacks */
      cbLock = MXUser_CreateRecLock("pollUnitTestLock",
                                    RANK_pollUnitTestLock);
      rtDeleted = FALSE;
      Poll_Callback(POLL_CS_MAIN,
                    periodicFlag[oddIter],
                    PollUnitTest_RealTime,
                    cbData[oddIter],
                    POLL_REALTIME,
                    5000,
                    cbLock);
      mlDeleted = FALSE;
      Poll_Callback(POLL_CS_MAIN,
                    periodicFlag[1 - oddIter],
                    PollUnitTest_MainLoop,
                    cbData[1 - oddIter],
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
      prevDummyCount = dummyCount;
      Util_Usleep(15000);
      /* This tells us if poll thread gets to run during our sleep. */
      dummyDiff = dummyCount - prevDummyCount;
      MXUser_AcquireRecLock(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                periodicFlag[oddIter],
                                PollUnitTest_RealTime,
                                cbData[oddIter],
                                POLL_REALTIME);
      rtDeleted = TRUE;
      MXUser_ReleaseRecLock(cbLock);
      ASSERT(ret);
      MXUser_AcquireRecLock(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                periodicFlag[1 - oddIter],
                                PollUnitTest_MainLoop,
                                cbData[1 - oddIter],
                                POLL_MAIN_LOOP);
      mlDeleted = TRUE;
      MXUser_ReleaseRecLock(cbLock);
      ASSERT(ret);
      MXUser_DestroyRecLock(cbLock);
      if (exitThread) {
         break;
      }
      /* If dummyCallback fires multiple times, our callbacks should too. */
      perIterError[0] += (realTimeCount - prevCounts[0] < 2 && dummyDiff > 2);
      perIterError[1] += (mainLoopCount - prevCounts[1] < 2 && dummyDiff > 2);
      prevCounts[0] = realTimeCount;
      prevCounts[1] = mainLoopCount;

      /* Add and remove device callbacks */
      cbLock = MXUser_CreateRecLock("pollUnitTestLock",
                                    RANK_pollUnitTestLock);
      drDeleted = FALSE;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ | periodicFlag[oddIter],
                    PollUnitTest_DeviceR,
                    cbData[oddIter],
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      dwDeleted = FALSE;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE |
                    periodicFlag[1 - oddIter],
                    PollUnitTest_DeviceW,
                    cbData[1 - oddIter],
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      prevDummyCount = dummyCount;
      Util_Usleep(10000);
      dummyDiff = dummyCount - prevDummyCount;
      MXUser_AcquireRecLock(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ |
                                periodicFlag[oddIter],
                                PollUnitTest_DeviceR,
                                cbData[oddIter],
                                POLL_DEVICE);
      drDeleted = TRUE;
      MXUser_ReleaseRecLock(cbLock);
      ASSERT(ret);
      MXUser_AcquireRecLock(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE |
                                periodicFlag[1 - oddIter],
                                PollUnitTest_DeviceW,
                                cbData[1 - oddIter],
                                POLL_DEVICE);
      dwDeleted = TRUE;
      MXUser_ReleaseRecLock(cbLock);
      ASSERT(ret);
      MXUser_DestroyRecLock(cbLock);
      if (!exitThread) {
         perIterError[2] += (deviceRCount - prevCounts[2] < 2 && dummyDiff > 2);
         perIterError[3] += (deviceWCount - prevCounts[3] < 2 && dummyDiff > 2);
         prevCounts[2] = deviceRCount;
         prevCounts[3] = deviceWCount;
      }
   }
   cbLock = NULL;
   PollUnitTest_TestResult(perIterError[0] == 0 && perIterError[1] == 0 &&
                           perIterError[2] == 0 && perIterError[3] == 0);
}


#ifdef _WIN32
/*
 *----------------------------------------------------------------------------
 *
 * PollLockContentionThread --
 *
 *      Create lock contention on cbLock.
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
PollLockContentionThread(void *clientData)  // IN: unused
{
   rqContext *rCtxt = Random_QuickSeed((uint32)time(NULL));

   while (!exitThread) {
      MXUser_AcquireRecLock(cbLock);
      Sleep(Random_Quick(rCtxt) % 5);
      MXUser_ReleaseRecLock(cbLock);
      Sleep(Random_Quick(rCtxt) % 5 + 10);
   }
}
#endif
#endif  // POLL_TESTLOCK


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest_StateMachine --
 *
 *      State machine. The heart of Poll's unit test. Sequentially run all
 *      tests.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Outputs the result of the tests with Warning().
 *
 *-----------------------------------------------------------------------------
 */

static void
PollUnitTest_StateMachine(void *clientData) // IN: Unused
{
#if POLL_TESTLOCK
   static VThreadID cbRaceThread;
   static unsigned int raceTestIter;
#endif
   static unsigned int queueTestIter;
#ifdef _WIN32
   static unsigned int eventTestIter;
#endif
   static unsigned int maxInetSockets;
   static unsigned int maxVMCISockets;
   static unsigned int queueLen;
   Bool ret;
   int i, retval;
   int queueReads = 0;

#ifdef _WIN32
   maxVMCISockets = MAXIMUM_WAIT_OBJECTS - 2;
#else
   maxVMCISockets = 60;
#endif
   queueLen = isVMX ? MAX_VMX_QUEUE_LENGTH : MAX_QUEUE_LENGTH;

   switch (state) {
   case 0:
      Warning("%s: Poll unit test: start%s%s\n", __FUNCTION__,
              testVMCI ? " vmci tests" : "", useLocking ? " locking tests" : "");
      Warning("%s: Testing RealTime 0 0\n", __FUNCTION__);
      realTimeCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_RealTime,
                    NULL,
                    POLL_REALTIME,
                    0,
                    cbLock);
      state++;
      break;

   case 1:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                0,
                                PollUnitTest_RealTime,
                                NULL,
                                POLL_REALTIME);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && realTimeCount == 1);
      state++;
      break;

   case 2:
      Warning("%s: Testing RealTime 1 0\n", __FUNCTION__);
      realTimeCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_PERIODIC,
                    PollUnitTest_RealTime,
                    NULL,
                    POLL_REALTIME,
                    100000,
                    cbLock);
      state++;
      break;

   case 3:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_PERIODIC,
                                PollUnitTest_RealTime,
                                NULL,
                                POLL_REALTIME);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && realTimeCount > 1);
      state++;
      break;

   case 4:
      Warning("%s: Testing RealTime 0 1\n", __FUNCTION__);
      realTimeCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_RealTime,
                    reinstallPoll,
                    POLL_REALTIME,
                    0,
                    cbLock);
      state++;
      break;

   case 5:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                0,
                                PollUnitTest_RealTime,
                                reinstallPoll,
                                POLL_REALTIME);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && realTimeCount > 1);
      state++;
      break;

   case 6:
      Warning("%s: Testing RealTime 1 1\n", __FUNCTION__);
      realTimeCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_PERIODIC,
                    PollUnitTest_RealTime,
                    removePoll,
                    POLL_REALTIME,
                    100000,
                    cbLock);
      state++;
      break;

   case 7:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_PERIODIC,
                                PollUnitTest_RealTime,
                                NULL,
                                POLL_REALTIME);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && realTimeCount == 1);
      state++;
      break;

   case 8:
      Warning("%s: Testing MainLoop 0 0\n", __FUNCTION__);

      /*
       * A periodic real time callback ensures that we go over the main loop
       * queue more than once for the duration of each state.
       */
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_PERIODIC,
                    PollUnitTest_DummyCallback,
                    NULL,
                    POLL_REALTIME,
                    100000,
                    NULL);
      mainLoopCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_MainLoop,
                    NULL,
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
      state++;
      break;

   case 9:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                0,
                                PollUnitTest_MainLoop,
                                NULL,
                                POLL_MAIN_LOOP);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && mainLoopCount == 1);
      state++;
      break;

   case 10:
      Warning("%s: Testing MainLoop 1 0\n", __FUNCTION__);
      mainLoopCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_PERIODIC,
                    PollUnitTest_MainLoop,
                    NULL,
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
      state++;
      break;

   case 11:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_PERIODIC,
                                PollUnitTest_MainLoop,
                                NULL,
                                POLL_MAIN_LOOP);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && mainLoopCount > 1);
      state++;
      break;

   case 12:
      Warning("%s: Testing MainLoop 0 1\n", __FUNCTION__);
      mainLoopCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    0,
                    PollUnitTest_MainLoop,
                    reinstallPoll,
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
      state++;
      break;

   case 13:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                0,
                                PollUnitTest_MainLoop,
                                reinstallPoll,
                                POLL_MAIN_LOOP);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && mainLoopCount > 1);
      state++;
      break;

   case 14:
      Warning("%s: Testing MainLoop 1 1\n", __FUNCTION__);
      mainLoopCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_PERIODIC,
                    PollUnitTest_MainLoop,
                    removePoll,
                    POLL_MAIN_LOOP,
                    0,
                    cbLock);
      state++;
      break;

   case 15:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_PERIODIC,
                                PollUnitTest_MainLoop,
                                removePoll,
                                POLL_MAIN_LOOP);
      DROP_LOCK(cbLock);
      Poll_CallbackRemove(POLL_CS_MAIN,
                          POLL_FLAG_PERIODIC,
                          PollUnitTest_DummyCallback,
                          NULL,
                          POLL_REALTIME);
      PollUnitTest_TestResult(ret == FALSE && mainLoopCount == 1);
      state++;
      break;

   case 16:
      Warning("%s: Testing DeviceR 0 0\n", __FUNCTION__);
      deviceRCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ,
                    PollUnitTest_DeviceR,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 17:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ,
                                PollUnitTest_DeviceR,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && deviceRCount == 1);
      state++;
      break;

   case 18:
      Warning("%s: Testing DeviceR 1 0\n", __FUNCTION__);
      deviceRCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceR,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 19:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceR,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceRCount > 1);
      state++;
      break;

   case 20:
      Warning("%s: Testing DeviceR 0 1\n", __FUNCTION__);
      deviceRCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ,
                    PollUnitTest_DeviceR,
                    reinstallPoll,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 21:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ,
                                PollUnitTest_DeviceR,
                                reinstallPoll,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceRCount > 1);
      state++;
      break;

   case 22:
      Warning("%s: Testing DeviceR 1 1\n", __FUNCTION__);
      deviceRCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceR,
                    removePoll,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 23:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceR,
                                removePoll,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && deviceRCount == 1);
      state++;
      break;

   case 24:
      Warning("%s: Testing DeviceW 0 0\n", __FUNCTION__);
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                    PollUnitTest_DeviceW,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 25:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                                PollUnitTest_DeviceW,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && deviceWCount == 1);
      state++;
      break;

   case 26:
      Warning("%s: Testing DeviceW 1 0\n", __FUNCTION__);
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceW,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 27:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceW,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceWCount > 1);
      state++;
      break;

   case 28:
      Warning("%s: Testing DeviceW 0 1\n", __FUNCTION__);
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                    PollUnitTest_DeviceW,
                    reinstallPoll,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 29:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                                PollUnitTest_DeviceW,
                                reinstallPoll,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceWCount > 1);
      state++;
      break;

   case 30:
      Warning("%s: Testing DeviceW 1 1\n", __FUNCTION__);
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceW,
                    removePoll,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 31:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceW,
                                removePoll,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && deviceWCount == 1);
      state++;
      break;

   case 32:
      Warning("%s: Testing Device add R, add W, remove R, remove W\n",
              __FUNCTION__);
      deviceRCount = 0;
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceR,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceW,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 33:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceR,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceRCount > 1 && deviceWCount > 1);
      deviceRCount = 0;
      deviceWCount = 0;
      state++;
      break;

   case 34:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceW,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceRCount == 0 && deviceWCount > 1);
      state++;
      break;

   case 35:
      Warning("%s: Testing Device add R, add W, remove W, remove R\n",
              __FUNCTION__);
      deviceRCount = 0;
      deviceWCount = 0;
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceR,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceW,
                    NULL,
                    POLL_DEVICE,
                    fds[1],
                    cbLock);
      state++;
      break;

   case 36:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceW,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceRCount > 1 && deviceWCount > 1);
      deviceRCount = 0;
      deviceWCount = 0;
      state++;
      break;

   case 37:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceR,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceWCount == 0 && deviceRCount > 1);
      state++;
#ifndef _WIN32
      // The next test only makes sense on Windows.
      state += 3;
#endif
      break;

#ifdef _WIN32
   case 38:
      Warning("%s: Testing connecting socket\n", __FUNCTION__);
      closesocket(fds[0]);
      closesocket(fds[1]);
      fds[0] = INVALID_SOCKET;
      fds[1] = INVALID_SOCKET;
      deviceWCount = 0;
      boundSocket = PollSocketPairStartConnecting(testVMCI, TRUE, FALSE,
                                                  (SOCKET *)&fds[0]);
      if (boundSocket == INVALID_SOCKET) {
         Warning("%s:   failure -- error creating socket pair\n", __FUNCTION__);
         state += 3;
      } else {
         Poll_Callback(POLL_CS_MAIN,
                       POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                       PollUnitTest_DeviceW,
                       NULL,
                       POLL_DEVICE,
                       fds[0],
                       cbLock);
         state++;
      }
      break;

   case 39:
      fds[1] = accept(boundSocket, NULL, NULL);
      if (fds[1] == INVALID_SOCKET) {
         Warning("%s: Error accepting socket %d: %d/%s\n", __FUNCTION__,
                 boundSocket, WSAGetLastError(), Err_ErrString());
      }
      state++;
      break;

   case 40:
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_SOCKET | POLL_FLAG_WRITE,
                                PollUnitTest_DeviceW,
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == FALSE && deviceWCount >= 1);
      state++;
      break;
#endif

   case 41:
      maxInetSockets = testVMCI ? queueLen - maxVMCISockets :
                                  queueLen;
      Warning("%s: Testing queue size %d\n", __FUNCTION__, queueLen);
      deviceRCount = 0;
      queueTestIter = 0;
      queueReads = 0;
      for (i = 0; i < queueLen; i++) {
         Bool useVMCI = i >= maxInetSockets;
#ifdef _WIN32
         socketPairs[i].fds[0] = INVALID_SOCKET;
         socketPairs[i].fds[1] = INVALID_SOCKET;
         if (Poll_SocketPair(useVMCI, TRUE, socketPairs[i].fds) < 0) {
            Warning("%s:   failure -- error creating socketpair, iteration %d\n",
                    __FUNCTION__, i);
            break;
         }
         send(socketPairs[i].fds[0], (const char *)fds, sizeof fds, 0);
#else
         int addrFamily = useVMCI ? VMCISock_GetAFValue(): AF_UNIX;
         socketPairs[i].fds[0] = -1;
         socketPairs[i].fds[1] = -1;
         if (socketpair(addrFamily, SOCK_STREAM, 0, socketPairs[i].fds) < 0) {
            Warning("%s:   failure -- error creating socketpair, iteration %d\n",
                    __FUNCTION__, i);
            break;
         }
         retval = write(socketPairs[i].fds[0], fds, 1);
#endif
         socketPairs[i].count = 0;
         Poll_Callback(POLL_CS_MAIN,
                       POLL_FLAG_SOCKET | POLL_FLAG_READ,
                       PollUnitTest_DeviceRQ,
                       (void *)(intptr_t)i,
                       POLL_DEVICE,
                       socketPairs[i].fds[1],
                       cbLock);
      }
      state++;
      break;

   case 42:
      if (++queueTestIter >= queueLen / 1000) {
         state++;
      }
      break;

   case 43:
      Warning("%s:   %d reads completed\n", __FUNCTION__, deviceRCount);
      for (i = 0; i < queueLen; i++) {
         if (socketPairs[i].count) {
            queueReads++;
         }
         GRAB_LOCK(cbLock);
         Poll_CallbackRemove(POLL_CS_MAIN,
                             POLL_FLAG_SOCKET | POLL_FLAG_READ,
                             PollUnitTest_DeviceRQ,
                             (void *)(intptr_t)i,
                             POLL_DEVICE);
         DROP_LOCK(cbLock);
#ifdef _WIN32
         closesocket(socketPairs[i].fds[0]);
         closesocket(socketPairs[i].fds[1]);
#else
         close(socketPairs[i].fds[0]);
         close(socketPairs[i].fds[1]);
#endif
      }
      Warning("%s:   read %d sockets at least once.\n", __FUNCTION__, queueReads);
      PollUnitTest_TestResult(deviceRCount > queueLen);
      state++;
      break;

   case 44:
#if POLL_TESTVMCI

      /*
       * The following tests only work inside the guest,
       * as stream VSockets are unsuported for host<->host communication.
       */

      if (!useLocking && !testVMCI) {
         testVMCI = TRUE;
   #ifdef _WIN32
         /* Discard sockets used in connect test and re-create them. */
         closesocket(fds[0]);
         closesocket(fds[1]);
         fds[0] = INVALID_SOCKET;
         fds[1] = INVALID_SOCKET;
         if (Poll_SocketPair(TRUE, TRUE, fds) < 0) {
            Warning("%s:   failure -- error creating vmci socketpair\n",
                    __FUNCTION__);
            state ++;
            break;
         }
         send(fds[0], (const char *)fds, sizeof fds, 0);
   #else
         close(fds[0]);
         close(fds[1]);
         fds[0] = -1;
         fds[1] = -1;
         if (socketpair(VMCISock_GetAFValue(), SOCK_STREAM, 0, fds) < 0) {
            Warning("%s:   failure -- error creating vsock socketpair\n",
                    __FUNCTION__);
            break;
         }
         retval = write(fds[0], fds, 0);
   #endif
         state = 0;
         break;
      } else {
         testVMCI = FALSE;
         state++;
         // fall through to next test
      }
#else
      state++;
#endif

   case 45:
#if POLL_TESTLOCK
      if (useLocking == FALSE) {
         useLocking = TRUE;
         cbLock = MXUser_CreateRecLock("pollUnitTestLock",
                                       RANK_pollUnitTestLock);

   #ifdef _WIN32
         /* Discard sockets used in connect test and re-create them. */
         closesocket(fds[0]);
         closesocket(fds[1]);
         fds[0] = INVALID_SOCKET;
         fds[1] = INVALID_SOCKET;
         if (Poll_SocketPair(FALSE, TRUE, fds) < 0) {
            Warning("%s:   failure -- error creating socketpair\n",
                    __FUNCTION__);
            state += 3;
            break;
         }
         send(fds[0], (const char *)fds, sizeof fds, 0);
   #endif
         state = 0;
         break;
      } else {
         state++;
         // fall through to next test
      }

   case 46:

      /*
       * This test requires a poll implementation that supports locking
       * for both the internal poll state and the callbacks.  It also uses
       * VThread.
       */
      Warning("%s: Testing add/remove callback and Poll_Loop race (about %u s)\n",
              __FUNCTION__, NUM_TEST_ITERS);

   #ifdef _WIN32
      closesocket(fds[0]);
      closesocket(fds[1]);
      fds[0] = INVALID_SOCKET;
      fds[1] = INVALID_SOCKET;
      if (Poll_SocketPair(FALSE, TRUE, fds) < 0)
   #else
      close(fds[0]);
      close(fds[1]);
      fds[0] = -1;
      fds[1] = -1;
      if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
   #endif
      {
         Warning("%s:   failure -- error creating socketpair\n", __FUNCTION__);
         state += 3;
         break;
      }

      /* Make fds[1] both readable and writable. */
   #ifdef _WIN32
      send(fds[0], (const char *)fds, sizeof fds, 0);
   #else
      retval = write(fds[0], fds, 1);
   #endif

      MXUser_DestroyRecLock(cbLock);
      cbLock = NULL;
      rtCbRace = 0;
      mlCbRace = 0;
      drCbRace = 0;
      dwCbRace = 0;
      realTimeCount = 0;
      mainLoopCount = 0;
      deviceRCount = 0;
      deviceWCount = 0;
      raceTestIter = 0;
      dummyCount = 0;
      exitThread = FALSE;
      VThread_CreateThread(PollAddRemoveCBThread, NULL,
                           "PollAddRemoveCBThread", &cbRaceThread);
      if (cbRaceThread == VTHREAD_INVALID_ID) {
         Warning("%s:   failure -- error creating thread\n", __FUNCTION__);
         state += 3;
         break;
      }
      Poll_Callback(POLL_CS_MAIN, POLL_FLAG_PERIODIC,
                    PollUnitTest_DummyCallback, NULL,
                    POLL_REALTIME, 5000, NULL);
      state++;
      break;

   case 47:
      if (++raceTestIter == NUM_TEST_ITERS) {
         state++;
      }
      break;

   case 48:
      Poll_CallbackRemove(POLL_CS_MAIN, POLL_FLAG_PERIODIC,
                          PollUnitTest_DummyCallback,
                          NULL, POLL_REALTIME);
      exitThread = TRUE;
      VThread_DestroyThread(cbRaceThread);
      PollUnitTest_TestResult(rtCbRace == 0 && mlCbRace == 0 && drCbRace == 0 &&
                              dwCbRace == 0);
      state++;
   #ifndef _WIN32
      // The next test only makes sense on Windows.
      state += 3;
   #endif
      break;

   #ifdef _WIN32
   case 49:
      Warning("%s: Testing event-based device callbacks with lock contention\n",
              __FUNCTION__);
      if (!cbLock) {
         /* The previous test may have destroyed the lock. */
         cbLock = MXUser_CreateRecLock("pollUnitTestLock",
                                       RANK_pollUnitTestLock);
      }
      deviceEv0Count = 0;
      deviceEv1Count = 0;
      eventTestIter = 0;
      exitThread = FALSE;
      VThread_CreateThread(PollLockContentionThread, NULL,
                           "PollLockContention", &cbRaceThread);
      if (cbRaceThread == VTHREAD_INVALID_ID) {
         Warning("%s:   failure -- error creating thread\n", __FUNCTION__);
         state += 3;
         break;
      }
      events[0] = CreateEvent(NULL, FALSE, TRUE, NULL);
      VERIFY(events[0]);
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                    PollUnitTest_DeviceEvent
                    NULL,
                    POLL_DEVICE,
                    (PollDevHandle)events[0],
                    cbLock);
      events[1] = CreateEvent(NULL, FALSE, TRUE, NULL);
      VERIFY(events[1]);
      Poll_Callback(POLL_CS_MAIN,
                    POLL_FLAG_READ,
                    PollUnitTest_DeviceEvent
                    reinstallPoll,
                    POLL_DEVICE,
                    (PollDevHandle)events[1],
                    cbLock);
      state++;
      break;

   case 50:
      if (++eventTestIter == 2) {
         state++;
      }
      break;

   case 51:
      exitThread = TRUE;
      VThread_DestroyThread(cbRaceThread);
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                PollUnitTest_DeviceEvent
                                NULL,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceEv0Count > 1);
      GRAB_LOCK(cbLock);
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_READ,
                                PollUnitTest_DeviceEvent
                                reinstallPoll,
                                POLL_DEVICE);
      DROP_LOCK(cbLock);
      PollUnitTest_TestResult(ret == TRUE && deviceEv1Count > 1);
      CloseHandle(events[0]);
      CloseHandle(events[1]);
      state++;
      break;
   #endif  // _WIN32
#endif  // POLL_TESTLOCK (#else fall through)

   case 52:
      ret = Poll_CallbackRemove(POLL_CS_MAIN,
                                POLL_FLAG_PERIODIC,
                                PollUnitTest_StateMachine,
                                NULL,
                                POLL_REALTIME);
      ASSERT(ret);
      Warning("%s: Poll unit test: stop, %u successes, %u failures\n",
              __FUNCTION__, successCount, failureCount);
      if (cbLock) {
         MXUser_DestroyRecLock(cbLock);
      }
#ifdef _WIN32
      closesocket(fds[0]);
      closesocket(fds[1]);
      WSACleanup();
#else
      close(fds[0]);
      close(fds[1]);
#endif
      break;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * PollUnitTest --
 *
 *      Start the unit test suite for an implementation of the Poll_* API. It
 *      will stop by itself.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Outputs the result of the tests with Warning().
 *
 *-----------------------------------------------------------------------------
 */

void
PollUnitTest(Bool vmx)  // IN: use vmx-size poll queue
{
#ifdef _WIN32
   WSADATA wsaData;
   WORD versionRequested = MAKEWORD(2, 0);
   int ret;
#endif
   int retval;

   state = 0;
   successCount = failureCount = 0;
   useLocking = FALSE;
   isVMX = vmx;
#ifdef _WIN32
   ret = WSAStartup(versionRequested, &wsaData);
   if (ret != 0) {
      Warning("%s: Error in WSAStartup: %d\n", __FUNCTION__, ret);
      return;
   }
   fds[0] = INVALID_SOCKET;
   fds[1] = INVALID_SOCKET;
   if (Poll_SocketPair(FALSE, TRUE, fds) < 0) {
#else
   fds[0] = -1;
   fds[1] = -1;
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
#endif
      Warning("%s: socketpair failed\n", __FUNCTION__);
      return;
   }

   // Make fds[1] both readable and writable.
#ifdef _WIN32
   retval = send(fds[0], (const char *)fds, sizeof fds, 0);
#else
   retval = write(fds[0], fds, 1);
#endif
   Warning("%s: Starting\n", __FUNCTION__);
   Poll_Callback(POLL_CS_MAIN,
                 POLL_FLAG_PERIODIC,
                 PollUnitTest_StateMachine,
                 NULL,
                 POLL_REALTIME,
                 1000000 /* 1 s. */,
                 NULL);
}


#endif // POLL_UNITTEST

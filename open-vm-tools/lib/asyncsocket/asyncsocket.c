/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * asyncsocket.c --
 *
 *      The AsyncSocket object is a fairly simple wrapper around a basic TCP
 *      socket. It's potentially asynchronous for both read and write
 *      operations. Reads are "requested" by registering a receive function
 *      that is called once the requested amount of data has been read from
 *      the socket. Similarly, writes are queued along with a send function
 *      that is called once the data has been written. Errors are reported via
 *      a separate callback.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#include "str.h"

#include "vmware.h"
#include "asyncsocket.h"
#include "asyncSocketInt.h"
#include "poll.h"
#include "log.h"
#include "err.h"
#include "hostinfo.h"
#include "util.h"
#include "msg.h"
#include "posix.h"
#include "vmci_sockets.h"
#ifndef VMX86_TOOLS
#include "vmdblib.h"
#endif

#define LOGLEVEL_MODULE asyncsocket
#include "loglevel_user.h"

#ifdef VMX86_SERVER
#include "uwvmkAPI.h"
#endif


/*
 * The slots each have a "unique" ID, which is just an incrementing integer.
 */
static Atomic_uint32 nextid = { 1 };

/*
 * Local Functions
 */
static Bool AsyncSocketHasDataPending(AsyncSocket *asock);
static int AsyncSocketMakeNonBlocking(int fd);
static void AsyncSocketAcceptCallback(void *clientData);
static void AsyncSocketConnectCallback(void *clientData);
static void AsyncSocketRecvUDPCallback(void *clientData);
static int AsyncSocketBlockingWork(AsyncSocket *asock, Bool read, void *buf, int len,
                                   int *completed, int timeoutMS, Bool partial);
static VMwareStatus AsyncSocketPollAdd(AsyncSocket *asock, Bool socket,
                                       int flags, PollerFunction callback,
                                       ...);
static Bool AsyncSocketPollRemove(AsyncSocket *asock, Bool socket,
                                  int flags, PollerFunction callback);
static unsigned int AsyncSocketGetPort(struct sockaddr *addr);
static AsyncSocket *AsyncSocketConnect(struct sockaddr *addr,
                                       socklen_t addrLen,
                                       AsyncSocketConnectFn connectFn,
                                       void *clientData,
                                       PollerFunction internalConnectFn,
                                       AsyncSocketConnectFlags flags,
                                       AsyncSocketPollParams *pollParams,
                                       int *outError);
static int AsyncSocketConnectInternal(AsyncSocket *s);
static int AsyncSocketRecv(AsyncSocket *asock, void *buf, int len,
                           Bool fireOnPartial, void *cb, void *cbData);
static Bool AsyncSocketHasDataPendingSocket(AsyncSocket *asock);
static void AsyncSocketReleaseSocket(AsyncSocket *s);

static VMwareStatus AsyncSocketIPollAdd(AsyncSocket *asock, Bool socket,
                                        int flags, PollerFunction callback,
                                        int info);
static Bool AsyncSocketIPollRemove(AsyncSocket *asock, Bool socket, int flags,
                                   PollerFunction callback);
static void AsyncSocketIPollSendCallback(void *clientData);
static void AsyncSocketIPollRecvCallback(void *clientData);


static const AsyncSocketVTable asyncStreamSocketVTable = {
   AsyncSocketDispatchConnect,
   AsyncSocketSendInternal,
   AsyncSocketSendSocket,
   AsyncSocketRecvSocket,
   AsyncSocketSendCallback,
   AsyncSocketRecvCallback,
   AsyncSocketHasDataPendingSocket,
   AsyncSocketCancelListenCbSocket,
   AsyncSocketCancelRecvCbSocket,
   AsyncSocketCancelCbForCloseSocket,
   AsyncSocketCancelCbForConnectingCloseSocket,
   AsyncSocketCloseSocket,
   AsyncSocketReleaseSocket,
};


static const AsyncSocketVTable asyncDgramSocketVTable = {
   AsyncSocketDispatchConnect,
   AsyncSocketSendInternal,
   AsyncSocketSendSocket,
   AsyncSocketRecvSocket,
   AsyncSocketSendCallback,
   AsyncSocketRecvUDPCallback,
   AsyncSocketHasDataPendingSocket,
   AsyncSocketCancelListenCbSocket,
   AsyncSocketCancelRecvCbSocket,
   AsyncSocketCancelCbForCloseSocket,
   AsyncSocketCancelCbForConnectingCloseSocket,
   AsyncSocketCloseSocket,
   AsyncSocketReleaseSocket,
};


static const AsyncSocketVTable asyncStreamSocketIPollVTable = {
   AsyncSocketDispatchConnect,
   AsyncSocketSendInternal,
   AsyncSocketSendSocket,
   AsyncSocketRecvSocket,
   AsyncSocketIPollSendCallback,
   AsyncSocketIPollRecvCallback,
   AsyncSocketHasDataPendingSocket,
   AsyncSocketCancelListenCbSocket,
   AsyncSocketCancelRecvCbSocket,
   AsyncSocketCancelCbForCloseSocket,
   AsyncSocketCancelCbForConnectingCloseSocket,
   AsyncSocketCloseSocket,
   AsyncSocketReleaseSocket,
};


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketLock --
 * AsyncSocketUnlock --
 *
 *      Acquire/Release the lock provided by the client when creating the
 *      AsyncSocket object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

INLINE void
AsyncSocketLock(AsyncSocket *asock)   // IN:
{
   if (asock->pollParams.lock) {
      MXUser_AcquireRecLock(asock->pollParams.lock);
   }
}


INLINE void
AsyncSocketUnlock(AsyncSocket *asock)   // IN:
{
   if (asock->pollParams.lock) {
      MXUser_ReleaseRecLock(asock->pollParams.lock);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketIsLocked --
 *
 *      If a lock is associated with the socket, check whether the calling
 *      thread holds the lock.
 *
 * Results:
 *      TRUE if calling thread holds the lock, or if there is no assoicated
 *      lock.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

INLINE Bool
AsyncSocketIsLocked(AsyncSocket *asock)   // IN:
{
   if (asock->pollParams.lock && Poll_LockingEnabled()) {
      return MXUser_IsCurThreadHoldingRecLock(asock->pollParams.lock);
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Init --
 *
 *      Initializes the host's socket library. NOP on Posix.
 *      On Windows, calls WSAStartup().
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      On Windows, loads winsock library.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Init(void)
{
#ifdef _WIN32
   WSADATA wsaData;
   WORD versionRequested = MAKEWORD(2, 0);
   return WSAStartup(versionRequested, &wsaData) ?
             ASOCKERR_GENERIC : ASOCKERR_SUCCESS;
#endif
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Err2String --
 *
 *      Returns the error string associated with error code.
 *
 * Results:
 *      Error string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

const char *
AsyncSocket_Err2String(int err)  // IN
{
   return Msg_StripMSGID(AsyncSocket_MsgError(err));
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_MsgError --
 *
 *      Returns the message associated with error code.
 *
 * Results:
 *      Message string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

const char *
AsyncSocket_MsgError(int asyncSockError)   // IN
{
   const char *result = NULL;
   switch (asyncSockError) {
   case ASOCKERR_SUCCESS:
      result = MSGID(asyncsocket.success) "Success";
      break;
   case ASOCKERR_GENERIC:
      result = MSGID(asyncsocket.generic) "Asyncsocket error";
      break;
   case ASOCKERR_INVAL:
      result = MSGID(asyncsocket.invalid) "Invalid parameters";
      break;
   case ASOCKERR_TIMEOUT:
      result = MSGID(asyncsocket.timeout) "Time-out error";
      break;
   case ASOCKERR_NOTCONNECTED:
      result = MSGID(asyncsocket.notconnected) "Local socket not connected";
      break;
   case ASOCKERR_REMOTE_DISCONNECT:
      result = MSGID(asyncsocket.remotedisconnect) "Remote connection failure";
      break;
   case ASOCKERR_CLOSED:
      result = MSGID(asyncsocket.closed) "Closed socket";
      break;
   case ASOCKERR_CONNECT:
      result = MSGID(asyncsocket.connect) "Connection error";
      break;
   case ASOCKERR_POLL:
      result = MSGID(asyncsocket.poll) "Poll registration error";
      break;
   case ASOCKERR_BIND:
      result = MSGID(asyncsocket.bind) "Socket bind error";
      break;
   case ASOCKERR_BINDADDRINUSE:
      result = MSGID(asyncsocket.bindaddrinuse) "Socket bind address already in use";
      break;
   case ASOCKERR_LISTEN:
      result = MSGID(asyncsocket.listen) "Socket listen error";
      break;
   }

   if (!result) {
      Warning("%s was passed bad code %d\n", __FUNCTION__, asyncSockError);
      result = MSGID(asyncsocket.unknown) "Unknown error";
   }
   return result;
}

/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetFd --
 *
 *      Returns the fd for this socket.
 *
 * Results:
 *      File descriptor.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetFd(AsyncSocket *s)
{
   return s->fd;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetRemoteIPAddress --
 *
 *      Given an AsyncSocket object, returns the remote IP address associated
 *      with it, or an error if the request is meaningless for the underlying
 *      connection.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetRemoteIPAddress(AsyncSocket *asock,      // IN
                               uint32 *ipRet,           // OUT
                               const char **ipRetStr)   // OUT
{
   uint32 ip;
   struct in_addr ipAddr;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(ipRet != NULL || ipRetStr != NULL);

   if ((ipRet == NULL && ipRetStr == NULL) || asock == NULL ||
       asock->state != AsyncSocketConnected ||
       asock->remoteAddrLen != sizeof (struct sockaddr_in)) {
      return ASOCKERR_GENERIC;
   }

   ip = ntohl(((struct sockaddr_in *) &asock->remoteAddr)->sin_addr.s_addr);

   if (ipRet != NULL) {
      *ipRet = ip;
   }

   if (ipRetStr != NULL) {
      ipAddr.s_addr = htonl(ip);
      *ipRetStr = inet_ntoa(ipAddr);
   }

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetLocalVMCIAddress --
 *
 *      Given an AsyncSocket object, returns the local VMCI context ID and
 *      port number associated with it, or an error if the request is
 *      meaningless for the underlying connection.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetLocalVMCIAddress(AsyncSocket *asock,  // IN
                                uint32 *cid,         // OUT: optional
                                uint32 *port)        // OUT: optional
{
   ASSERT(asock);

   if (asock->localAddrLen != sizeof(struct sockaddr_vm)) {
      return ASOCKERR_GENERIC;
   }

   if (cid != NULL) {
      *cid = ((struct sockaddr_vm *)&asock->localAddr)->svm_cid;
   }

   if (port != NULL) {
      *port = ((struct sockaddr_vm *)&asock->localAddr)->svm_port;
   }

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetRemoteVMCIAddress --
 *
 *      Given an AsyncSocket object, returns the remote VMCI context ID and
 *      port number associated with it, or an error if the request is
 *      meaningless for the underlying connection.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetRemoteVMCIAddress(AsyncSocket *asock,  // IN
                                 uint32 *cid,         // OUT: optional
                                 uint32 *port)        // OUT: optional
{
   ASSERT(asock);

   if (asock->remoteAddrLen != sizeof(struct sockaddr_vm)) {
      return ASOCKERR_GENERIC;
   }

   if (cid != NULL) {
      *cid = ((struct sockaddr_vm *)&asock->remoteAddr)->svm_cid;
   }

   if (port != NULL) {
      *port = ((struct sockaddr_vm *)&asock->remoteAddr)->svm_port;
   }

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Listen --
 *
 *      Listens on the specified port and accepts new connections. Fires the
 *      connect callback with new AsyncSocket object for each connection.
 *
 * Results:
 *      New AsyncSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_Listen(unsigned short port,
                   AsyncSocketConnectFn connectFn,
                   void *clientData,
                   AsyncSocketPollParams *pollParams,
                   int *outError)
{
   return AsyncSocket_ListenIP(INADDR_ANY, port, connectFn, clientData,
                               pollParams, outError);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ListenIPStr --
 *
 *      Listens on the specified ip, port and accepts new connections. Fires
 *      the connect callback with new AsyncSocket object for each connection.
 *
 * Results:
 *      New AsyncSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ListenIPStr(const char *ipStr,
                        unsigned short port,
                        AsyncSocketConnectFn connectFn,
                        void *clientData,
                        AsyncSocketPollParams *pollParams,
                        int *outError)
{
   struct in_addr ipAddr;

   /*
    * Windows doesn't have inet_aton so using inet_addr insted. Alternative is
    * ifdef with WSAStringToAddress and inet_aton.
    */

   ipAddr.s_addr = inet_addr(ipStr);
   if (ipAddr.s_addr == INADDR_NONE) {
      if (outError) {
         *outError = INADDR_NONE;
      }

      return NULL;
   }

   return AsyncSocket_ListenIP(ntohl(ipAddr.s_addr), port, connectFn,
                               clientData, pollParams, outError);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ListenIP --
 *
 *      Listens on the specified ip, port and accepts new connections. Fires
 *      the connect callback with new AsyncSocket object for each connection.
 *
 * Results:
 *      New AsyncSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ListenIP(unsigned int ip,
                     unsigned short port,
                     AsyncSocketConnectFn connectFn,
                     void *clientData,
                     AsyncSocketPollParams *pollParams,
                     int *outError)
{
   AsyncSocket *asock = AsyncSocketInit(AF_INET, SOCK_STREAM, pollParams,
                                        outError);

   if (NULL != asock) {
      struct sockaddr_in addr;

      memset((char *)&addr, 0, sizeof addr);
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(ip);
      addr.sin_port = htons(port);

      if (AsyncSocketBind(asock, (struct sockaddr *)&addr, outError) &&
          AsyncSocketListen(asock, connectFn, clientData, outError)) {
         return asock;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ListenVMCI --
 *
 *      Listens on the specified port and accepts new connections. Fires the
 *      connect callback with new AsyncSocket object for each connection.
 *
 * Results:
 *      New AsyncSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ListenVMCI(unsigned int cid,                  // IN
                       unsigned int port,                 // IN
                       AsyncSocketConnectFn connectFn,    // IN
                       void *clientData,                  // IN
                       AsyncSocketPollParams *pollParams, // IN
                       int *outError)                     // OUT
{
   int vsockDev = -1;
   int socketFamily = VMCISock_GetAFValueFd(&vsockDev);
   AsyncSocket *asock = AsyncSocketInit(socketFamily, SOCK_STREAM,
                                        pollParams, outError);

   if (NULL != asock) {
      struct sockaddr_vm addr;

      memset((char *)&addr, 0, sizeof addr);
      addr.svm_family = socketFamily;
      addr.svm_cid = cid;
      addr.svm_port = port;

      if (!AsyncSocketBind(asock, (struct sockaddr *)&addr, outError) ||
          !AsyncSocketListen(asock, connectFn, clientData, outError)) {
         asock = NULL;
      }
   }

   VMCISock_ReleaseAFValueFd(vsockDev);
   return asock;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_BindUDP --
 *
 *      Listens on the specified port and accepts new UDP connections.
 *
 * Results:
 *      New AsyncSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_BindUDP(unsigned short port,
                    void *clientData,
                    AsyncSocketPollParams *pollParams,
                    int *outError)
{
   AsyncSocket *asock = AsyncSocketInit(AF_INET, SOCK_DGRAM, pollParams,
                                        outError);

   if (NULL != asock) {
      struct sockaddr_in addr;

      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
      addr.sin_port = htons(0);

      if (AsyncSocketBind(asock, (struct sockaddr *)&addr, outError)) {
         asock->connectFn = NULL;
         asock->clientData = clientData;
         asock->state = AsyncSocketConnected;

         return asock;
      }
   }

   return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketInit --
 *
 *      This is an internal routine that sets up a socket.
 *
 * Results:
 *      New AsyncSocket or NULL on error.
 *
 * Side effects:
 *      Creates new socket.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocketInit(int socketFamily,                  // IN
                int socketType,                    // IN
                AsyncSocketPollParams *pollParams, // IN
                int *outError)                     // OUT
{
   AsyncSocket *asock = NULL;
   int error = ASOCKERR_GENERIC;
   int sysErr;
   int fd;

   /*
    * Create a new socket
    */

   if ((fd = socket(socketFamily, socketType, 0)) == -1) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "could not create new socket, error %d: %s\n",
              sysErr, Err_Errno2String(sysErr));
      goto errorNoFd;
   }

   /*
    * Wrap it with an asock object
    */

   if ((asock = AsyncSocket_AttachToFd(fd, pollParams, &error)) == NULL) {
      goto error;
   }
   asock->type = socketType;
   asock->asockType = ASYNCSOCKET_TYPE_SOCKET;

   if (SOCK_STREAM == socketType) {
      asock->vt = &asyncStreamSocketVTable;
   } else if (SOCK_DGRAM == socketType) {
      /*
       *  Enable broadcast for UDP.
       */

      int bcast = 1;
      if (setsockopt(asock->fd, SOL_SOCKET, SO_BROADCAST,
                     (const void *) &bcast, sizeof(bcast)) != 0) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "could not set SO_BROADCAST, error %d: %s\n",
                 sysErr, Err_Errno2String(sysErr));
      }

#ifdef _WIN32
      {
         /*
          * On Windows, sending a UDP packet to a host may result in
          * a "connection reset by peer" message to be sent back by
          * the remote machine.  If that happens, our UDP socket becomes
          * useless.  We can disable this with the SIO_UDP_CONNRESET
          * ioctl option.
          */

         DWORD dwBytesReturned = 0;
         BOOL bNewBehavior = FALSE;
         DWORD status;

         status = WSAIoctl(asock->fd, SIO_UDP_CONNRESET,
                           &bNewBehavior, sizeof(bNewBehavior),
                           NULL, 0, &dwBytesReturned,
                           NULL, NULL);

         if (SOCKET_ERROR == status) {
            DWORD dwErr = WSAGetLastError();
            ASOCKLOG(3, asock, ("WSAIoctl(SIO_UDP_CONNRESET) Error: %d\n",
                                          dwErr));
         }
      }
#endif

      asock->vt = &asyncDgramSocketVTable;
   } else {
      NOT_IMPLEMENTED();
   }

   return asock;

error:
   SSLGeneric_close(fd);

errorNoFd:
   if (outError) {
      *outError = error;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketGetPort --
 *
 *      This is an internal routine that gets a port given an address.  The
 *      address must be in either AF_INET or AF_VMCI format.
 *
 * Results:
 *      Returns the port of the address.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
AsyncSocketGetPort(struct sockaddr *addr)
{
   ASSERT(NULL != addr);

   if (AF_INET == addr->sa_family) {
      return ((struct sockaddr_in *)addr)->sin_port;
   } else {
#ifdef VMX86_DEBUG
      int vsockDev = -1;

      ASSERT(VMCISock_GetAFValueFd(&vsockDev) == addr->sa_family);
      VMCISock_ReleaseAFValueFd(vsockDev);
#endif

      return ((struct sockaddr_vm *)addr)->svm_port;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketBind --
 *
 *      This is an internal routine that binds a socket to a port.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon failure.
 *
 * Side effects:
 *      Socket is bound to a particular port.
 *
 *----------------------------------------------------------------------------
 */


Bool
AsyncSocketBind(AsyncSocket *asock,    // IN
                struct sockaddr *addr, // IN
                int *outError)         // OUT
{
   int error = ASOCKERR_BIND;
   int sysErr;
   unsigned int port;

   ASSERT(NULL != asock);
   ASSERT(NULL != asock->sslSock);
   ASSERT(NULL != addr);

   port = AsyncSocketGetPort(addr);
   Log(ASOCKPREFIX "creating new listening socket on port %d\n", port);

#ifndef _WIN32
   /*
    * Don't ever use SO_REUSEADDR on Windows; it doesn't mean what you think
    * it means.
    */

   {
      int reuse = port != 0;

      if (setsockopt(asock->fd, SOL_SOCKET, SO_REUSEADDR,
                     (const void *) &reuse, sizeof(reuse)) != 0) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "could not set SO_REUSEADDR, error %d: %s\n",
                 sysErr, Err_Errno2String(sysErr));
      }
   }
#endif

#ifdef _WIN32
   /*
    * Always set SO_EXCLUSIVEADDRUSE on Windows, to prevent other applications
    * from stealing this socket. (Yes, Windows is that stupid).
    */

   {
      int exclusive = 1;

      if (setsockopt(asock->fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     (const void *) &exclusive, sizeof(exclusive)) != 0) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "could not set SO_REUSEADDR, error %d: %s\n",
                 sysErr, Err_Errno2String(sysErr));
      }
   }
#endif

   /*
    * Bind to a port
    */

   if (bind(asock->fd, addr, sizeof *addr) != 0) {
      sysErr = ASOCK_LASTERROR();
      if (sysErr == ASOCK_EADDRINUSE) {
         error = ASOCKERR_BINDADDRINUSE;
      }
      Warning("could not bind socket, error %d: %s\n", sysErr,
              Err_Errno2String(sysErr));
      goto error;
   }

   return TRUE;

error:
   SSL_Shutdown(asock->sslSock);
   free(asock);

   if (outError) {
      *outError = error;
   }

   return FALSE;
}

/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketListen --
 *
 *      This is an internal routine that calls listen() on a socket.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon failure.
 *
 * Side effects:
 *      Socket is in listening state.
 *
 *----------------------------------------------------------------------------
 */


Bool
AsyncSocketListen(AsyncSocket *asock,                // IN
                  AsyncSocketConnectFn connectFn,    // IN
                  void *clientData,                  // IN
                  int *outError)                     // OUT
{
   VMwareStatus pollStatus;
   int error;

   ASSERT(NULL != asock);
   ASSERT(NULL != asock->sslSock);
   ASSERT(SOCK_STREAM == asock->type);

   if (!connectFn) {
      Warning(ASOCKPREFIX "invalid arguments to listen!\n");
      error = ASOCKERR_INVAL;
      goto error;
   }

   /*
    * Listen on the socket
    */

   if (listen(asock->fd, 5) != 0) {
      int sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "could not listen on socket, error %d: %s\n",
              sysErr, Err_Errno2String(sysErr));
      error = ASOCKERR_LISTEN;
      goto error;
   }

   /*
    * Register a read callback to fire each time the socket
    * is ready for accept.
    */

   AsyncSocketLock(asock);
   pollStatus = AsyncSocketPollAdd(asock, TRUE,
                                   POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                   AsyncSocketAcceptCallback);

   if (pollStatus != VMWARE_STATUS_SUCCESS) {
      ASOCKWARN(asock, ("could not register accept callback!\n"));
      error = ASOCKERR_POLL;
      AsyncSocketUnlock(asock);
      goto error;
   }
   asock->state = AsyncSocketListening;

   asock->connectFn = connectFn;
   asock->clientData = clientData;
   AsyncSocketUnlock(asock);

   return TRUE;

error:
   SSL_Shutdown(asock->sslSock);
   free(asock);

   if (outError) {
      *outError = error;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Connect --
 *
 *      AsyncSocket AF_INET constructor.  This is just a wrapper for ConnectIP
 *      that does hostname -> IP address lookup.
 *
 *      NOTE: This function can block.
 *
 * Results:
 *      AsyncSocket * on success and NULL on failure.
 *      On failure, error is returned in *outError.
 *
 * Side effects:
 *      Allocates an AsyncSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_Connect(const char *hostname,
                    unsigned short port,
                    AsyncSocketConnectFn connectFn,
                    void *clientData,
                    AsyncSocketConnectFlags flags,
                    AsyncSocketPollParams *pollParams,
                    int *outError)
{
   struct sockaddr_in addr;
   int getaddrinfoError;
   int error;
   AsyncSocket *asock;

   if (!connectFn || !hostname) {
      error = ASOCKERR_INVAL;
      Warning(ASOCKPREFIX "invalid arguments to connect!\n");
      goto error;
   }

   /*
    * Resolve the hostname.  Handles dotted decimal strings, too.
    */

   getaddrinfoError = AsyncSocketResolveAddr(hostname, port,
                                             SOCK_STREAM, &addr);
   if (0 != getaddrinfoError) {
      Log(ASOCKPREFIX "Failed to resolve address '%s' and port %u\n",
          hostname, port);
      error = ASOCKERR_CONNECT;
      goto error;
   }

   /* Only IPv4 for now.  Change this when IPv6 support is added. */
   ASSERT(addr.sin_family == AF_INET);

   {
      uint32 ip;
      ip = ntohl(addr.sin_addr.s_addr);
      Log(ASOCKPREFIX "creating new socket, connecting to %u.%u.%u.%u:%u (%s)\n",
          (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
          port, hostname);
   }

   asock = AsyncSocketConnect((struct sockaddr *)&addr, sizeof addr,
                              connectFn, clientData, AsyncSocketConnectCallback,
                              flags, pollParams, &error);
   if (!asock) {
      Warning(ASOCKPREFIX "connection attempt failed\n");
      error = ASOCKERR_CONNECT;
      goto error;
   }

   return asock;

error:
   if (outError) {
      *outError = error;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectIP --
 *
 *      AsyncSocket AF_INET constructor. Connects to the specified
 *      address:port, and passes the caller a valid asock via the callback once
 *      the connection has been established.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ConnectIP(uint32 ip,
                      unsigned short port,
                      AsyncSocketConnectFn connectFn,
                      void *clientData,
                      AsyncSocketConnectFlags flags,
                      AsyncSocketPollParams *pollParams,
                      int *outError)
{
   struct sockaddr_in addr;

   memset((char *)&addr, 0, sizeof addr);
   addr.sin_family = AF_INET;
   if ((flags & ASOCKCONN_ADDR_IN_NETWORK_BYTE_ORDER) !=
         ASOCKCONN_ADDR_IN_NETWORK_BYTE_ORDER) {
      addr.sin_addr.s_addr = htonl(ip);
   } else {
      addr.sin_addr.s_addr = ip;
   }
   addr.sin_port = htons(port);

   Log(ASOCKPREFIX "creating new socket, connecting to %u.%u.%u.%u:%u\n",
       (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
       port);

   return AsyncSocketConnect((struct sockaddr *)&addr, sizeof addr,
                             connectFn, clientData, AsyncSocketConnectCallback,
                             flags, pollParams, outError);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectIPStr --
 *
 *      Like AsyncSocket_Connect but operates on an IP address string.
 *
 * Results:
 *      See AsyncSocket_ConnectIP.
 *
 * Side effects:
 *      See AsyncSocket_ConnectIP.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ConnectIPStr(const char *ipStr,
                         unsigned short port,
                         AsyncSocketConnectFn connectFn,
                         void *clientData,
                         AsyncSocketConnectFlags flags,
                         AsyncSocketPollParams *pollParams,
                         int *outError)
{
   struct in_addr ipAddr;

   /*
    * Windows doesn't have inet_aton so using inet_addr insted. Alternative is
    * ifdef with WSAStringToAddress and inet_aton.
    */

   ipAddr.s_addr = inet_addr(ipStr);
   if (ipAddr.s_addr == INADDR_NONE) {
      if (outError) {
         *outError = INADDR_NONE;
      }

      return NULL;
   }

   return AsyncSocket_ConnectIP(ntohl(ipAddr.s_addr), port, connectFn,
                                clientData, flags, pollParams, outError);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectVMCI --
 *
 *      AsyncSocket AF_VMCI constructor. Connects to the specified cid:port,
 *      and passes the caller a valid asock via the callback once the
 *      connection has been established.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ConnectVMCI(unsigned int cid,                  // IN
                        unsigned int port,                 // IN
                        AsyncSocketConnectFn connectFn,    // IN
                        void *clientData,                  // IN
                        AsyncSocketConnectFlags flags,     // IN
                        AsyncSocketPollParams *pollParams, // IN
                        int *outError)                     // OUT
{
   int vsockDev = -1;
   struct sockaddr_vm addr;
   AsyncSocket *asock;

   memset((char *)&addr, 0, sizeof addr);
   addr.svm_family = VMCISock_GetAFValueFd(&vsockDev);
   addr.svm_cid = cid;
   addr.svm_port = port;

   Log(ASOCKPREFIX "creating new socket, connecting to %u:%u\n", cid, port);

   asock = AsyncSocketConnect((struct sockaddr *)&addr, sizeof addr,
                              connectFn, clientData, AsyncSocketConnectCallback,
                              flags, pollParams, outError);

   VMCISock_ReleaseAFValueFd(vsockDev);
   return asock;
}


#ifndef _WIN32
/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectUnixDomain --
 *
 *      AsyncSocket AF_UNIX constructor. Connects to the specified unix socket,
 *      and passes the caller a valid asock via the callback once the
 *      connection has been established.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ConnectUnixDomain(const char *path,                  // IN
                              AsyncSocketConnectFn connectFn,    // IN
                              void *clientData,                  // IN
                              AsyncSocketConnectFlags flags,     // IN
                              AsyncSocketPollParams *pollParams, // IN
                              int *outError)                     // OUT
{
   struct sockaddr_un addr;
   AsyncSocket *asock;

   memset((char *)&addr, 0, sizeof addr);
   addr.sun_family = AF_UNIX;

   if (strlen(path) + 1 > sizeof addr.sun_path) {
      Warning(ASOCKPREFIX "Path '%s' is too long for a unix domain socket!\n", path);
      return NULL;
   }
   Str_Strcpy(addr.sun_path, path, sizeof addr.sun_path);

   Log(ASOCKPREFIX "creating new socket, connecting to %s\n", path);

   asock = AsyncSocketConnect((struct sockaddr *)&addr, sizeof addr,
                              connectFn, clientData, AsyncSocketConnectCallback,
                              flags, pollParams, outError);

   return asock;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketConnect --
 * AsyncSocketConnectWithAsock --
 *
 *      Internal AsyncSocket constructor.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

static AsyncSocket *
AsyncSocketConnect(struct sockaddr *addr,
                   socklen_t addrLen,
                   AsyncSocketConnectFn connectFn,
                   void *clientData,
                   PollerFunction internalConnectFn,
                   AsyncSocketConnectFlags flags,
                   AsyncSocketPollParams *pollParams,
                   int *outError)
{
   int fd;
   AsyncSocket *asock = NULL;
   int error = ASOCKERR_GENERIC;
   int sysErr;
   int socketFamily;

   ASSERT(addr);

   if (!connectFn) {
      error = ASOCKERR_INVAL;
      Warning(ASOCKPREFIX "invalid arguments to connect!\n");
      goto error;
   }

   if (!internalConnectFn) {
      error = ASOCKERR_INVAL;
      Warning(ASOCKPREFIX "invalid arguments to connect!\n");
      goto error;
   }

   socketFamily = addr->sa_family;

   /*
    * Create a new IP socket
    */
   if ((fd = socket(socketFamily, SOCK_STREAM, 0)) == -1) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "failed to create socket, error %d: %s\n",
              sysErr, Err_Errno2String(sysErr));
      error = ASOCKERR_CONNECT;
      goto error;
   }

   /*
    * Wrap it with an asock
    */

   if ((asock = AsyncSocket_AttachToFd(fd, pollParams, &error)) == NULL) {
      SSLGeneric_close(fd);
      goto error;
   }

   return AsyncSocketConnectWithAsock(asock, addr, addrLen,
                                      connectFn, clientData, internalConnectFn,
                                      pollParams, outError);

error:
   if (outError) {
      *outError = error;
   }

   return NULL;
}

AsyncSocket *
AsyncSocketConnectWithAsock(AsyncSocket *asock,
                            struct sockaddr *addr,
                            socklen_t addrLen,
                            AsyncSocketConnectFn connectFn,
                            void *clientData,
                            PollerFunction internalConnectFn,
                            AsyncSocketPollParams *pollParams,
                            int *outError)
{
   VMwareStatus pollStatus;
   int sysErr;
   int error = ASOCKERR_GENERIC;

   /*
    * Call connect(), which can either succeed immediately or return an error
    * indicating that the connection is in progress. In the latter case, we
    * can poll the fd for write to find out when the connection attempt
    * has succeeded (or failed). In either case, we want to invoke the
    * caller's connect callback from Poll rather than directly, so if the
    * connection succeeds immediately, we just schedule the connect callback
    * as a one-time (RTime) callback instead.
    */

   AsyncSocketLock(asock);
   if (connect(asock->fd, addr, addrLen) != 0) {
      if (ASOCK_LASTERROR() == ASOCK_ECONNECTING) {
         ASSERT(!(vmx86_server && addr->sa_family == AF_UNIX));
         ASOCKLOG(1, asock, ("registering write callback for socket connect\n"));
         pollStatus = AsyncSocketPollAdd(asock, TRUE, POLL_FLAG_WRITE,
                                         internalConnectFn);
      } else {
         sysErr = ASOCK_LASTERROR();
         Log(ASOCKPREFIX "connect failed, error %d: %s\n",
             sysErr, Err_Errno2String(sysErr));
         error = ASOCKERR_CONNECT;
         goto errorHaveAsock;
      }
   } else {
      ASOCKLOG(2, asock,
               ("socket connected, registering RTime callback for connect\n"));
      pollStatus = AsyncSocketPollAdd(asock, FALSE, 0,
                                      internalConnectFn, 0);
   }

   if (pollStatus != VMWARE_STATUS_SUCCESS) {
      ASOCKWARN(asock, ("failed to register callback in connect!\n"));
      error = ASOCKERR_POLL;
      goto errorHaveAsock;
   }

   asock->state = AsyncSocketConnecting;
   asock->connectFn = connectFn;
   asock->clientData = clientData;
   asock->type = SOCK_STREAM;

   /* Store a copy of the sockaddr so we can look it up later. */
   asock->remoteAddr = *addr;
   asock->remoteAddrLen = addrLen;

   AsyncSocketUnlock(asock);

   return asock;

errorHaveAsock:
   SSL_Shutdown(asock->sslSock);
   AsyncSocketUnlock(asock);
   free(asock);

   if (outError) {
      *outError = error;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCreate --
 *
 *      AsyncSocket constructor for fields common to all AsyncSocket types.
 *
 * Results:
 *      New AsyncSocket object.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocketCreate(AsyncSocketPollParams *pollParams) // IN
{
   AsyncSocket *s;

   s = Util_SafeCalloc(1, sizeof *s);
   s->id = Atomic_FetchAndInc(&nextid);
   s->state = AsyncSocketConnected;
   s->refCount = 1;
   s->inRecvLoop = FALSE;
   s->sendBufFull = FALSE;
   s->sendBufTail = &(s->sendBufList);
   s->passFd.fd = -1;

   if (pollParams) {
      s->pollParams = *pollParams;
   } else {
      s->pollParams.pollClass = POLL_CS_MAIN;
      s->pollParams.flags = 0;
      s->pollParams.lock = NULL;
      s->pollParams.iPoll = NULL;
   }

   return s;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_AttachToSSLSock --
 *
 *      AsyncSocket constructor. Wraps an existing SSLSock object with an
 *      AsyncSocket and returns the latter.
 *
 * Results:
 *      New AsyncSocket object or NULL on error.
 *
 * Side effects:
 *      Allocates memory, makes the underlying fd for the socket non-blocking.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_AttachToSSLSock(SSLSock sslSock,
                            AsyncSocketPollParams *pollParams,
                            int *outError)
{
   AsyncSocket *s;
   int fd;
   int error;

   ASSERT(sslSock);

   fd = SSL_GetFd(sslSock);

   if ((AsyncSocketMakeNonBlocking(fd)) != ASOCKERR_SUCCESS) {
      int sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "failed to make fd %d non-blocking!: %d, %s\n",
              fd, sysErr, Err_Errno2String(sysErr));
      error = ASOCKERR_GENERIC;
      goto error;
   }

   s = AsyncSocketCreate(pollParams);
   s->sslSock = sslSock;
   s->fd = fd;
   s->type = SOCK_STREAM;
   s->asockType = ASYNCSOCKET_TYPE_SOCKET;
   if (s->pollParams.iPoll == NULL) {
      s->vt = &asyncStreamSocketVTable;
   } else {
      s->vt = &asyncStreamSocketIPollVTable;
   }

   /* From now on socket is ours. */
   SSL_SetCloseOnShutdownFlag(sslSock);
   ASOCKLOG(1, s, ("new asock id %u attached to fd %d\n", s->id, s->fd));

   return s;

error:
   if (outError) {
      *outError = error;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_AttachToFd --
 *
 *      AsyncSocket constructor. Wraps a valid socket fd with an AsyncSocket
 *      object.
 *
 * Results:
 *      New AsyncSocket or NULL on error.
 *
 * Side effects:
 *      If function succeeds, fd is owned by AsyncSocket and should not be
 *      used (f.e. closed) anymore.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_AttachToFd(int fd,
                       AsyncSocketPollParams *pollParams,
                       int *outError)
{
   SSLSock sslSock;
   AsyncSocket *asock;

   /*
    * Create a new SSL socket object with the current socket
    */

   if (!(sslSock = SSL_New(fd, FALSE))) {
      if (outError) {
         *outError = ENOMEM;
      }
      LOG(0, (ASOCKPREFIX "failed to create SSL socket object\n"));

      return NULL;
   }
   asock = AsyncSocket_AttachToSSLSock(sslSock, pollParams, outError);
   if (asock) {
      return asock;
   }
   SSL_Shutdown(sslSock);

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_UseNodelay --
 *
 *      Sets or unset TCP_NODELAY on the socket, which disables or
 *      enables Nagle's algorithm, respectively.
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_GENERIC otherwise.
 *
 * Side Effects:
 *      Increased bandwidth usage for short messages on this socket
 *      due to TCP overhead, in exchange for lower latency.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_UseNodelay(AsyncSocket *asock,  // IN/OUT:
                       Bool nodelay)        // IN:
{
   int flag = nodelay ? 1 : 0;

   AsyncSocketLock(asock);
   if (setsockopt(asock->fd, IPPROTO_TCP, TCP_NODELAY,
                  (const void *) &flag, sizeof(flag)) != 0) {
      asock->genericErrno = Err_Errno();
      LOG(0, (ASOCKPREFIX "could not set TCP_NODELAY, error %d: %s\n",
              Err_Errno(), Err_ErrString()));
      AsyncSocketUnlock(asock);
      return ASOCKERR_GENERIC;
   } else {
      AsyncSocketUnlock(asock);
      return ASOCKERR_SUCCESS;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetTCPTimeouts --
 *
 *      Allow caller to set a number of TCP-specific timeout
 *      parameters on the socket for the active connection.
 *
 *      Parameters:
 *      keepIdle --  The number of seconds a TCP connection must be idle before
 *                   keep-alive probes are sent.
 *      keepIntvl -- The number of seconds between TCP keep-alive probes once
 *                   they are being sent.
 *      keepCnt   -- The number of keep-alive probes to send before killing
 *                   the connection if no response is received from the peer.
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_GENERIC otherwise.
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VMX86_SERVER
int
AsyncSocket_SetTCPTimeouts(AsyncSocket *asock,  // IN/OUT:
                           int keepIdle,        // IN
                           int keepIntvl,       // IN
                           int keepCnt)         // IN
{
   int val;
   int opt;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   AsyncSocketLock(asock);

   val = keepIdle;
   opt = TCP_KEEPIDLE;
   if (setsockopt(asock->fd, IPPROTO_TCP, opt,
                  &val, sizeof val) != 0) {
      goto error;
   }

   val = keepIntvl;
   opt = TCP_KEEPINTVL;
   if (setsockopt(asock->fd, IPPROTO_TCP, opt,
                  &val, sizeof val) != 0) {
      goto error;
   }

   val = keepCnt;
   opt = TCP_KEEPCNT;
   if (setsockopt(asock->fd, IPPROTO_TCP, opt,
                  &val, sizeof val) != 0) {
      goto error;
   }

   AsyncSocketUnlock(asock);
   return ASOCKERR_SUCCESS;

error:
   asock->genericErrno = Err_Errno();
   LOG(0, (ASOCKPREFIX "could not set TCP Timeout %d, error %d: %s\n",
           opt, Err_Errno(), Err_ErrString()));
   AsyncSocketUnlock(asock);
   return ASOCKERR_GENERIC;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketRecvSocket --
 *
 *      Does the socket specific portion of a AsyncSocket_Recv call.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Could register poll callback.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketRecvSocket(AsyncSocket *asock, // IN:
                      void *buf,          // IN: unused
                      int len)            // IN: unused
{
   int retVal = ASOCKERR_SUCCESS;

   if (!asock->recvBuf && !asock->recvCb) {
      VMwareStatus pollStatus;

      /*
       * Register the Poll callback
       */

      ASOCKLOG(3, asock, ("installing recv poll callback\n"));

      pollStatus = AsyncSocketPollAdd(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      asock->vt->recvCallback);

      if (pollStatus != VMWARE_STATUS_SUCCESS) {
         ASOCKWARN(asock, ("failed to install recv callback!\n"));
         retVal = ASOCKERR_POLL;
         goto out;
      }
      asock->recvCb = TRUE;
   }

   if (SOCK_STREAM == asock->type &&
       (AsyncSocketHasDataPending(asock) && !asock->inRecvLoop)) {
      ASOCKLOG(0, asock, ("installing recv RTime poll callback\n"));
      if (Poll_CB_RTime(asock->vt->recvCallback,
                        asock, 0, FALSE, NULL) !=
          VMWARE_STATUS_SUCCESS) {
         retVal = ASOCKERR_POLL;
         goto out;
      }
   }

out:
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Recv --
 * AsyncSocket_RecvPartial --
 *
 *      Registers a callback that will fire once the specified amount of data
 *      has been received on the socket.  This handles both TCP and UDP.
 *
 *      In the case of AsyncSocket_RecvPartial, the callback is fired
 *      once all or part of the data has been received on the socket.
 *
 *      Data that was not retrieved at the last call of SSL_read() could still
 *      be buffered inside the SSL layer and will be retrieved on the next
 *      call to SSL_read(). However poll/select might not mark the socket as
 *      for reading since there might not be any data in the underlying network
 *      socket layer. Hence in the read callback, we keep spinning until all
 *      all the data buffered inside the SSL layer is retrieved before
 *      returning to the poll loop (See AsyncSocketFillRecvBuffer()).
 *
 *      However, we might not have come out of Poll in the first place, e.g.
 *      if this is the first call to AsyncSocket_Recv() after creating a new
 *      connection. In this situation, if there is buffered SSL data pending,
 *      we have to schedule an RTTime callback to force retrieval of the data.
 *      This could also happen if the client calls AsyncSocket_RecvBlocking,
 *      some data is left in the SSL layer, and the client then calls
 *      AsyncSocket_Recv. We use the inRecvLoop variable to detect and handle
 *      this condition, i.e., if inRecvLoop is FALSE, we need to schedule the
 *      RTime callback.
 *
 *      This can be used with either TCP or UDP sockets.
 *
 *      TCP usage:
 *      AsyncSocket_Recv(AsyncSocket *asock,
 *                       void *buf,
 *                       int len,
 *                       AsyncSocketRecvFn recvFn,
 *                       void *clientData)
 *
 *      UDP usage:
 *      AsyncSocket_Recv(AsyncSocket *asock,
 *                       void *buf,
 *                       int len,
 *                       AsyncSocketRecvUDPFn recvFn,
 *                       void *clientData)
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Could register poll callback.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Recv(AsyncSocket *asock,
                        void *buf,
                        int len,
                        void *cb,
                        void *cbData)
{
   return AsyncSocketRecv(asock, buf, len, FALSE, cb, cbData);
}

int
AsyncSocket_RecvPartial(AsyncSocket *asock,
                        void *buf,
                        int len,
                        void *cb,
                        void *cbData)
{
   /*
    * Not yet implemented/tested on windows named pipe (though support
    * there should be easy).
    */
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   return AsyncSocketRecv(asock, buf, len, TRUE, cb, cbData);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketRecv --
 *
 *      Internal function to implement AsyncSocket_Recv and
 *      AsyncSocket_RecvPartial.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Could register poll callback.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketRecv(AsyncSocket *asock,  // IN:
                void *buf,           // IN: unused
                int len,             // IN: unused
                Bool fireOnPartial,  // IN:
                void *cb,            // IN:
                void *cbData)        // IN:
{
   AsyncSocketRecvFn recvFn = NULL;
   AsyncSocketRecvUDPFn recvUDPFn = NULL;
   void *clientData = NULL;
   int retVal;

   if (!asock) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   if (!asock->errorFn) {
      ASOCKWARN(asock, ("%s: no registered error handler!\n", __FUNCTION__));

      return ASOCKERR_INVAL;
   }

   if (SOCK_STREAM == asock->type) {
      recvFn = cb;
      clientData = cbData;
   } else if (SOCK_DGRAM == asock->type) {
      recvUDPFn = cb;
      clientData = cbData;
   } else {
      /*
       * If this isn't TCP or UDP, it isn't supported.
       */

      return ASOCKERR_INVAL;
   }

   /*
    * XXX We might want to allow passing NULL for the recvFn, to indicate that
    *     the client is no longer interested in reading from the socket. This
    *     would be useful e.g. for HTTP, where the client sends a request and
    *     then the client->server half of the connection is closed.
    */

   if (!buf || (!recvFn && !recvUDPFn) || len <= 0) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(asock);

   if (asock->state != AsyncSocketConnected) {
      ASOCKWARN(asock, ("recv called but state is not connected!\n"));
      retVal = ASOCKERR_NOTCONNECTED;
      goto outHaveLock;
   }

   if (asock->inBlockingRecv) {
      ASOCKWARN(asock, ("Recv called while a blocking recv is pending.\n"));
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   if (asock->recvBuf && asock->recvPos != 0) {
      ASOCKWARN(asock, ("Recv called -- partially read buffer discarded.\n"));
   }

   ASSERT(asock->vt);
   ASSERT(asock->vt->recv);
   retVal = asock->vt->recv(asock, buf, len);
   if (retVal != ASOCKERR_SUCCESS) {
      goto outHaveLock;
   }

   asock->recvBuf = buf;
   asock->recvFn = recvFn;
   asock->recvUDPFn = recvUDPFn;
   asock->recvLen = len;
   asock->recvFireOnPartial = fireOnPartial;
   asock->recvPos = 0;
   asock->clientData = clientData;
   retVal = ASOCKERR_SUCCESS;

outHaveLock:
   AsyncSocketUnlock(asock);
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_RecvPassedFd --
 *
 *      See AsyncSocket_Recv.  Besides that it allows for receiving one
 *      file descriptor...
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Could register poll callback.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_RecvPassedFd(AsyncSocket *asock,  // IN/OUT: socket
                         void *buf,           // OUT: buffer with data
                         int len,             // IN: length
                         void *cb,            // IN: completion calback
                         void *cbData)        // IN: callback's data
{
   int err;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (!asock) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   if (!asock->errorFn) {
      ASOCKWARN(asock, ("%s: no registered error handler!\n", __FUNCTION__));

      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(asock);
   if (asock->passFd.fd != -1) {
      SSLGeneric_close(asock->passFd.fd);
      asock->passFd.fd = -1;
   }
   asock->passFd.expected = TRUE;

   err = AsyncSocket_Recv(asock, buf, len, cb, cbData);
   if (err != ASOCKERR_SUCCESS) {
      asock->passFd.expected = FALSE;
   }
   AsyncSocketUnlock(asock);

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketPoll --
 *
 *      Blocks on the specified socket until there's data pending or a
 *      timeout occurs.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on system call
 *        failures
 *      ASOCKERR_TIMEOUT if we just didn't receive enough data.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

static int
AsyncSocketPoll(AsyncSocket *s,  // IN:
                Bool read,       // IN:
                int timeoutMS)   // IN:
{
#ifndef _WIN32
   struct pollfd p;
   int retval;
#else
   /*
    * We use select() to do this on Windows, since there ain't no poll().
    * Fortunately, select() doesn't have the 1024 fd value limit.
    */

   int retval;
   struct timeval tv;
   struct fd_set rwfds;
   struct fd_set exceptfds;
#endif

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (read && SSL_Pending(s->sslSock)) {
      return ASOCKERR_SUCCESS;
   }

   while (1) {

#ifndef _WIN32
      p.fd = s->fd;
      p.events = read ? POLLIN : POLLOUT;

      retval = poll(&p, 1, timeoutMS);
#else
      tv.tv_sec = timeoutMS / 1000;
      tv.tv_usec = (timeoutMS % 1000) * 1000;

      FD_ZERO(&rwfds);
      FD_ZERO(&exceptfds);
      FD_SET(s->fd, &rwfds);
      FD_SET(s->fd, &exceptfds);

      retval = select(1, read ? &rwfds : NULL, read ? NULL : &rwfds,
                      &exceptfds, timeoutMS >= 0 ? &tv : NULL);
#endif
      switch (retval) {

      case 1:
#ifndef _WIN32
         if (p.revents & (POLLERR | POLLNVAL))
#else
            if (FD_ISSET(s->fd, &exceptfds))
#endif
            {
               int sockErr = 0, sysErr, sockErrLen = sizeof sockErr;

               ASOCKLG0(s, ("%s on fd %d failed\n", __FUNCTION__, s->fd));

               if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR,
                              (void *) &sockErr, (void *) &sockErrLen) == 0) {
                  if (sockErr) {
                     s->genericErrno = sockErr;
                     ASOCKLG0(s, ("getsockopt error lookup returned %d: %s\n",
                                  sockErr, Err_Errno2String(sockErr)));
                  }
               } else {
                  sysErr = ASOCK_LASTERROR();
                  s->genericErrno = sysErr;
                  ASOCKLG0(s, ("getsockopt failed with error %d: %s\n", sysErr,
                               Err_Errno2String(sysErr)));
               }

               return ASOCKERR_GENERIC;
            }

         /*
          * One socket was ready, and it wasn't in an exception state, so
          * everything is ok. The socket is ready for reading/writing.
          */

         return ASOCKERR_SUCCESS;

      case 0:
         /*
          * No sockets were ready within the specified time.
          */
         return ASOCKERR_TIMEOUT;

      case -1:
         if (ASOCK_LASTERROR() == EINTR) {
            /*
             * We were somehow interrupted by signal. Let's loop and retry.
             */

            continue;
         }
         s->genericErrno = ASOCK_LASTERROR();

         return ASOCKERR_GENERIC;
      default:
         NOT_REACHED();
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_RecvBlocking --
 * AsyncSocket_RecvPartialBlocking --
 * AsyncSocket_SendBlocking --
 *
 *      Implement "blocking + timeout" operations on the socket. These are
 *      simple wrappers around the AsyncSocketBlockingWork function, which
 *      operates on the actual non-blocking socket, using poll to determine
 *      when it's ok to keep reading/writing. If we can't finish within the
 *      specified time, we give up and return the ASOCKERR_TIMEOUT error.
 *
 *      Note that if these are called from a callback and a lock is being
 *      used (pollParams.lock), the whole blocking operation takes place
 *      with that lock held.  Regardless, it is the caller's responsibility
 *      to make sure the synchronous and asynchronous operations do not mix.
 *
 * Results:
 *      ASOCKERR_SUCCESS if we finished the operation, ASOCKERR_* error codes
 *      otherwise.
 *
 * Side effects:
 *      Reads/writes the socket.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_RecvBlocking(AsyncSocket *s,
                         void *buf,
                         int len,
                         int *received,
                         int timeoutMS)
{
   return AsyncSocketBlockingWork(s, TRUE, buf, len, received, timeoutMS, FALSE);
}

int
AsyncSocket_RecvPartialBlocking(AsyncSocket *s,
                                void *buf,
                                int len,
                                int *received,
                                int timeoutMS)
{
   return AsyncSocketBlockingWork(s, TRUE, buf, len, received, timeoutMS, TRUE);
}

int
AsyncSocket_SendBlocking(AsyncSocket *s,
                         void *buf,
                         int len,
                         int *sent,
                         int timeoutMS)
{
   return AsyncSocketBlockingWork(s, FALSE, buf, len, sent, timeoutMS, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketBlockingWork --
 *
 *      Try to complete the specified read/write operation within the
 *      specified time.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocketBlockingWork(AsyncSocket *s,  // IN:
                        Bool read,       // IN:
                        void *buf,       // IN/OUT:
                        int len,         // IN:
                        int *completed,  // OUT:
                        int timeoutMS,   // IN:
                        Bool partial)    // IN:
{
   VmTimeType now, done;
   int sysErr;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (s == NULL || buf == NULL || len <= 0) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   if (s->state != AsyncSocketConnected) {
      ASOCKWARN(s, ("recv called but state is not connected!\n"));

      return ASOCKERR_NOTCONNECTED;
   }

   if (completed) {
      *completed = 0;
   }
   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;
   do {
      int numBytes, error;

      if ((error = AsyncSocketPoll(s, read, done - now)) != ASOCKERR_SUCCESS) {
         return error;
      }

      if ((numBytes = read ? SSL_Read(s->sslSock, buf, len)
                           : SSL_Write(s->sslSock, buf, len)) > 0) {
         if (completed) {
            *completed += numBytes;
         }
         len -= numBytes;
         if (len == 0 || partial) {
            return ASOCKERR_SUCCESS;
         }
         buf = (uint8*)buf + numBytes;
      } else if (numBytes == 0) {
         ASOCKLG0(s, ("blocking %s detected peer closed connection\n",
                      read ? "recv" : "send"));
         return ASOCKERR_REMOTE_DISCONNECT;
      } else if ((sysErr = ASOCK_LASTERROR()) != ASOCK_EWOULDBLOCK) {
         s->genericErrno = sysErr;
         ASOCKWARN(s, ("blocking %s error %d: %s\n",
                       read ? "recv" : "send",
                       sysErr, Err_Errno2String(sysErr)));

         return ASOCKERR_GENERIC;
      }

      now = Hostinfo_SystemTimerUS() / 1000;
   } while ((now < done && timeoutMS > 0) || (timeoutMS < 0));

   return ASOCKERR_TIMEOUT;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSendSocket --
 *
 *      Does the socket specific portion of a AsyncSocket_Send call.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      May register poll callback or perform I/O.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketSendSocket(AsyncSocket *asock,      // IN:
                      Bool bufferListWasEmpty, // IN:
                      void *buf,               // IN: unused
                      int len)                 // IN: unused
{
   int retVal = ASOCKERR_SUCCESS;

   if (bufferListWasEmpty && !asock->sendCb) {
#ifdef _WIN32
      /*
       * If the send buffer list was empty, we schedule a one-time callback
       * to "prime" the output. This is necessary to support the FD_WRITE
       * network event semantic for sockets on Windows (see WSAEventSelect
       * documentation). The event won't signal unless a previous write() on
       * the socket failed with WSAEWOULDBLOCK, so we have to perform at
       * least one partial write before we can start polling for write.
       *
       * XXX: This can be a device callback once all poll implementations
       * know to get around this Windows quirk.  Both PollVMX and PollDefault
       * already make 0-byte send() to force WSAEWOULDBLOCK.
       */

      if (AsyncSocketPollAdd(asock, FALSE, 0, asock->vt->sendCallback, 0)
          != VMWARE_STATUS_SUCCESS) {
         retVal = ASOCKERR_POLL;
         return retVal;
      }
      asock->sendCbTimer = TRUE;
#else
      /*
       * For non-Windows platforms, just schedule a regular device callback.
       */

      if (AsyncSocketPollAdd(asock, TRUE, POLL_FLAG_WRITE,
                             asock->vt->sendCallback)
          != VMWARE_STATUS_SUCCESS) {
         retVal = ASOCKERR_POLL;
         return retVal;
      }
#endif
      asock->sendCb = TRUE;
   }

   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Send --
 *
 *      Queues the provided data for sending on the socket. If a send callback
 *      is provided, the callback is fired after the data has been written to
 *      the socket. Note that this only guarantees that the data has been
 *      copied to the transmit buffer, we make no promises about whether it
 *      has actually been transmitted, or received by the client, when the
 *      callback is fired.
 *
 *      Send callbacks should also be able to deal with being called if none
 *      or only some of the queued buffer has been transmitted, since the send
 *      callbacks for any remaining buffers are fired by AsyncSocket_Close().
 *      This condition can be detected by checking the len parameter passed to
 *      the send callback.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      May register poll callback or perform I/O.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Send(AsyncSocket *asock,
                 void *buf,
                 int len,
                 AsyncSocketSendFn sendFn,
                 void *clientData)
{
   int retVal;
   SendBufList *listBeforeAppend = asock->sendBufList;
   Bool bufferListWasEmpty = FALSE;

   /*
    * Note: I think it should be fine to send with a length of zero and a
    * buffer of NULL or any other garbage value.  However the code
    * downstream of here is unprepared for it (silently misbehaves).  Hence
    * the <= zero check instead of just a < zero check.  --Jeremy.
    */

   if (!asock || !buf || len <= 0) {
      Warning(ASOCKPREFIX "Send called with invalid arguments! asynchSock: %p "
              "buffer: %p length: %d\n", asock, buf, len);

      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(asock);

   ASSERT(SOCK_STREAM == asock->type);

   if (asock->state != AsyncSocketConnected) {
      ASOCKWARN(asock, ("send called but state is not connected!\n"));
      retVal = ASOCKERR_NOTCONNECTED;
      goto outHaveLock;
   }

   ASSERT(asock->vt);
   ASSERT(asock->vt->prepareSend);
   retVal = asock->vt->prepareSend(asock, buf, len,
                                   sendFn, clientData, &bufferListWasEmpty);
   if (retVal != ASOCKERR_SUCCESS) {
      goto outUndoAppend;
   }

   ASSERT(asock->vt->send);
   retVal = asock->vt->send(asock, bufferListWasEmpty, buf, len);
   if (retVal != ASOCKERR_SUCCESS) {
      goto outUndoAppend;
   }

   retVal = ASOCKERR_SUCCESS;
   goto outHaveLock;

outUndoAppend:
   if (asock->sendBufList != listBeforeAppend) {
      SendBufList *appendedBuffer = asock->sendBufList;
      asock->sendBufList = listBeforeAppend;
      if (asock->sendBufList == NULL) {
         asock->sendBufTail = &(asock->sendBufList);
      }
      free(appendedBuffer);
   }

outHaveLock:
   AsyncSocketUnlock(asock);
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketResolveAddr --
 *
 *      Resolves a hostname and port.
 *
 * Results:
 *      Zero upon success.  This returns whatever getaddrinfo() returns.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
int
AsyncSocketResolveAddr(const char *hostname,
                       unsigned short port,
                       int type,
                       struct sockaddr_in *addr)
{
   struct addrinfo hints;
   struct addrinfo *aiTop = NULL;
   struct addrinfo *aiIterator = NULL;
   int getaddrinfoError = 0;
   char portString[6]; /* strlen("65535\0") == 6 */

   ASSERT(NULL != addr);
   Str_Sprintf(portString, sizeof(portString), "%d", port);
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = type;

   /*
    * We use getaddrinfo() since it is thread-safe and IPv6 ready.
    * gethostbyname() is not thread-safe, and gethostbyname_r() is not
    * defined on Windows.
    */

   getaddrinfoError = Posix_GetAddrInfo(hostname, portString, &hints, &aiTop);
   if (0 != getaddrinfoError) {
      Log(ASOCKPREFIX "getaddrinfo failed for host %s: %s\n", hostname,
                      gai_strerror(getaddrinfoError));
      goto bye;
   }
   for (aiIterator = aiTop; NULL != aiIterator ; aiIterator =
                                                       aiIterator->ai_next) {
      if (aiIterator->ai_family != AF_INET) {
         continue;
      }
      *addr = *((struct sockaddr_in *) (aiIterator->ai_addr));
      break;
   }

bye:
   if (NULL != aiTop) {
      Posix_FreeAddrInfo(aiTop);
   }

   return getaddrinfoError;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCheckAndDispatchRecv --
 *
 *      Check if the recv buffer is full and dispatch the client callback.
 *
 *      Handles the possibility that the client registers a new receive buffer
 *      or closes the socket in their callback.
 *
 * Results:
 *      TRUE if the socket was closed or the receive was cancelled,
 *      FALSE if the caller should continue to try to receive data.
 *
 * Side effects:
 *      Could fire recv completion or trigger socket destruction.
 *
 *----------------------------------------------------------------------------
 */

Bool
AsyncSocketCheckAndDispatchRecv(AsyncSocket *s,  // IN
                                int *result)     // OUT
{
   ASSERT(s);
   ASSERT(result);
   ASSERT(s->recvFn);
   ASSERT(s->recvBuf);
   ASSERT(s->recvLen > 0);
   ASSERT(s->recvPos <= s->recvLen);

   if (s->recvPos == s->recvLen || s->recvFireOnPartial) {
      void *recvBuf = s->recvBuf;
      ASOCKLOG(3, s, ("recv buffer full, calling recvFn\n"));

      /*
       * We do this dance in case the handler frees the buffer (so
       * that there's no possible window where there are dangling
       * references here.  Obviously if the handler frees the buffer,
       * but them fails to register a new one, we'll put back the
       * dangling reference in the automatic reset case below, but
       * there's currently a limit to how far we go to shield clients
       * who use our API in a broken way.
       */

      s->recvBuf = NULL;
      s->recvFn(recvBuf, s->recvPos, s, s->clientData);
      if (s->state == AsyncSocketClosed) {
         ASOCKLG0(s, ("owner closed connection in recv callback\n"));
         *result = ASOCKERR_CLOSED;
         return TRUE;
      } else if (s->recvFn == NULL && s->recvLen == 0) {
         /*
          * Further recv is cancelled from within the last recvFn, see
          * AsyncSocket_CancelRecv(). So exit from the loop.
          */
         *result = ASOCKERR_SUCCESS;
         return TRUE;
      } else if (s->recvLen - s->recvPos == 0) {
         /* Automatically reset keeping the current handler */
         s->recvPos = 0;
         s->recvBuf = recvBuf;
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketFillRecvBuffer --
 *
 *      Called when an asock has data ready to be read via the poll callback.
 *
 * Results:
 *      ASOCKERR_SUCCESS if everything worked,
 *      ASOCKERR_REMOTE_DISCONNECT if peer closed connection gracefully,
 *      ASOCKERR_CLOSED if trying to read from a closed socket.
 *      ASOCKERR_GENERIC for other errors.
 *
 * Side effects:
 *      Reads data, could fire recv completion or trigger socket destruction.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketFillRecvBuffer(AsyncSocket *s)
{
   int recvd;
   int needed;
   int sysErr = 0;
   int result;
   int pending = 0;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(s));
   ASSERT(s->state == AsyncSocketConnected);

   /*
    * When a socket has received all its desired content and FillRecvBuffer is
    * called again for the same socket, just return ASOCKERR_SUCCESS. The
    * reason we need this hack is that if a client which registered a receive
    * callback asynchronously later changes its mind to do it synchronously,
    * (e.g. aioMgr wait function), then FillRecvBuffer can be potentially be
    * called twice for the same receive event.
    */

   needed = s->recvLen - s->recvPos;
   if (!s->recvBuf && needed == 0) {
      return ASOCKERR_SUCCESS;
   }

   ASSERT(needed > 0);

   AsyncSocketAddRef(s);

   /*
    * See comment in AsyncSocket_Recv
    */

   s->inRecvLoop = TRUE;

   do {

      /*
       * Try to read the remaining bytes to complete the current recv request.
       */

      if (s->passFd.expected) {
         int fd;

         recvd = SSL_RecvDataAndFd(s->sslSock,
                                   (uint8 *) s->recvBuf + s->recvPos,
                                   needed, &fd);
         if (fd != -1) {
            s->passFd.fd = fd;
            s->passFd.expected = FALSE;
         }
      } else {
         recvd = SSL_Read(s->sslSock, (uint8 *) s->recvBuf + s->recvPos,
                          needed);
      }
      ASOCKLOG(3, s, ("need\t%d\trecv\t%d\tremain\t%d\n", needed, recvd,
                      needed - recvd));

      if (recvd > 0) {
         s->sslConnected = TRUE;
         s->recvPos += recvd;
         if (AsyncSocketCheckAndDispatchRecv(s, &result)) {
            goto exit;
         }
      } else if (recvd == 0) {
         ASOCKLG0(s, ("recv detected client closed connection\n"));
         /*
          * We treat this as an error so that the owner can detect closing
          * of connection by peer (via the error handler callback).
          */
         result = ASOCKERR_REMOTE_DISCONNECT;
         goto exit;
      } else if ((sysErr = ASOCK_LASTERROR()) == ASOCK_EWOULDBLOCK) {
         ASOCKLOG(4, s, ("recv would block\n"));
         break;
      } else {
         ASOCKLG0(s, ("recv error %d: %s\n", sysErr,
                      Err_Errno2String(sysErr)));
         s->genericErrno = sysErr;
         result = ASOCKERR_GENERIC;
         goto exit;
      }

      /*
       * At this point, s->recvFoo have been updated to point to the
       * next chained Recv buffer. By default we're done at this
       * point, but we may want to continue if the SSL socket has data
       * buffered in userspace already (SSL_Pending).
       */

      needed = s->recvLen - s->recvPos;
      ASSERT(needed > 0);

      pending = SSL_Pending(s->sslSock);
      needed = MIN(needed, pending);

   } while (needed);

   /*
    * Reach this point only when previous SSL_Pending returns 0 or
    * error is ASOCK_EWOULDBLOCK
    */

   ASSERT(pending == 0 || sysErr == ASOCK_EWOULDBLOCK);

   /*
    * Both a spurious wakeup and receiving any data even if it wasn't enough
    * to fire the callback are both success.  We were ready and now
    * presumably we aren't ready anymore.
    */

   result = ASOCKERR_SUCCESS;

exit:
   s->inRecvLoop = FALSE;
   AsyncSocketRelease(s, FALSE);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketDispatchSentBuffer --
 *
 *      Pop off the head of the send buffer list and call its callback.
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
AsyncSocketDispatchSentBuffer(AsyncSocket *s)
{
   /*
    * We're done with the current buffer, so pop it off and nuke it.
    * We do the list management *first*, so that the list is in a
    * consistent state.
    */

   SendBufList *head = s->sendBufList;
   SendBufList tmp = *head;

   s->sendBufList = head->next;
   if (s->sendBufList == NULL) {
      s->sendBufTail = &(s->sendBufList);
   }
   s->sendPos = 0;
   free(tmp.base64Buf);
   free(head);

   if (tmp.sendFn) {
      /*
       * XXX
       * Firing the send completion could trigger the socket's
       * destruction (since the callback could turn around and call
       * AsyncSocket_Close()). Since we're in the middle of a loop on
       * the asock's queue, we avoid a use-after-free by deferring
       * the actual freeing of the asock structure. This is shady but
       * it works. --rrdharan
       */

      tmp.sendFn(tmp.buf, tmp.len, s, tmp.clientData);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketWriteBuffers --
 *
 *      The meat of AsyncSocket's sending functionality.  This function
 *      actually writes to the wire assuming there's space in the buffers
 *      for the socket.
 *
 * Results:
 *      ASOCKERR_SUCESS if everything worked, else ASOCKERR_GENERIC.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncSocketWriteBuffers(AsyncSocket *s)
{
   int result;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(s));

   if (s->sendBufList == NULL) {
      return ASOCKERR_SUCCESS;     /* Vacuously true */
   }

   if (s->state != AsyncSocketConnected) {
      ASOCKWARN(s, ("write buffers on a disconnected socket (%d)!\n",
                    s->state));
      return ASOCKERR_GENERIC;
   }

   AsyncSocketAddRef(s);

   while (s->sendBufList && s->state == AsyncSocketConnected) {
      SendBufList *head = s->sendBufList;
      int error = 0;
      int sent = 0;
      int left = head->len - s->sendPos;
      int sizeToSend = head->len;

      if (head->base64Buf) {
         sent = SSL_Write(s->sslSock,
                          (uint8 *) head->base64Buf + s->sendPos, left);
      } else {
         sent = SSL_Write(s->sslSock,
                          (uint8 *) head->buf + s->sendPos, left);
      }
      ASOCKLOG(3, s, ("left\t%d\tsent\t%d\tremain\t%d\n",
                      left, sent, left - sent));
      if (sent > 0) {
         s->sendBufFull = FALSE;
         s->sslConnected = TRUE;
         if ((s->sendPos += sent) == sizeToSend) {
            AsyncSocketDispatchSentBuffer(s);
         }
      } else if (sent == 0) {
         ASOCKLG0(s, ("socket write() should never return 0.\n"));
         NOT_REACHED();
      } else if ((error = ASOCK_LASTERROR()) != ASOCK_EWOULDBLOCK) {
         ASOCKLG0(s, ("send error %d: %s\n", error, Err_Errno2String(error)));
         s->genericErrno = error;
         result = ASOCKERR_GENERIC;
         goto exit;
      } else {
         /*
          * Ran out of space to send. This is actually successful completion
          * (our contract obligates us to send as much data as space allows
          * and we fulfilled that).
          *
          * Indicate send buffer is full.
          */

         s->sendBufFull = TRUE;
         break;
      }
   }

   result = ASOCKERR_SUCCESS;

exit:
   AsyncSocketRelease(s, FALSE);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketAcceptInternal --
 *
 *      The meat of 'accept'.  This function can be invoked either via a
 *      poll callback or blocking. We call accept to get the new socket fd,
 *      create a new asock, and call the newFn callback previously supplied
 *      by the call to AsyncSocket_Listen.
 *
 * Results:
 *      ASOCKERR_SUCCESS if everything works, else an error code.
 *      ASOCKERR_GENERIC is returned to hide accept() system call's
 *        nitty-gritty, it implies that we should try accept() again and not
 *        report error to client.
 *      ASOCKERR_ACCEPT to report accept operation's error to client.
 *
 * Side effects:
 *      Accepts on listening fd, creates new asock.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncSocketAcceptInternal(AsyncSocket *s)
{
   AsyncSocket *newsock;
   int sysErr;
   int fd;
   struct sockaddr remoteAddr;
   socklen_t remoteAddrLen = sizeof remoteAddr;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(s));
   ASSERT(s->state == AsyncSocketListening);

   if ((fd = accept(s->fd, &remoteAddr, &remoteAddrLen)) == -1) {
      sysErr = ASOCK_LASTERROR();
      s->genericErrno = sysErr;
      if (sysErr == ASOCK_EWOULDBLOCK) {
         ASOCKWARN(s, ("spurious accept notification\n"));

         return ASOCKERR_GENERIC;
#ifndef _WIN32
         /*
          * This sucks. Linux accept() can return ECONNABORTED for connections
          * that closed before we got to actually call accept(), but Windows
          * just ignores this case. So we have to special case for Linux here.
          * We return ASOCKERR_GENERIC here because we still want to continue
          * accepting new connections.
          */

      } else if (sysErr == ECONNABORTED) {
         ASOCKLG0(s, ("accept: new connection was aborted\n"));

         return ASOCKERR_GENERIC;
#endif
      } else {
         ASOCKWARN(s, ("accept failed on fd %d, error %d: %s\n",
                       s->fd, sysErr, Err_Errno2String(sysErr)));

         return ASOCKERR_ACCEPT;
      }
   }

   newsock = AsyncSocket_AttachToFd(fd, &s->pollParams, NULL);
   if (!newsock) {
      SSLGeneric_close(fd);

      return ASOCKERR_ACCEPT;
   }

   newsock->remoteAddr = remoteAddr;
   newsock->remoteAddrLen = remoteAddrLen;
   newsock->state = AsyncSocketConnected;
   newsock->vt = s->vt;

   ASSERT(s->vt);
   ASSERT(s->vt->dispatchConnect);
   s->vt->dispatchConnect(s, newsock);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketConnectInternal --
 *
 *      The meat of connect.  This function is invoked either via a poll
 *      callback or the blocking API and verifies that connect() succeeded
 *      or reports is failure.  On success we call the registered 'new
 *      connection' function.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it all worked out or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Creates new asock, fires newFn callback.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncSocketConnectInternal(AsyncSocket *s)
{
   int optval = 0, optlen = sizeof optval, sysErr;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(s));
   ASSERT(s->state == AsyncSocketConnecting);

   /* Remove when bug 859728 is fixed */
   if (vmx86_server && s->remoteAddr.sa_family == AF_UNIX) {
      goto done;
   }

   if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR,
                  (void *) &optval, (void *)&optlen) != 0) {
      sysErr = ASOCK_LASTERROR();
      s->genericErrno = sysErr;
      Warning(ASOCKPREFIX "getsockopt for connect on fd %d failed with "
              "error %d : %s\n", s->fd, sysErr, Err_Errno2String(sysErr));

      return ASOCKERR_GENERIC;
   }

   if (optval != 0) {
      s->genericErrno = optval;
      Warning(ASOCKPREFIX "SO_ERROR for connect on fd %d: %s\n",
              s->fd, Err_Errno2String(optval));

      return ASOCKERR_GENERIC;
   }

   s->localAddrLen = sizeof s->localAddr;
   if (getsockname(s->fd, &s->localAddr, &s->localAddrLen) != 0) {
      sysErr = ASOCK_LASTERROR();
      s->genericErrno = sysErr;
      Warning(ASOCKPREFIX "getsockname for connect on fd %d failed with "
              "error %d: %s\n", s->fd, sysErr, Err_Errno2String(sysErr));

      return ASOCKERR_GENERIC;
   }

done:
   s->state = AsyncSocketConnected;
   s->connectFn(s, s->clientData);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetGenericErrno --
 *
 *      Used when an ASOCKERR_GENERIC is returned due to a system error.
 *      The errno that was returned by the system is stored in the asock
 *      struct and returned to the user in this function.
 *
 *      XXX: This function is not thread-safe.  The errno should be returned
 *      in a parameter to any function that can return ASOCKERR_GENERIC.
 *
 * Results:
 *      int error code
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetGenericErrno(AsyncSocket *s)  // IN:
{
   ASSERT(s);

   return s->genericErrno;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_WaitForConnection --
 *
 *      Spins a socket currently listening or connecting until the
 *      connection completes or the allowed time elapses.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on failures, and
 *      ASOCKERR_TIMEOUT if nothing happened in the allotted time.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_WaitForConnection(AsyncSocket *s,  // IN:
                              int timeoutMS)   // IN:
{
   Bool read;
   int error;
   VmTimeType now, done;
   Bool removed = FALSE;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   AsyncSocketLock(s);

   if (s->state == AsyncSocketConnected) {
      error = ASOCKERR_SUCCESS;
      goto outHaveLock;
   }

   if (s->state != AsyncSocketListening &&
       s->state != AsyncSocketConnecting) {
      error = ASOCKERR_GENERIC;
      goto outHaveLock;
   }

   /*
   * A nuisance.  ConnectCallback() is either registered as a device or
   * rtime callback depending on the prior return value of connect().
   * So we try to remove it from both.
   *
   * XXX: For listening sockets, the callback is AsyncSocketAcceptCallback,
   * which would need to be unregistered here and then be re-registered
   * before returning.
   */
   if (s->state == AsyncSocketConnecting) {
      removed = AsyncSocketPollRemove(s, TRUE, POLL_FLAG_WRITE,
                                      AsyncSocketConnectCallback)
         || AsyncSocketPollRemove(s, FALSE, 0, AsyncSocketConnectCallback);
      ASSERT(removed);
   }

   AsyncSocketUnlock(s);

   read = s->state == AsyncSocketListening;

   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;

   do {
      if ((error = AsyncSocketPoll(s, read,
                                   done - now)) != ASOCKERR_SUCCESS) {
         goto out;
      }

      AsyncSocketLock(s);

      now = Hostinfo_SystemTimerUS() / 1000;

      if (read) {
         if (AsyncSocketAcceptInternal(s) != ASOCKERR_SUCCESS) {
            ASOCKLG0(s, ("wait for connection: accept failed\n"));

            /*
             * Just fall through, we'll loop and try again as long as we still
             * have time remaining.
             */

         } else {
            error = ASOCKERR_SUCCESS;
            goto outHaveLock;
         }
      } else {
         error = AsyncSocketConnectInternal(s);
         goto outHaveLock;
      }

      AsyncSocketUnlock(s);

   } while ((now < done && timeoutMS > 0) || (timeoutMS < 0));

   error = ASOCKERR_TIMEOUT;
   goto out;

outHaveLock:
   AsyncSocketUnlock(s);

out:
   return error;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_DoOneMsg --
 *
 *      Spins a socket until the specified amount of time has elapsed or
 *      data has arrived / been sent.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on system call
 *         failures
 *      ASOCKERR_TIMEOUT if nothing happened in the allotted time.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_DoOneMsg(AsyncSocket *s, Bool read, int timeoutMS)
{
   int retVal;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (!s) {
      Warning(ASOCKPREFIX "DoOneMsg called with invalid paramters.\n");
      return ASOCKERR_INVAL;
   }

   if (read) {
      VMwareStatus pollStatus;

      /*
       * Bug 158571: There could other threads polling on the same asyncsocket
       * If two threads land up polling  on the same socket at the same time,
       * the first thread to be scheduled reads the data from the socket,
       * while the second one blocks infinitely. This hangs the VM. To prevent
       * this, we temporarily remove the poll callback and then reinstate it
       * after reading the data.
       */

      Bool removed;

      AsyncSocketLock(s);
      ASSERT(s->state == AsyncSocketConnected);
      ASSERT(s->recvCb); /* We are supposed to call someone... */
      AsyncSocketAddRef(s);
      removed = AsyncSocketPollRemove(s, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      s->vt->recvCallback);
      ASSERT(removed || s->pollParams.iPoll);

      s->inBlockingRecv++;
      AsyncSocketUnlock(s); /* We may sleep in poll. */
      retVal = AsyncSocketPoll(s, read, timeoutMS);
      AsyncSocketLock(s);
      s->inBlockingRecv--;
      if (retVal != ASOCKERR_SUCCESS) {
         if (retVal == ASOCKERR_GENERIC) {
            ASOCKWARN(s, ("%s: failed to poll on the socket during read.\n",
                          __FUNCTION__));
         }
      } else {
         retVal = AsyncSocketFillRecvBuffer(s);
      }

      /*
       * If socket got closed in AsyncSocketFillRecvBuffer, we cannot add poll
       * callback - AsyncSocket_Close() would remove it if we would not remove
       * it above.
       */

      if (s->state != AsyncSocketClosed) {
         ASSERT(s->refCount > 1); /* We should not be last user of socket */
         ASSERT(s->state == AsyncSocketConnected);
         ASSERT(s->recvCb); /* Still interested in callback. */
         pollStatus = AsyncSocketPollAdd(s, TRUE,
                                         POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                         s->vt->recvCallback);

         if (pollStatus != VMWARE_STATUS_SUCCESS) {
            ASOCKWARN(s, ("failed to install recv callback!\n"));
            AsyncSocketRelease(s, TRUE);

            retVal = ASOCKERR_POLL;
            goto out;
         }
      }
      /* This may destroy socket s if it is in AsyncSocketClosed state now. */
      AsyncSocketRelease(s, TRUE);
   } else {
      if ((retVal = AsyncSocketPoll(s, read, timeoutMS)) != ASOCKERR_SUCCESS) {
         if (retVal == ASOCKERR_GENERIC) {
            ASOCKWARN(s, ("%s: failed to poll on the socket during write.\n",
                          __FUNCTION__));
         }
      } else {
         AsyncSocketLock(s);
         retVal = AsyncSocketWriteBuffers(s);
         AsyncSocketUnlock(s);
      }
   }

out:
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Flush --
 *
 *      Try to send any pending out buffers until we run out of buffers, or
 *      the timeout expires.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on system call
 *      failures, and ASOCKERR_TIMEOUT if we couldn't send enough data
 *      before the timeout expired.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Flush(AsyncSocket *s, int timeoutMS)
{
   VmTimeType now, done;
   int retVal;

   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (s == NULL) {
      Warning(ASOCKPREFIX "Flush called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(s);
   AsyncSocketAddRef(s);

   if (s->state != AsyncSocketConnected) {
      ASOCKWARN(s, ("flush called but state is not connected!\n"));
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;

   while (s->sendBufList) {
      AsyncSocketUnlock(s); /* We may sleep in poll. */
      retVal = AsyncSocketPoll(s, FALSE, done - now);
      AsyncSocketLock(s);

      if (retVal != ASOCKERR_SUCCESS) {
         ASOCKWARN(s, ("flush failed\n"));
         goto outHaveLock;
      }

      if ((retVal = AsyncSocketWriteBuffers(s)) != ASOCKERR_SUCCESS) {
         goto outHaveLock;
      }
      ASSERT(s->state == AsyncSocketConnected);

      /* Setting timeoutMS to -1 means never timeout. */
      if (timeoutMS >= 0) {
         now = Hostinfo_SystemTimerUS() / 1000;

         /* Don't timeout if you've sent everything */
         if (now > done && s->sendBufList) {
            ASOCKWARN(s, ("flush timed out\n"));
            retVal = ASOCKERR_TIMEOUT;
            goto outHaveLock;
         }
      }
   }

   retVal = ASOCKERR_SUCCESS;

outHaveLock:
   AsyncSocketRelease(s, TRUE);

   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetErrorFn --
 *
 *      Sets the error handling function for the asock. The error function
 *      is invoked automatically on I/O errors. Passing NULL as the error
 *      function restores the default behavior, which is to just destroy the
 *      AsyncSocket on any errors.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_INVAL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetErrorFn(AsyncSocket *asock,           // IN/OUT
                       AsyncSocketErrorFn errorFn,   // IN
                       void *clientData)             // IN
{
   if (!asock) {
      Warning(ASOCKPREFIX "%s called with invalid arguments!\n",
              __FUNCTION__);

      return ASOCKERR_INVAL;
   }
   AsyncSocketLock(asock);
   asock->errorFn = errorFn;
   asock->errorClientData = clientData;
   AsyncSocketUnlock(asock);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCancelListenCbSocket --
 *
 *      Socket specific code for canceling callbacks for a listening socket.
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
AsyncSocketCancelListenCbSocket(AsyncSocket *asock)  // IN:
{
   Bool removed;

   ASSERT(AsyncSocketIsLocked(asock));

   removed = AsyncSocketPollRemove(asock, TRUE,
                                   POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                   AsyncSocketAcceptCallback);
   ASSERT(removed);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCancelRecvCbSocket --
 *
 *      Socket specific code for canceling callbacks when a receive
 *      request is being canceled.
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
AsyncSocketCancelRecvCbSocket(AsyncSocket *asock)  // IN:
{
   ASSERT(AsyncSocketIsLocked(asock));

   if (asock->recvCb) {
      Bool removed;
      ASOCKLOG(1, asock, ("Removing poll recv callback while cancelling recv.\n"));
      removed = AsyncSocketPollRemove(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      asock->vt->recvCallback);
      ASSERT_NOT_IMPLEMENTED(removed || asock->pollParams.iPoll);
      asock->recvCb = FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCancelCbForCloseSocket --
 *
 *      Socket specific code for canceling callbacks when a socket is
 *      being closed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters send/recv Poll callbacks, and fires the send
 *      triggers for any remaining output buffers. May also change
 *      the socket state.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketCancelCbForCloseSocket(AsyncSocket *asock)  // IN:
{
   Bool removed;

   /*
    * Remove the read and write poll callbacks.
    *
    * We could fire the current recv completion callback here, but in
    * practice clients won't want to know about partial reads since it just
    * complicates the common case (i.e. every read callback would need to
    * check the len parameter).
    *
    * For writes, however, we *do* fire all of the callbacks. The argument
    * here is that the common case for writes is "fire and forget", e.g.
    * send this buffer and free it. Firing the triggers at close time
    * simplifies client code, since the clients aren't forced to keep track
    * of send buffers themselves. Clients can figure out how much data was
    * actually transmitted (if they care) by checking the len parameter
    * passed to the send callback.
    *
    * A modification suggested by Jeremy is to pass a list of unsent
    * buffers and their completion callbacks to the error handler if one is
    * registered, and only fire the callbacks here if there was no error
    * handler invoked.
    */

   ASSERT(!asock->recvBuf || asock->recvCb);

   if (asock->recvCb) {
      ASOCKLOG(1, asock, ("recvCb is non-NULL, removing recv callback\n"));
      removed = AsyncSocketPollRemove(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      asock->vt->recvCallback);

      /*
       * Callback might be temporarily removed in AsyncSocket_DoOneMsg.
       */

      ASSERT_NOT_TESTED(removed || asock->pollParams.iPoll);

      /*
       * We may still have the RTime callback, try to remove if it exists
       */

      removed = Poll_CB_RTimeRemove(asock->vt->recvCallback,
                                    asock, FALSE);
      asock->recvCb = FALSE;
      asock->recvBuf = NULL;
   }

   if (asock->sendCb) {
      ASOCKLOG(1, asock, ("sendBufList is non-NULL, removing send callback\n"));

      /*
       * The send callback could be either a device or RTime callback, so
       * we check the latter if it wasn't the former.
       */

      if (asock->sendCbTimer) {
         removed = AsyncSocketPollRemove(asock, FALSE, 0,
                                         asock->vt->sendCallback);
      } else {
         removed = AsyncSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                         asock->vt->sendCallback);
      }
      ASSERT(removed || asock->pollParams.iPoll);
      asock->sendCb = FALSE;
      asock->sendCbTimer = FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCancelCbForCloseInt --
 *
 *      Cancel future asynchronous send and recv by unregistering
 *      their Poll callbacks, and change the socket state to
 *      AsyncSocketCBCancelled if the socket state is AsyncSocketConnected.
 *
 *      The function can be called in a send/recv error handler before
 *      actually closing the socket in a separate thread, to prevent other
 *      code calling AsyncSocket_Send/Recv from re-registering the
 *      callbacks again. The next operation should be just AsyncSocket_Close().
 *      This helps to avoid unnecessary send/recv callbacks before the
 *      socket is closed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters send/recv Poll callbacks, and fires the send
 *      triggers for any remaining output buffers. May also change
 *      the socket state.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketCancelCbForCloseInt(AsyncSocket *asock)  // IN:
{
   ASSERT(AsyncSocketIsLocked(asock));

   if (asock->state == AsyncSocketConnected) {
      asock->state = AsyncSocketCBCancelled;
   }

   ASSERT(asock->vt);
   ASSERT(asock->vt->cancelCbForClose);
   asock->vt->cancelCbForClose(asock);

   AsyncSocketAddRef(asock);
   while (asock->sendBufList) {
      /*
       * Pop each remaining buffer and fire its completion callback.
       */

      SendBufList *cur = asock->sendBufList;
      int pos = asock->sendPos;

      /*
       * Free the Base64 encoded data if it exists.
       */
      free(cur->base64Buf);
      asock->sendBufList = asock->sendBufList->next;
      asock->sendPos = 0;

      if (cur->sendFn) {
         cur->sendFn(cur->buf, pos, asock, cur->clientData);
      }
      free(cur);
   }
   AsyncSocketRelease(asock, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_CancelCbForClose --
 *
 *      This is the external version of AsyncSocketCancelCbForCloseInt().  It
 *      takes care of acquiring any necessary lock before calling the internal
 *      function.
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
AsyncSocket_CancelCbForClose(AsyncSocket *asock)  // IN:
{
   AsyncSocketLock(asock);
   AsyncSocketCancelCbForCloseInt(asock);
   AsyncSocketUnlock(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCloseSocket --
 *
 *      AsyncSocket destructor for SSL sockets.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Closes the socket fd.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketCloseSocket(AsyncSocket *asock) // IN
{
   SSL_Shutdown(asock->sslSock);

   if (asock->passFd.fd != -1) {
      SSLGeneric_close(asock->passFd.fd);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketCancelCbForConnectingCloseSocket --
 *
 *      Cancels outstanding connect requests for a socket that is going
 *      away.
 *
 * Results:
 *      TRUE on callback removed. FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
AsyncSocketCancelCbForConnectingCloseSocket(AsyncSocket *asock) // IN
{
   return AsyncSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                AsyncSocketConnectCallback)
      || AsyncSocketPollRemove(asock, FALSE, 0, AsyncSocketConnectCallback);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Close --
 *
 *      AsyncSocket destructor. The destructor should be safe to call at any
 *      time.  It's invoked automatically for I/O errors on slots that have no
 *      error handler set, and should be called manually by the error handler
 *      as necessary. It could also be called as part of the normal program
 *      flow.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Closes the socket fd, unregisters all Poll callbacks, and fires the
 *      send triggers for any remaining output buffers.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Close(AsyncSocket *asock)
{
   Bool removed;
   AsyncSocketState oldState;

   if (!asock) {
      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(asock);

   if (asock->state == AsyncSocketClosed) {
      Warning("%s() called on already closed asock!\n", __FUNCTION__);
      AsyncSocketUnlock(asock);

      return ASOCKERR_CLOSED;
   }

   /*
    * Set the new state to closed, and then check the old state and do the
    * right thing accordingly
    */

   ASOCKLOG(1, asock, ("closing socket\n"));
   oldState = asock->state;
   asock->state = AsyncSocketClosed;

   ASSERT(asock->vt);

   switch(oldState) {
   case AsyncSocketListening:
      ASOCKLOG(1, asock, ("old state was listening, removing accept callback\n"));
      ASSERT(asock->vt->cancelListenCb);
      asock->vt->cancelListenCb(asock);
      break;

   case AsyncSocketConnecting:
      ASOCKLOG(1, asock, ("old state was connecting, removing connect callback\n"));
      ASSERT(asock->vt->cancelCbForConnectingClose);
      removed = asock->vt->cancelCbForConnectingClose(asock);
      if (!removed) {
         ASOCKLOG(1, asock, ("connect callback is not present in the poll list.\n"));
      }
      break;

   case AsyncSocketConnected:
      ASOCKLOG(1, asock, ("old state was connected\n"));
      AsyncSocketCancelCbForCloseInt(asock);
      break;

   case AsyncSocketCBCancelled:
      ASOCKLOG(1, asock, ("old state was CB-cancelled\n"));
      break;

   default:
      NOT_REACHED();
   }

   ASSERT(asock->vt->close);
   asock->vt->close(asock);

   AsyncSocketRelease(asock, TRUE);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetState --
 *
 *      Returns the state of the provided asock or ASOCKERR_INVAL.  Note that
 *      unless this is called from a callback function, the state should be
 *      treated as transient (except the state AsyncSocketClosed).
 *
 * Results:
 *      AsyncSocketState enum.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocketState
AsyncSocket_GetState(AsyncSocket *asock)
{
   return (asock ? asock->state : ASOCKERR_INVAL);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_IsSendBufferFull --
 *
 *      Indicate if socket send buffer is full.  Note that unless this is
 *      called from a callback function, the return value should be treated
 *      as transient.
 *
 * Results:
 *      0: send space probably available,
 *      1: send has reached maximum,
 *      ASOCKERR_GENERIC: null socket.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_IsSendBufferFull(AsyncSocket *asock)
{
   return (asock ? asock->sendBufFull : ASOCKERR_GENERIC);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetID --
 *
 *      Returns a unique identifier for the asock.
 *
 * Results:
 *      Integer id or ASOCKERR_INVAL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetID(AsyncSocket *asock)
{
   return (asock ? asock->id : ASOCKERR_INVAL);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSendInternal --
 *
 *      Internal send method for 'regular' socket connections, allocates & prepares
 *      a buffer and enqueues it.
 *
 * Results:
 *      ASOCKERR_SUCCESS if there are no errors.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketSendInternal(AsyncSocket *asock,         // IN
                        void *buf,                  // IN
                        int len,                    // IN
                        AsyncSocketSendFn sendFn,   // IN
                        void *clientData,           // IN
                        Bool *bufferListWasEmpty)   // IN
{
   SendBufList *newBuf;
   ASSERT(bufferListWasEmpty);

   /*
    * Allocate and initialize new send buffer entry
    */

   newBuf = Util_SafeCalloc(1, sizeof(SendBufList));
   newBuf->buf = buf;
   newBuf->len = len;
   newBuf->sendFn = sendFn;
   newBuf->clientData = clientData;

   /*
    * Append new send buffer to the tail of list.
    */

   *asock->sendBufTail = newBuf;
   asock->sendBufTail = &(newBuf->next);
   if (asock->sendBufList == newBuf) {
      *bufferListWasEmpty = TRUE;
   }

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketDispatchConnect --
 *
 *      Simple dispatch to call the connect callback for the socket pair.
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
AsyncSocketDispatchConnect(AsyncSocket *asock,
                           AsyncSocket *newsock)
{
   asock->connectFn(newsock, asock->clientData);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketHasDataPendingSocket --
 *
 *      Determine if the SSL socket has any pending/unread data.
 *
 * Results:
 *      TRUE if this socket has pending data.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
AsyncSocketHasDataPendingSocket(AsyncSocket *asock) // IN
{
   return SSL_Pending(asock->sslSock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketHasDataPending --
 *
 *      Determine if the SSL or WebSocket has any pending/unread data.
 *
 * Results:
 *      TRUE if this socket has pending data.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
AsyncSocketHasDataPending(AsyncSocket *asock)   // IN:
{
   ASSERT(asock->vt);
   ASSERT(asock->vt->hasDataPending);

   return asock->vt->hasDataPending(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketMakeNonBlocking --
 *
 *      Make the specified socket non-blocking if it isn't already.
 *
 * Results:
 *      ASOCKERR_SUCCESS if the operation succeeded, ASOCKERR_GENERIC otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncSocketMakeNonBlocking(int fd)
{
#ifdef _WIN32
   int retval;
   u_long argp = 1; /* non-zero => enable non-blocking mode */

   retval = ioctlsocket(fd, FIONBIO, &argp);

   if (retval != 0) {
      ASSERT(retval == SOCKET_ERROR);

      return ASOCKERR_GENERIC;
   }
#elif defined(__APPLE__)
   int argp = 1;
   if (ioctl(fd, FIONBIO, &argp) < 0) {
      return ASOCKERR_GENERIC;
   }
#else
   int flags;

   if ((flags = fcntl(fd, F_GETFL)) < 0) {
      return ASOCKERR_GENERIC;
   }

   if (!(flags & O_NONBLOCK) && (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0))
   {
      return ASOCKERR_GENERIC;
   }
#endif

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketHandleError --
 *
 *      Internal error handling helper. Changes the socket's state to error,
 *      and calls the registered error handler or closes the socket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Lots.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketHandleError(AsyncSocket *asock, int asockErr)
{
   ASSERT(asock);
   if (asock->errorFn) {
      ASOCKLOG(3, asock, ("firing error callback\n"));
      asock->errorFn(asockErr, asock, asock->errorClientData);
   } else {
      ASOCKLOG(3, asock, ("no error callback, closing socket\n"));
      AsyncSocket_Close(asock);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketAcceptCallback --
 *
 *      Poll callback for listening fd waiting to complete an accept
 *      operation. We call accept to get the new socket fd, create a new
 *      asock, and call the newFn callback previously supplied by the call to
 *      AsyncSocket_Listen.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Accepts on listening fd, creates new asock.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketAcceptCallback(void *clientData)
{
   AsyncSocket *asock = (AsyncSocket *) clientData;
   int retval;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(asock->pollParams.iPoll == NULL);
   ASSERT(AsyncSocketIsLocked(asock));

   AsyncSocketAddRef(asock);
   retval = AsyncSocketAcceptInternal(asock);

   /*
    * See comment for return value of AsyncSocketAcceptInternal().
    */

   if (retval == ASOCKERR_ACCEPT) {
      AsyncSocketHandleError(asock, retval);
   }
   AsyncSocketRelease(asock, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketConnectCallback --
 *
 *      Poll callback for connecting fd. Calls through to
 *      AsyncSocketConnectInternal to do the real work.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Creates new asock, fires newFn callback.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketConnectCallback(void *clientData)
{
   AsyncSocket *asock = (AsyncSocket *) clientData;
   int retval;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(asock->pollParams.iPoll == NULL);
   ASSERT(AsyncSocketIsLocked(asock));

   AsyncSocketAddRef(asock);
   retval = AsyncSocketConnectInternal(asock);
   if (retval != ASOCKERR_SUCCESS) {
      ASSERT(retval == ASOCKERR_GENERIC); /* Only one we're expecting */
      AsyncSocketHandleError(asock, retval);
   }
   AsyncSocketRelease(asock, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketRecvCallback --
 *
 *      Poll callback for input waiting on the socket. We try to pull off the
 *      remaining data requested by the current receive function.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Reads data, could fire recv completion or trigger socket destruction.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketRecvCallback(void *clientData)
{
   AsyncSocket *asock = (AsyncSocket *) clientData;
   int error;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(asock));

   AsyncSocketAddRef(asock);

   error = AsyncSocketFillRecvBuffer(asock);
   if (error == ASOCKERR_GENERIC || error == ASOCKERR_REMOTE_DISCONNECT) {
      AsyncSocketHandleError(asock, error);
   }

   AsyncSocketRelease(asock, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketIPollRecvCallback --
 *
 *      Poll callback for input waiting on the socket.  IVmdbPoll does not
 *      handle callback locks, so this function first locks the asyncsocket
 *      and verify that the recv callback has not been cancelled before
 *      calling AsyncSocketFillRecvBuffer to do the real work.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Reads data, could fire recv completion or trigger socket destruction.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketIPollRecvCallback(void *clientData)  // IN:
{
#ifdef VMX86_TOOLS
   NOT_IMPLEMENTED();
#else
   AsyncSocket *asock = (AsyncSocket *) clientData;
   MXUserRecLock *lock;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(asock->pollParams.lock == NULL ||
          !MXUser_IsCurThreadHoldingRecLock(asock->pollParams.lock));

   AsyncSocketLock(asock);
   lock = asock->pollParams.lock;
   if (asock->recvCb) {
      /*
       * There is no need to take a reference here -- the fact that this
       * callback is running means AsyncsocketIPollRemove would not release a
       * reference if it is called.
       */
      int error = AsyncSocketFillRecvBuffer(asock);

      if (error == ASOCKERR_GENERIC || error == ASOCKERR_REMOTE_DISCONNECT) {
         AsyncSocketHandleError(asock, error);
      }
   }

   if (asock->recvCb) {
      AsyncSocketUnlock(asock);
   } else {
      /*
       * Callback has been unregistered.  Per above, we need to release the
       * reference explicitly.
       */
      AsyncSocketRelease(asock, TRUE);
      if (lock != NULL) {
         MXUser_DecRefRecLock(lock);
      }
   }
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketRecvUDPCallback --
 *
 *      Retrieve the UDP packet and fire a callback with it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Reads data, fires recv completion.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketRecvUDPCallback(void *clientData)
{
   AsyncSocket *asock = (AsyncSocket *) clientData;
   struct sockaddr_in clientAddr;
   int clientAddrLen = sizeof(clientAddr);
   int actualPacketLength = 0;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(asock));

   /*
    * TODO: It would be useful if we also get the destination address
    * and pass that to the callback.  This way, if the socket is
    * bound to multiple interfaces, we know which interface the
    * packet came in from.  getsockname() doesn't appear to work here.
    * Instead, recvmsg() with the IP_PKTINFO socket option enabled
    * appears to be the right thing to do.  (Use WSARecvMsg on Win32)
    */

   AsyncSocketAddRef(asock);
   actualPacketLength = recvfrom(asock->fd, asock->recvBuf,
                                 asock->recvLen, 0,
                                 (struct sockaddr *) &clientAddr,
                                 &clientAddrLen);

   if (-1 == actualPacketLength) {
      AsyncSocketHandleError(asock, ASOCKERR_GENERIC);
      goto exit;
   }

   asock->recvUDPFn(asock->recvBuf, actualPacketLength, asock,
                    asock->clientData, &clientAddr, clientAddrLen);

exit:
   AsyncSocketRelease(asock, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSendCallback --
 *
 *      Poll callback for output socket buffer space available (socket is
 *      writable). We iterate over all the remaining buffers in our queue,
 *      writing as much as we can until we fill the socket buffer again. If we
 *      don't finish, we register ourselves as a device write callback.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Writes data, could trigger write completion or socket destruction.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketSendCallback(void *clientData)
{
   AsyncSocket *s = (AsyncSocket *) clientData;
   int retval;

   ASSERT(s);
   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(AsyncSocketIsLocked(s));

   AsyncSocketAddRef(s);
   s->sendCb = FALSE; /* AsyncSocketSendCallback is never periodic */
   s->sendCbTimer = FALSE;
   retval = AsyncSocketWriteBuffers(s);
   if (retval != ASOCKERR_SUCCESS) {
      AsyncSocketHandleError(s, retval);
   } else if (s->sendBufList && !s->sendCb) {
      VMwareStatus pollStatus;

      /*
       * We didn't finish, so we need to reschedule the Poll callback (the
       * write callback is *not* periodic).
       */

#ifdef _WIN32
      /*
       * If any data has been sent out or read in from the sslSock,
       * SSL has finished the handshaking. Otherwise,
       * we have to schedule a realtime callback for write. See bug 37147
       */

      if (!s->sslConnected) {
         pollStatus = AsyncSocketPollAdd(s, FALSE, 0,
                                         s->vt->sendCallback, 100000);
         ASSERT_NOT_IMPLEMENTED(pollStatus == VMWARE_STATUS_SUCCESS);
         s->sendCbTimer = TRUE;
      } else
#endif
      {
         pollStatus = AsyncSocketPollAdd(s, TRUE, POLL_FLAG_WRITE,
                                         s->vt->sendCallback);
         ASSERT_NOT_IMPLEMENTED(pollStatus == VMWARE_STATUS_SUCCESS);
      }
      s->sendCb = TRUE;
   }
   AsyncSocketRelease(s, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketIPollSendCallback --
 *
 *      IVmdbPoll callback for output socket buffer space available.  IVmdbPoll
 *      does not handle callback locks, so this function first locks the
 *      asyncsocket and verify that the send callback has not been cancelled.
 *      IVmdbPoll only has periodic callbacks, so this function unregisters
 *      itself before calling AsyncSocketSendCallback to do the real work.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Writes data, could trigger write completion or socket destruction.
 *
 *----------------------------------------------------------------------------
 */

static void
AsyncSocketIPollSendCallback(void *clientData)  // IN:
{
#ifdef VMX86_TOOLS
   NOT_IMPLEMENTED();
#else
   AsyncSocket *s = (AsyncSocket *) clientData;
   MXUserRecLock *lock;

   ASSERT(s);
   ASSERT(s->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   AsyncSocketLock(s);
   lock = s->pollParams.lock;
   if (s->sendCb) {
      /*
       * Unregister this callback as we want the non-periodic behavior.  There
       * is no need to take a reference here -- the fact that this callback is
       * running means AsyncsocketIPollRemove would not release a reference.
       * We would release that reference at the end.
       */
      if (s->sendCbTimer) {
         AsyncSocketIPollRemove(s, FALSE, 0, AsyncSocketIPollSendCallback);
      } else {
         AsyncSocketIPollRemove(s, TRUE, POLL_FLAG_WRITE,
                                AsyncSocketIPollSendCallback);
      }

      AsyncSocketSendCallback(s);
   }

   AsyncSocketRelease(s, TRUE);
   if (lock != NULL) {
      MXUser_DecRefRecLock(lock);
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketAddRef --
 *
 *    Increments reference count on AsyncSocket struct.
 *
 * Results:
 *    New reference count.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocketAddRef(AsyncSocket *s)
{
   ASSERT(s && s->refCount > 0);
   ASOCKLOG(1, s, ("%s (count now %d)\n", __FUNCTION__, s->refCount + 1));

   return ++s->refCount;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketReleaseSocket --
 *
 *      Release any resources used by a socket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
AsyncSocketReleaseSocket(AsyncSocket *s) // IN
{
   /* nothing to do */
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketRelease --
 *
 *    Decrements reference count on AsyncSocket struct, freeing it when it
 *    reaches 0.  If "unlock" is TRUE, releases the lock after decrementing
 *    the count.
 *
 * Results:
 *    New reference count; 0 if freed.
 *
 * Side effects:
 *    May free struct.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocketRelease(AsyncSocket *s,  // IN:
                   Bool unlock)     // IN: release lock
{
   int count = --s->refCount;

   if (unlock) {
      AsyncSocketUnlock(s);
   }
   if (0 == count) {
      ASOCKLOG(1, s, ("Final release; freeing asock struct\n"));
      ASSERT(s->vt);
      ASSERT(s->vt->release);
      s->vt->release(s);
      free(s);

      return 0;
   }
   ASOCKLOG(1, s, ("Release (count now %d)\n", count));

   return count;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketPollAdd --
 *
 *    Add a poll callback.  Wrapper for Poll_Callback since we always call
 *    it in one of two basic forms.
 *
 *    If socket is FALSE, user has to pass in the timeout value
 *
 * Results:
 *    VMwareStatus result code from Poll_Callback
 *
 * Side effects:
 *    Only the obvious.
 *
 *-----------------------------------------------------------------------------
 */

VMwareStatus
AsyncSocketPollAdd(AsyncSocket *asock,
                   Bool socket,
                   int flags,
                   PollerFunction callback,
                   ...)
{
   int type, info;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (socket) {
      type = POLL_DEVICE;
      flags |= POLL_FLAG_SOCKET;
      info = asock->fd;
   } else {
      va_list marker;
      va_start(marker, callback);

      type = POLL_REALTIME;
      info = va_arg(marker, int);

      va_end(marker);
   }

   if (asock->pollParams.iPoll != NULL) {
      return AsyncSocketIPollAdd(asock, socket, flags, callback, info);
   }

   return Poll_Callback(asock->pollParams.pollClass,
                        flags | asock->pollParams.flags,
                        callback, asock, type, info,
                        asock->pollParams.lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketPollRemove --
 *
 *    Remove a poll callback.  Wrapper for Poll_CallbackRemove since we
 *    always call it in one of two basic forms.
 *
 * Results:
 *    TRUE if removed, FALSE if not found.
 *
 * Side effects:
 *    Only the obvious.
 *
 *-----------------------------------------------------------------------------
 */

Bool
AsyncSocketPollRemove(AsyncSocket *asock,
                      Bool socket,
                      int flags,
                      PollerFunction callback)
{
   int type;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (asock->pollParams.iPoll != NULL) {
      return AsyncSocketIPollRemove(asock, socket, flags, callback);
   }

   if (socket) {
      type = POLL_DEVICE;
      flags |= POLL_FLAG_SOCKET;
   } else {
      type = POLL_REALTIME;
   }

   return Poll_CallbackRemove(asock->pollParams.pollClass,
                              flags | asock->pollParams.flags,
                              callback, asock, type);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketIPollAdd --
 *
 *    Add a poll callback.  Wrapper for IVmdbPoll.Register[Timer].
 *
 *    If socket is FALSE, user has to pass in the timeout value
 *
 * Results:
 *    VMwareStatus result code.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VMwareStatus
AsyncSocketIPollAdd(AsyncSocket *asock,
                    Bool socket,
                    int flags,
                    PollerFunction callback,
                    int info)
{
#ifdef VMX86_TOOLS
   return VMWARE_STATUS_ERROR;
#else
   VMwareStatus status = VMWARE_STATUS_SUCCESS;
   VmdbRet ret;
   IVmdbPoll *poll;

   ASSERT(asock->pollParams.iPoll);
   ASSERT(AsyncSocketIsLocked(asock));

   /* Protect asyncsocket and lock from disappearing */
   AsyncSocketAddRef(asock);
   if (asock->pollParams.lock != NULL) {
      MXUser_IncRefRecLock(asock->pollParams.lock);
   }

   poll = asock->pollParams.iPoll;

   if (socket) {
      int pollFlags = (flags & POLL_FLAG_READ) != 0 ? VMDB_PRF_READ
                                                    : VMDB_PRF_WRITE;

      ret = poll->Register(poll, pollFlags, callback, asock, info);
   } else {
      ret = poll->RegisterTimer(poll, callback, asock, info);
   }

   if (ret != VMDB_S_OK) {
      Log(ASOCKPREFIX "failed to register callback (%s %d): error %d\n",
          socket ? "socket" : "delay", info, ret);
      if (asock->pollParams.lock != NULL) {
         MXUser_DecRefRecLock(asock->pollParams.lock);
      }
      AsyncSocketRelease(asock, FALSE);
      status = VMWARE_STATUS_ERROR;
   }

   return status;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketIPollRemove --
 *
 *    Remove a poll callback.  Wrapper for IVmdbPoll.Unregister[Timer].
 *
 * Results:
 *    TRUE  if the callback was registered and has been cancelled successfully.
 *    FALSE if the callback was not registered, or the callback is already
 *          scheduled to fire (and is guaranteed to fire).
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
AsyncSocketIPollRemove(AsyncSocket *asock,
                       Bool socket,
                       int flags,
                       PollerFunction callback)
{
#ifdef VMX86_TOOLS
   return FALSE;
#else
   IVmdbPoll *poll;
   Bool ret;

   ASSERT(asock->pollParams.iPoll);
   ASSERT(AsyncSocketIsLocked(asock));

   poll = asock->pollParams.iPoll;

   if (socket) {
      int pollFlags = (flags & POLL_FLAG_READ) != 0 ? VMDB_PRF_READ
                                                    : VMDB_PRF_WRITE;

      ret = poll->Unregister(poll, pollFlags, callback, asock);
   } else {
      ret = poll->UnregisterTimer(poll, callback, asock);
   }

   if (ret) {
      MXUserRecLock *lock = asock->pollParams.lock;

      /* Release the reference taken when registering the callback. */
      AsyncSocketRelease(asock, FALSE);
      if (lock != NULL) {
         MXUser_DecRefRecLock(lock);
      }
   }

   return ret;
#endif
}




/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_CancelRecv --
 * AsyncSocket_CancelRecvEx --
 *
 *    Call this function if you know what you are doing. This should be
 *    called if you want to synchronously receive the outstanding data on
 *    the socket. It removes the recv poll callback for both tcp/udp sockets.
 *    For tcp socket it also returns number of partially read bytes (if any).
 *    A partially read response may exist as AsyncSocketRecvCallback calls
 *    the recv callback only when all the data has been received.
 *
 * Results:
 *    ASOCKERR_SUCCESS or ASOCKERR_INVAL.
 *
 * Side effects:
 *    Subsequent client call to AsyncSocket_Recv can reinstate async behaviour.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocket_CancelRecv(AsyncSocket *asock,         // IN
                       int *partialRecvd,          // OUT
                       void **recvBuf,             // OUT
                       void **recvFn)              // OUT
{
   return AsyncSocket_CancelRecvEx(asock, partialRecvd, recvBuf, recvFn, FALSE);
}

int
AsyncSocket_CancelRecvEx(AsyncSocket *asock,         // IN
                         int *partialRecvd,          // OUT
                         void **recvBuf,             // OUT
                         void **recvFn,              // OUT
                         Bool cancelOnSend)          // IN
{
   Bool isTcp;
   int retVal;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (!asock) {
      Warning(ASOCKPREFIX "Invalid socket while cancelling recv request!\n");

      return ASOCKERR_INVAL;
   }

   AsyncSocketLock(asock);

   if (asock->state != AsyncSocketConnected) {
      Warning(ASOCKPREFIX "Failed to cancel request on disconnected socket!\n");
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   if (asock->inBlockingRecv) {
      Warning(ASOCKPREFIX "Cannot cancel request while a blocking recv is "
                          "pending.\n");
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   isTcp = SOCK_STREAM == asock->type;

   if (!cancelOnSend && isTcp && (asock->sendBufList || asock->sendCb)) {
      Warning(ASOCKPREFIX "Can't cancel request as socket has send operation "
              "pending.\n");
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   ASSERT(asock->vt);
   ASSERT(asock->vt->cancelRecvCb);
   asock->vt->cancelRecvCb(asock);

   if (isTcp && partialRecvd && asock->recvLen > 0) {
      ASOCKLOG(1, asock, ("Partially read %d bytes out of %d bytes while "
                          "cancelling recv request.\n", asock->recvPos, asock->recvLen));
      *partialRecvd = asock->recvPos;
   }
   if (recvFn) {
      if (isTcp) {
         *recvFn = asock->recvFn;
      } else {
         *recvFn = asock->recvUDPFn;
      }
   }
   if (recvBuf) {
      *recvBuf = asock->recvBuf;
   }
   asock->recvBuf = NULL;
   if (isTcp) {
      asock->recvFn = NULL;
      asock->recvPos = 0;
   } else {
      asock->recvUDPFn = NULL;
   }
   asock->recvLen = 0;

   if (asock->passFd.fd != -1) {
      SSLGeneric_close(asock->passFd.fd);
      asock->passFd.fd = -1;
   }
   asock->passFd.expected = FALSE;

   retVal = ASOCKERR_SUCCESS;

outHaveLock:
   AsyncSocketUnlock(asock);
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_GetReceivedFd --
 *
 *    Retrieve received file descriptor from socket.
 *
 * Results:
 *    File descriptor.  Or -1 if none was received.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocket_GetReceivedFd(AsyncSocket *asock)      // IN
{
   int fd;

   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   if (!asock) {
      Warning(ASOCKPREFIX "Invalid socket while receiving fd!\n");

      return -1;
   }

   AsyncSocketLock(asock);

   if (asock->state != AsyncSocketConnected) {
      Warning(ASOCKPREFIX "Failed to receive fd on disconnected socket!\n");
      AsyncSocketUnlock(asock);

      return -1;
   }
   fd = asock->passFd.fd;
   asock->passFd.fd = -1;
   asock->passFd.expected = FALSE;

   AsyncSocketUnlock(asock);

   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectSSL --
 *
 *    Initialize the socket's SSL object, by calling SSL_ConnectAndVerify.
 *    NOTE: This call is blocking.
 *
 * Results:
 *    TRUE if SSL_ConnectAndVerify succeeded, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
AsyncSocket_ConnectSSL(AsyncSocket *asock,          // IN
                       SSLVerifyParam *verifyParam) // IN/OPT
{
   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   return SSL_ConnectAndVerify(asock->sslSock, verifyParam);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_AcceptSSL --
 *
 *    Initialize the socket's SSL object, by calling SSL_Accept.
 *
 * Results:
 *    TRUE if SSL_Accept succeeded, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
AsyncSocket_AcceptSSL(AsyncSocket *asock)  // IN
{
   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);

   return SSL_Accept(asock->sslSock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSslAcceptCallback --
 *
 *      Poll callback for redrive an outstanding ssl accept operation
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
AsyncSocketSslAcceptCallback(void *clientData)
{
   int sslOpCode;
   AsyncSocket *asock = (AsyncSocket *) clientData;
   VMwareStatus pollStatus;

   ASSERT(asock);
   ASSERT(asock->pollParams.iPoll == NULL);
   ASSERT(AsyncSocketIsLocked(asock));

   AsyncSocketAddRef(asock);

   sslOpCode = SSL_TryCompleteAccept(asock->sslSock);
   if (sslOpCode > 0) {
      (*asock->sslAcceptFn)(TRUE, asock, clientData);
   } else if (sslOpCode < 0) {
      (*asock->sslAcceptFn)(FALSE, asock, clientData);
   } else {
      /* register the poll callback to redrive the SSL accept */
      pollStatus = AsyncSocketPollAdd(asock, TRUE,
                                      SSL_WantRead(asock->sslSock) ?
                                      POLL_FLAG_READ : POLL_FLAG_WRITE,
                                      AsyncSocketSslAcceptCallback);

      if (pollStatus != VMWARE_STATUS_SUCCESS) {
         ASOCKWARN(asock, ("failed to reinstall ssl accept callback!\n"));
         (*asock->sslAcceptFn)(FALSE, asock, clientData);
      }
   }

   AsyncSocketRelease(asock, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_StartSslAccept --
 *
 *    Start an asynchronous SSL accept operation.
 *
 *    The supplied callback function is called when the operation is complete
 *    or an error occurs.
 *
 *    Note: The client callback could be invoked from this function or
 *          from a poll callback. If there is any requirement to always
 *          invoke the client callback from outside this function, consider
 *          changing this code to use a poll timer callback with timeout
 *          set to zero.
 *
 *    Note: sslCtx is typed as void *, so that the async socket code does
 *          not have to include the openssl header. This is in sync with
 *          SSL_AcceptWithContext(), where the sslCtx param is typed as void *
 * Results:
 *    None.
 *    Error is always reported using the callback supplied.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocket_StartSslAccept(AsyncSocket *asock,                 // IN
                           void *sslCtx,                       // IN
                           AsyncSocketSslAcceptFn sslAcceptFn, // IN
                           void *clientData)                   // IN
{
   Bool ok;

   ASSERT(asock);
   ASSERT(asock->asockType != ASYNCSOCKET_TYPE_NAMEDPIPE);
   ASSERT(sslAcceptFn);

   AsyncSocketLock(asock);

   if (asock->sslAcceptFn) {
      ASOCKWARN(asock, ("A SSL accept operation has already been initiated.\n"));
      goto done;
   }

   ok = SSL_SetupAcceptWithContext(asock->sslSock, sslCtx);
   if (!ok) {
      /* Something went wrong already */
      (*sslAcceptFn)(FALSE, asock, clientData);
      goto done;
   }

   asock->sslAcceptFn = sslAcceptFn;
   asock->clientData = clientData;

   AsyncSocketSslAcceptCallback(asock);

done:
   AsyncSocketUnlock(asock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_SetBufferSizes --
 *
 *    Set socket level recv/send buffer sizes if they are less than given sizes.
 *
 * Result
 *    TRUE: on success
 *    FALSE: on failure
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
AsyncSocket_SetBufferSizes(AsyncSocket *asock,  // IN
                           int sendSz,          // IN
                           int recvSz)          // IN
{
   int err;
   int buffSz;
   int len = sizeof buffSz;
   int sysErr;
   int fd;

   if (!asock) {
      return FALSE;
   }

   fd = asock->fd;

   err = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&buffSz, &len);
   if (err) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "Could not get recv buffer size for socket %d, "
              "error %d: %s\n", fd, sysErr, Err_Errno2String(sysErr));
      return FALSE;
   }

   if (buffSz < recvSz) {
      buffSz = recvSz;
      err = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&buffSz, len);
      if (err) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "Could not set recv buffer size for socket %d "
                 "to %d, error %d: %s\n", fd, buffSz,
                 sysErr, Err_Errno2String(sysErr));
         return FALSE;
      }
   }

   err =  getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&buffSz, &len);
   if (err) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "Could not get send buffer size for socket %d, "
              "error %d: %s\n", fd, sysErr, Err_Errno2String(sysErr));
      return FALSE;
   }

   if (buffSz < sendSz) {
      buffSz = sendSz;
      err = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&buffSz, len);
      if (err) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "Could not set send buffer size for socket %d "
                 "to %d, error %d: %s\n", fd, buffSz,
                 sysErr, Err_Errno2String(sysErr));
         return FALSE;
      }
   }

   return TRUE;
}

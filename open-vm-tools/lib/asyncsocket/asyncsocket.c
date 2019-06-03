/*********************************************************
 * Copyright (C) 2003-2019 VMware, Inc. All rights reserved.
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
 *      The AsyncTCPSocket object is a fairly simple wrapper around a basic TCP
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

#ifdef _WIN32
/*
 * We redefine strcpy/strcat because the Windows SDK uses it for getaddrinfo().
 * When we upgrade SDKs, this redefinition can go away.
 * Note: Now we are checking if we have secure libs for string operations
 */
#if !(defined(__GOT_SECURE_LIB__) && __GOT_SECURE_LIB__ >= 200402L)
#define strcpy(dst,src) Str_Strcpy((dst), (src), 0x7FFFFFFF)
#define strcat(dst,src) Str_Strcat((dst), (src), 0x7FFFFFFF)
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#include <MSWSock.h>
#include <windows.h>
#if !(defined(__GOT_SECURE_LIB__) && __GOT_SECURE_LIB__ >= 200402L)
#undef strcpy
#undef strcat
#endif
#else
#include <stddef.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "vmware.h"
#include "str.h"
#include "random.h"
#include "asyncsocket.h"
#include "asyncSocketBase.h"
#include "poll.h"
#include "log.h"
#include "err.h"
#include "hostinfo.h"
#include "util.h"
#include "msg.h"
#include "posix.h"
#include "vm_basic_asm.h"
#include "vmci_sockets.h"
#ifndef VMX86_TOOLS
#include "vmdblib.h"
#endif


#ifdef _WIN32
#define ASOCK_LASTERROR()       WSAGetLastError()
#else
#define ASOCK_LASTERROR()       errno
#endif


#define LOGLEVEL_MODULE asyncsocket
#include "loglevel_user.h"

#ifdef VMX86_SERVER
#include "uwvmkAPI.h"
#endif

#ifdef __linux__
/*
 * Our toolchain does not support IPV6_V6ONLY, but the host we are running on
 * may support it. Since setsockopt will return a error that we treat as
 * non-fatal, it is fine to attempt it. See define in in6.h.
 */
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 26
#endif

/*
 * Linux versions can lack support for IPV6_V6ONLY while still supporting
 * V4MAPPED addresses. We check for a V4MAPPED address during accept to cover
 * this scenario. In case IN6_IS_ADDR_V4MAPPED is also not avaiable, define it.
 */
#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a)                                   \
   (*(const u_int32_t *)(const void *)(&(a)->s6_addr[0]) == 0 &&  \
    *(const u_int32_t *)(const void *)(&(a)->s6_addr[4]) == 0 &&  \
    *(const u_int32_t *)(const void *)(&(a)->s6_addr[8]) == ntohl(0x0000ffff)))
#endif
#endif

#define PORT_STRING_LEN 6 /* "12345\0" or ":12345" */

#define IN_IPOLL_RECV (1 << 0)
#define IN_IPOLL_SEND (1 << 1)

/*
 * INET6_ADDRSTRLEN allows for only 45 characters. If we somehow have a
 * non-recommended V4MAPPED address we can exceed 45 total characters in our
 * address string format. While this should not be the case it is possible.
 * Account for the possible:
 *    "[XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:AAA.BBB.CCC.DDD]:12345\0"
 *    (XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:AAA.BBB.CCC.DDD\0 + [] + :12345)
 */
#define ADDR_STRING_LEN (INET6_ADDRSTRLEN + 2 + PORT_STRING_LEN)


/* Local types. */

/*
 * Output buffer list data type, for the queue of outgoing buffers
 */
typedef struct SendBufList {
   struct SendBufList   *next;
   void                 *buf;
   int                   len;
   AsyncSocketSendFn     sendFn;
   void                 *clientData;
} SendBufList;


typedef struct AsyncTCPSocket {
   /*
    * The base class, which is just a vtable:
    */
   AsyncSocket base;

   /*
    * Everything for the TCP AsyncSocket implementation:
    */
   int fd;
   SSLSock sslSock;

   int genericErrno;

   struct sockaddr_storage localAddr;
   socklen_t localAddrLen;
   struct sockaddr_storage remoteAddr;
   socklen_t remoteAddrLen;

   AsyncSocketConnectFn connectFn;
   AsyncSocketSslAcceptFn sslAcceptFn;
   AsyncSocketSslConnectFn sslConnectFn;
   int sslPollFlags;       /* shared by sslAcceptFn, sslConnectFn */

   /* shared by connectFn, sslAcceptFn and sslConnectFn */
   void *clientData;

   PollerFunction internalConnectFn;
   PollerFunction internalSendFn;
   PollerFunction internalRecvFn;

   /* governs optional AsyncSocket_Close() behavior */
   int flushEnabledMaxWaitMsec;
   AsyncSocketCloseFn closeCb;
   void *closeCbData;

   Bool recvCb;
   Bool recvCbTimer;

   SendBufList *sendBufList;
   SendBufList **sendBufTail;
   int sendPos;
   Bool sendCb;
   Bool sendCbTimer;
   Bool sendCbRT;
   Bool sendBufFull;
   Bool sendLowLatency;
   int inLowLatencySendCb;

   Bool sslConnected;

   uint8 inIPollCb;
   Bool inRecvLoop;
   uint32 inBlockingRecv;

   struct AsyncTCPSocket *listenAsock4;
   struct AsyncTCPSocket *listenAsock6;

   struct {
      Bool expected;
      int fd;
   } passFd;

} AsyncTCPSocket;



/*
 * Local Functions
 */
static AsyncTCPSocket *AsyncTCPSocketCreate(AsyncSocketPollParams *pollParams);
static void AsyncTCPSocketSendCallback(void *clientData);
static void AsyncTCPSocketRecvCallback(void *clientData);
static int AsyncTCPSocketResolveAddr(const char *hostname,
                                     unsigned int port,
                                     int family,
                                     Bool passive,
                                     struct sockaddr_storage *addr,
                                     socklen_t *addrLen,
                                     char **addrString);
static AsyncTCPSocket *AsyncTCPSocketAttachToFd(
   int fd, AsyncSocketPollParams *pollParams, int *outError);
static Bool AsyncTCPSocketHasDataPending(AsyncTCPSocket *asock);
static int AsyncTCPSocketMakeNonBlocking(int fd);
static void AsyncTCPSocketAcceptCallback(void *clientData);
static void AsyncTCPSocketConnectCallback(void *clientData);
static int AsyncTCPSocketBlockingWork(AsyncTCPSocket *asock, Bool read,
                                      void *buf, int len,
                                      int *completed, int timeoutMS,
                                      Bool partial);
static VMwareStatus AsyncTCPSocketPollAdd(AsyncTCPSocket *asock, Bool socket,
                                          int flags, PollerFunction callback,
                                          ...);
static Bool AsyncTCPSocketPollRemove(AsyncTCPSocket *asock, Bool socket,
                                     int flags, PollerFunction callback);
static unsigned int AsyncTCPSocketGetPortFromAddr(
   struct sockaddr_storage *addr);
static AsyncTCPSocket *AsyncTCPSocketConnect(struct sockaddr_storage *addr,
                                             socklen_t addrLen,
                                             int socketFd,
                                             AsyncSocketConnectFn connectFn,
                                             void *clientData,
                                             AsyncSocketConnectFlags flags,
                                             AsyncSocketPollParams *pollParams,
                                             int *outError);
static int AsyncTCPSocketConnectInternal(AsyncTCPSocket *s);

static VMwareStatus AsyncTCPSocketIPollAdd(AsyncTCPSocket *asock, Bool socket,
                                           int flags, PollerFunction callback,
                                           int info);
static Bool AsyncTCPSocketIPollRemove(AsyncTCPSocket *asock, Bool socket,
                                      int flags, PollerFunction callback);
static void AsyncTCPSocketIPollSendCallback(void *clientData);
static void AsyncTCPSocketIPollRecvCallback(void *clientData);
static Bool AsyncTCPSocketAddListenCb(AsyncTCPSocket *asock);
static void AsyncTCPSocketSslConnectCallback(void *clientData);
static void AsyncTCPSocketSslAcceptCallback(void *clientData);

static Bool AsyncTCPSocketBind(AsyncTCPSocket *asock,
                               struct sockaddr_storage *addr,
                               socklen_t addrLen,
                               int *outError);
static Bool AsyncTCPSocketListen(AsyncTCPSocket *asock,
                                 AsyncSocketConnectFn connectFn,
                                 void *clientData,
                                 int *outError);
static AsyncTCPSocket *AsyncTCPSocketInit(int socketFamily,
                                          AsyncSocketPollParams *pollParams,
                                          int *outError);

static void AsyncTCPSocketCancelListenCb(AsyncTCPSocket *asock);


static int AsyncTCPSocketRegisterRecvCb(AsyncTCPSocket *asock);
static Bool AsyncTCPSocketCancelCbForConnectingClose(AsyncTCPSocket *asock);

static int AsyncTCPSocketWaitForConnection(AsyncSocket *s, int timeoutMS);
static int AsyncTCPSocketGetGenericErrno(AsyncSocket *s);
static int AsyncTCPSocketGetFd(AsyncSocket *asock);
static int AsyncTCPSocketGetRemoteIPStr(AsyncSocket *asock, const char **ipStr);
static int AsyncTCPSocketGetINETIPStr(AsyncSocket *asock, int socketFamily,
                                      char **ipRetStr);
static unsigned int AsyncTCPSocketGetPort(AsyncSocket *asock);
static Bool AsyncTCPSocketConnectSSL(AsyncSocket *asock,
                                     struct _SSLVerifyParam *verifyParam,
                                     void *sslContext);
static int AsyncTCPSocketStartSslConnect(AsyncSocket *asock,
                                         SSLVerifyParam *verifyParam,
                                         void *sslCtx,
                                         AsyncSocketSslConnectFn sslConnectFn,
                                         void *clientData);
static Bool AsyncTCPSocketAcceptSSL(AsyncSocket *asock, void *sslCtx);
static int AsyncTCPSocketStartSslAccept(AsyncSocket *asock, void *sslCtx,
                                        AsyncSocketSslAcceptFn sslAcceptFn,
                                        void *clientData);
static int AsyncTCPSocketFlush(AsyncSocket *asock, int timeoutMS);
static void AsyncTCPSocketCancelRecvCb(AsyncTCPSocket *asock);

static int AsyncTCPSocketRecv(AsyncSocket *asock,
             void *buf, int len, Bool partial, void *cb, void *cbData);
static int AsyncTCPSocketRecvPassedFd(AsyncSocket *asock, void *buf, int len,
                     void *cb, void *cbData);
static int AsyncTCPSocketGetReceivedFd(AsyncSocket *asock);
static int AsyncTCPSocketSend(AsyncSocket *asock, void *buf, int len,
                              AsyncSocketSendFn sendFn, void *clientData);
static int AsyncTCPSocketIsSendBufferFull(AsyncSocket *asock);
static int AsyncTCPSocketClose(AsyncSocket *asock);
static int AsyncTCPSocketCancelRecv(AsyncSocket *asock, int *partialRecvd,
                                    void **recvBuf, void **recvFn,
                                    Bool cancelOnSend);
static int AsyncTCPSocketCancelCbForClose(AsyncSocket *asock);
static int AsyncTCPSocketGetLocalVMCIAddress(AsyncSocket *asock,
                            uint32 *cid, uint32 *port);
static int AsyncTCPSocketGetRemoteVMCIAddress(AsyncSocket *asock,
                             uint32 *cid, uint32 *port);
static int AsyncTCPSocketSetCloseOptions(AsyncSocket *asock,
                                          int flushEnabledMaxWaitMsec,
                                          AsyncSocketCloseFn closeCb);
static void AsyncTCPSocketDestroy(AsyncSocket *s);
static int AsyncTCPSocketRecvBlocking(AsyncSocket *s, void *buf, int len,
                                      int *received, int timeoutMS);
static int AsyncTCPSocketRecvPartialBlocking(AsyncSocket *s, void *buf, int len,
                                             int *received, int timeoutMS);
static int AsyncTCPSocketSendBlocking(AsyncSocket *s, void *buf, int len,
                                      int *sent, int timeoutMS);
static int AsyncTCPSocketDoOneMsg(AsyncSocket *s, Bool read, int timeoutMS);
static int AsyncTCPSocketWaitForReadMultiple(AsyncSocket **asock, int numSock,
                                             int timeoutMS, int *outIdx);
static int AsyncTCPSocketSetOption(AsyncSocket *asyncSocket,
                                   AsyncSocketOpts_Layer layer,
                                   AsyncSocketOpts_ID optID,
                                   const void *valuePtr,
                                   socklen_t inBufLen);
static int AsyncTCPSocketGetOption(AsyncSocket *asyncSocket,
                                   AsyncSocketOpts_Layer layer,
                                   AsyncSocketOpts_ID optID,
                                   void *valuePtr,
                                   socklen_t *outBufLen);
static void AsyncTCPSocketListenerError(int error,
                                        AsyncSocket *asock,
                                        void *clientData);


/* Local constants. */

static const AsyncSocketVTable asyncTCPSocketVTable = {
   AsyncSocketGetState,
   AsyncTCPSocketSetOption,
   AsyncTCPSocketGetOption,
   AsyncTCPSocketGetGenericErrno,
   AsyncTCPSocketGetFd,
   AsyncTCPSocketGetRemoteIPStr,
   AsyncTCPSocketGetINETIPStr,
   AsyncTCPSocketGetPort,
   AsyncTCPSocketSetCloseOptions,
   AsyncTCPSocketConnectSSL,
   AsyncTCPSocketStartSslConnect,
   AsyncTCPSocketAcceptSSL,
   AsyncTCPSocketStartSslAccept,
   AsyncTCPSocketFlush,
   AsyncTCPSocketRecv,
   AsyncTCPSocketRecvPassedFd,
   AsyncTCPSocketGetReceivedFd,
   AsyncTCPSocketSend,
   AsyncTCPSocketIsSendBufferFull,
   NULL,                        /* getNetworkStats */
   AsyncTCPSocketClose,
   AsyncTCPSocketCancelRecv,
   AsyncTCPSocketCancelCbForClose,
   AsyncTCPSocketGetLocalVMCIAddress,
   AsyncTCPSocketGetRemoteVMCIAddress,
   NULL,                        /* getWebSocketError */
   NULL,                        /* getWebSocketURI */
   NULL,                        /* getWebSocketCookie */
   NULL,                        /* getWebSocketCloseStatus */
   NULL,                        /* getWebSocketProtocol */
   NULL,                        /* setWebSocketCookie */
   AsyncTCPSocketRecvBlocking,
   AsyncTCPSocketRecvPartialBlocking,
   AsyncTCPSocketSendBlocking,
   AsyncTCPSocketDoOneMsg,
   AsyncTCPSocketWaitForConnection,
   AsyncTCPSocketWaitForReadMultiple,
   AsyncTCPSocketDestroy
};


/* Function bodies. */

/*
 *----------------------------------------------------------------------
 *
 * BaseSocket --
 *
 *      Return a pointer to the tcp socket's base class.
 *
 *----------------------------------------------------------------------
 */

static INLINE AsyncSocket *
BaseSocket(AsyncTCPSocket *s)
{
   ASSERT((void *)s == (void *)&s->base);
   return &s->base;
}


/*
 *----------------------------------------------------------------------
 *
 * TCPSocket --
 *
 *      Cast a generic AsyncSocket pointer to AsyncTCPSocket, after
 *      asserting this is legal.
 *
 *----------------------------------------------------------------------
 */

static INLINE AsyncTCPSocket *
TCPSocket(AsyncSocket *s)
{
   ASSERT(s->vt == &asyncTCPSocketVTable);
   ASSERT(s == &((AsyncTCPSocket *)s)->base);
   return (AsyncTCPSocket *)s;
}


/*
 *----------------------------------------------------------------------
 *
 * TCPSocketLock --
 * TCPSocketUnlock --
 * TCPSocketIsLocked --
 * TCPSocketAddRef --
 * TCPSocketRelease --
 * TCPSocketPollParams --
 * TCPSocketGetState --
 * TCPSocketSetState --
 * TCPSocketHandleError --
 *
 *      AsyncTCPSocket versions of base class interfaces.  These
 *      simply invoke the corresponding function on the base class
 *      pointer.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
AsyncTCPSocketLock(AsyncTCPSocket *asock)
{
   AsyncSocketLock(BaseSocket(asock));
}

static INLINE void
AsyncTCPSocketUnlock(AsyncTCPSocket *asock)
{
   AsyncSocketUnlock(BaseSocket(asock));
}

static INLINE Bool
AsyncTCPSocketIsLocked(AsyncTCPSocket *asock)
{
   return AsyncSocketIsLocked(BaseSocket(asock));
}

static INLINE void
AsyncTCPSocketAddRef(AsyncTCPSocket *asock)
{
   AsyncSocketAddRef(BaseSocket(asock));
}

static INLINE void
AsyncTCPSocketRelease(AsyncTCPSocket *asock)
{
   AsyncSocketRelease(BaseSocket(asock));
}

static INLINE AsyncSocketPollParams *
AsyncTCPSocketPollParams(AsyncTCPSocket *asock)
{
   return AsyncSocketGetPollParams(BaseSocket(asock));
}

static INLINE Bool
AsyncTCPSocketGetState(AsyncTCPSocket *asock)
{
   return AsyncSocketGetState(BaseSocket(asock));
}

static INLINE void
AsyncTCPSocketSetState(AsyncTCPSocket *asock, AsyncSocketState state)
{
   AsyncSocketSetState(BaseSocket(asock), state);
}

static INLINE void
AsyncTCPSocketHandleError(AsyncTCPSocket *asock, int error)
{
   AsyncSocketHandleError(BaseSocket(asock), error);
}


/*
 *----------------------------------------------------------------------
 *
 * TCPSOCKWARN --
 * TCPSOCKLOG --
 * TCPSOCKLG0 --
 *
 *      AsyncTCPSocket versions of base class logging macros.  These
 *      simply invoke the corresponding macro on the base class
 *      pointer.
 *
 *----------------------------------------------------------------------
 */

#define TCPSOCKWARN(a,b) ASOCKWARN(BaseSocket(a), b)
#define TCPSOCKLOG(a,b,c) ASOCKLOG(a, BaseSocket(b), c)
#define TCPSOCKLG0(a,b) ASOCKLG0(BaseSocket(a), b)


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocket_Init --
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
AsyncTCPSocket_Init(void)
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
 * AsyncTCPSocketGetFd --
 *
 *      Returns the fd for this socket.  If listening, return one of
 *      the asock6/asock4 fds.
 *
 * Results:
 *      File descriptor.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketGetFd(AsyncSocket *base)         // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);

   if (asock->fd != -1) {
      return asock->fd;
   } else if (asock->listenAsock4 && asock->listenAsock4->fd != -1) {
      return asock->listenAsock4->fd;
   } else if (asock->listenAsock6 && asock->listenAsock6->fd != -1) {
      return asock->listenAsock6->fd;
   } else {
      return -1;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetAddr --
 *
 *      Given an AsyncTCPSocket object, return the sockaddr associated with the
 *      requested address family's file descriptor if available.
 *
 *      Passing AF_UNSPEC to socketFamily will provide you with the first
 *      usable sockaddr found (if multiple are available), with a preference
 *      given to IPv6.
 *
 * Results:
 *      ASOCKERR_SUCCESS. ASOCKERR_INVAL if there is no socket associated with
 *      address family requested. ASOCKERR_GENERIC for all other errors.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketGetAddr(AsyncTCPSocket *asock,             // IN
                      int socketFamily,                  // IN
                      struct sockaddr_storage *outAddr,  // OUT
                      socklen_t *outAddrLen)             // IN/OUT
{
   AsyncTCPSocket *tempAsock;
   int tempFd;
   struct sockaddr_storage addr;
   socklen_t addrLen = sizeof addr;

   if (asock->fd != -1) {
      tempAsock = asock;
   } else if ((socketFamily == AF_UNSPEC || socketFamily == AF_INET6) &&
              asock->listenAsock6 && asock->listenAsock6->fd != -1) {
      tempAsock = asock->listenAsock6;
   } else if ((socketFamily == AF_UNSPEC || socketFamily == AF_INET) &&
              asock->listenAsock4 && asock->listenAsock4->fd != -1) {
      tempAsock = asock->listenAsock4;
   } else {
      return ASOCKERR_INVAL;
   }

   ASSERT(AsyncTCPSocketIsLocked(tempAsock));
   tempFd = tempAsock->fd;

   if (getsockname(tempFd, (struct sockaddr*)&addr, &addrLen) == 0) {
      if (socketFamily != AF_UNSPEC && addr.ss_family != socketFamily) {
         return ASOCKERR_INVAL;
      }

      memcpy(outAddr, &addr, Min(*outAddrLen, addrLen));
      *outAddrLen = addrLen;
      return ASOCKERR_SUCCESS;
   } else {
      TCPSOCKWARN(tempAsock, ("%s: could not locate socket.\n", __FUNCTION__));
      return ASOCKERR_GENERIC;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetRemoteIPStr --
 *
 *      Given an AsyncTCPSocket object, returns the remote IP address
 *      associated with it, or an error if the request is meaningless
 *      for the underlying connection.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketGetRemoteIPStr(AsyncSocket *base,      // IN
                             const char **ipRetStr)  // OUT
{
   AsyncTCPSocket *asock = TCPSocket(base);
   int ret = ASOCKERR_SUCCESS;

   ASSERT(asock);
   ASSERT(ipRetStr != NULL);

   if (ipRetStr == NULL || asock == NULL ||
       AsyncTCPSocketGetState(asock) != AsyncSocketConnected ||
       (asock->remoteAddrLen != sizeof (struct sockaddr_in) &&
        asock->remoteAddrLen != sizeof (struct sockaddr_in6))) {
      ret = ASOCKERR_GENERIC;
   } else {
      char addrBuf[NI_MAXHOST];

      if (Posix_GetNameInfo((struct sockaddr *)&asock->remoteAddr,
                            asock->remoteAddrLen, addrBuf,
                            sizeof addrBuf, NULL, 0, NI_NUMERICHOST) != 0) {
         ret = ASOCKERR_GENERIC;
      } else {
         *ipRetStr = Util_SafeStrdup(addrBuf);
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetINETIPStr --
 *
 *      Given an AsyncTCPSocket object, returns the IP addresses associated with
 *      the requested address family's file descriptor if available.
 *
 *      Passing AF_UNSPEC to socketFamily will provide you with the first
 *      usable IP address found (if multiple are available), with a preference
 *      given to IPv6.
 *
 *      It is the caller's responsibility to free ipRetStr.
 *
 * Results:
 *      ASOCKERR_SUCCESS. ASOCKERR_INVAL if there is no socket associated with
 *      address family requested. ASOCKERR_GENERIC for all other errors.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketGetINETIPStr(AsyncSocket *base,   // IN
                           int socketFamily,    // IN
                           char **ipRetStr)     // OUT
{
   AsyncTCPSocket *asock = TCPSocket(base);
   struct sockaddr_storage addr;
   socklen_t addrLen = sizeof addr;
   int ret;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   ret = AsyncTCPSocketGetAddr(asock, socketFamily, &addr, &addrLen);
   if (ret == ASOCKERR_SUCCESS) {
      char addrBuf[NI_MAXHOST];

      if (ipRetStr == NULL) {
         TCPSOCKWARN(asock, ("%s: Output string is not usable.\n",
                             __FUNCTION__));
         ret = ASOCKERR_INVAL;
      } else if (Posix_GetNameInfo((struct sockaddr *)&addr, addrLen, addrBuf,
                                   sizeof addrBuf, NULL, 0,
                                   NI_NUMERICHOST) == 0) {
         *ipRetStr = Util_SafeStrdup(addrBuf);
      } else {
         TCPSOCKWARN(asock, ("%s: could not find IP address.\n", __FUNCTION__));
         ret = ASOCKERR_GENERIC;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetLocalVMCIAddress --
 *
 *      Given an AsyncTCPSocket object, returns the local VMCI context ID and
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

static int
AsyncTCPSocketGetLocalVMCIAddress(AsyncSocket *base,   // IN
                                  uint32 *cid,         // OUT: optional
                                  uint32 *port)        // OUT: optional
{
   AsyncTCPSocket *asock = TCPSocket(base);
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
 * AsyncTCPSocketGetRemoteVMCIAddress --
 *
 *      Given an AsyncTCPSocket object, returns the remote VMCI context ID and
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

static int
AsyncTCPSocketGetRemoteVMCIAddress(AsyncSocket *base,   // IN
                                   uint32 *cid,         // OUT: optional
                                   uint32 *port)        // OUT: optional
{
   AsyncTCPSocket *asock = TCPSocket(base);
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
 * AsyncTCPSocketListenImpl --
 *
 *      Initializes, binds, and listens on pre-populated address structure.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketListenImpl(struct sockaddr_storage *addr,      // IN
                         socklen_t addrLen,                  // IN
                         AsyncSocketConnectFn connectFn,     // IN
                         void *clientData,                   // IN
                         AsyncSocketPollParams *pollParams,  // IN: optional
                         int *outError)                      // OUT: optional
{
   AsyncTCPSocket *asock = AsyncTCPSocketInit(addr->ss_family, pollParams,
                                              outError);

   if (asock != NULL) {
      if (AsyncTCPSocketBind(asock, addr, addrLen, outError) &&
          AsyncTCPSocketListen(asock, connectFn, clientData, outError)) {
         return asock;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketListenerCreateImpl --
 *
 *      Listens on specified address and/or port for resolved/requested socket
 *      family and accepts new connections. Fires the connect callback with
 *      new AsyncTCPSocket object for each connection.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketListenerCreateImpl(
   const char *addrStr,                // IN: optional
   unsigned int port,                  // IN: optional
   int socketFamily,                   // IN
   AsyncSocketConnectFn connectFn,     // IN
   void *clientData,                   // IN
   AsyncSocketPollParams *pollParams,  // IN
   int *outError)                      // OUT: optional
{
   AsyncTCPSocket *asock = NULL;
   struct sockaddr_storage addr;
   socklen_t addrLen;
   char *ipString = NULL;
   int getaddrinfoError = AsyncTCPSocketResolveAddr(addrStr, port, socketFamily,
                                                    TRUE, &addr, &addrLen,
                                                    &ipString);

   if (getaddrinfoError == 0) {
      asock = AsyncTCPSocketListenImpl(&addr, addrLen, connectFn, clientData,
                                       pollParams,
                                       outError);

      if (asock) {
         TCPSOCKLG0(asock,
                  ("Created new %s %s listener for (%s)\n",
                   addr.ss_family == AF_INET ? "IPv4" : "IPv6",
                   "socket", ipString));
      } else {
         Log(ASOCKPREFIX "Could not create %s listener socket, error %d: %s\n",
             addr.ss_family == AF_INET ? "IPv4" : "IPv6", *outError,
             AsyncSocket_Err2String(*outError));
      }
      free(ipString);
   } else {
      Log(ASOCKPREFIX "Could not resolve listener socket address.\n");
      if (outError) {
         *outError = ASOCKERR_ADDRUNRESV;
      }
   }

   return asock;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Listen --
 *
 *      Listens on specified address and/or port for all resolved socket
 *      families and accepts new connections. Fires the connect callback with
 *      new AsyncTCPSocket object for each connection.
 *
 *      If address string is present and that string is not the "localhost"
 *      loopback, then we will listen on resolved address only.
 *
 *      If address string is NULL or is "localhost" we will listen on all
 *      address families that will resolve on the host.
 *
 *      If port requested is 0, we will let the system assign the first
 *      available port.
 *
 *      If address string is NULL and port requested is not 0, we will listen
 *      on any address for all resolved protocols for the port requested.
 *
 *      If address string is "localhost" and port is 0, we will use the first
 *      port we are given if the host supports multiple address families.
 *      If by chance we try to bind on a port that is available for one
 *      protocol and not the other, we will attempt a second time with the
 *      order of address families reversed.
 *
 *      If address string is NULL, port cannot be 0.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket/s, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_Listen(const char *addrStr,                // IN: optional
                   unsigned int port,                  // IN: optional
                   AsyncSocketConnectFn connectFn,     // IN
                   void *clientData,                   // IN
                   AsyncSocketPollParams *pollParams,  // IN
                   int *outError)                      // OUT: optional
{
   if (addrStr != NULL && *addrStr != '\0' &&
       Str_Strcmp(addrStr, "localhost")) {
      AsyncTCPSocket *asock;

      asock = AsyncTCPSocketListenerCreateImpl(addrStr, port, AF_UNSPEC,
                                               connectFn,
                                               clientData, pollParams,
                                               outError);
      return BaseSocket(asock);
   } else {
      Bool localhost = addrStr != NULL && !Str_Strcmp(addrStr, "localhost");
      unsigned int tempPort = port;
      AsyncTCPSocket *asock6 = NULL;
      AsyncTCPSocket *asock4 = NULL;
      int tempError4;
      int tempError6;

      asock6 = AsyncTCPSocketListenerCreateImpl(addrStr, port, AF_INET6,
                                                connectFn, clientData,
                                                pollParams,
                                                &tempError6);

      if (localhost && port == 0) {
         tempPort = AsyncSocket_GetPort(BaseSocket(asock6));
         if (tempPort == MAX_UINT32) {
            Log(ASOCKPREFIX
                "Could not resolve IPv6 listener socket port number.\n");
            tempPort = port;
         }
      }

      asock4 = AsyncTCPSocketListenerCreateImpl(addrStr, tempPort, AF_INET,
                                                connectFn, clientData,
                                                pollParams,
                                                &tempError4);

      if (localhost && port == 0 && tempError4 == ASOCKERR_BINDADDRINUSE) {
         Log(ASOCKPREFIX "Failed to reuse IPv6 localhost port number for IPv4 "
             "listener socket.\n");
         AsyncSocket_Close(BaseSocket(asock6));

         tempError4 = ASOCKERR_SUCCESS;
         asock4 = AsyncTCPSocketListenerCreateImpl(addrStr, port, AF_INET,
                                                   connectFn, clientData,
                                                   pollParams,
                                                   &tempError4);

         tempPort = AsyncSocket_GetPort(BaseSocket(asock4));
         if (tempPort == MAX_UINT32) {
            Log(ASOCKPREFIX
                "Could not resolve IPv4 listener socket port number.\n");
            tempPort = port;
         }

         tempError6 = ASOCKERR_SUCCESS;
         asock6 = AsyncTCPSocketListenerCreateImpl(addrStr, tempPort, AF_INET6,
                                                   connectFn, clientData,
                                                   pollParams,
                                                   &tempError6);

         if (!asock6 && tempError6 == ASOCKERR_BINDADDRINUSE) {
            Log(ASOCKPREFIX "Failed to reuse IPv4 localhost port number for "
                "IPv6 listener socket.\n");
            AsyncSocket_Close(BaseSocket(asock4));
         }
      }

      if (asock6 && asock4) {
         AsyncTCPSocket *asock;

         asock = AsyncTCPSocketCreate(pollParams);
         AsyncTCPSocketSetState(asock, AsyncSocketListening);
         asock->listenAsock6 = asock6;
         asock->listenAsock4 = asock4;
         AsyncSocket_SetErrorFn(BaseSocket(asock4),
                                AsyncTCPSocketListenerError,
                                asock);
         AsyncSocket_SetErrorFn(BaseSocket(asock6),
                                AsyncTCPSocketListenerError,
                                asock);

         return BaseSocket(asock);
      } else if (asock6) {
         return BaseSocket(asock6);
      } else if (asock4) {
         return BaseSocket(asock4);
      }

      if (outError) {
         /* Client only gets one error and the one for IPv6 is favored. */
         if (!asock6) {
            *outError = tempError6;
         } else if (!asock4) {
            *outError = tempError4;
         } else {
            *outError = ASOCKERR_LISTEN;
         }
      }

      return NULL;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketListenerCreateLoopback --
 *
 *      Listens on loopback interface and port for all resolved socket
 *      families and accepts new connections. Fires the connect callback with
 *      new AsyncTCPSocket object for each connection.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error.
 *
 * Side effects:
 *      Creates new socket/s, binds and listens.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ListenLoopback(unsigned int port,                  // IN
                           AsyncSocketConnectFn connectFn,     // IN
                           void *clientData,                   // IN
                           AsyncSocketPollParams *pollParams,  // IN
                           int *outError)                      // OUT: optional
{
   AsyncTCPSocket *asock6 = NULL;
   AsyncTCPSocket *asock4 = NULL;
   int tempError4;
   int tempError6;

   /*
    * "localhost6" does not work on Windows. "localhost" does
    * not work for IPv6 on old Linux versions like 2.6.18. So,
    * using IP address for both the cases to be consistent.
    */
   asock6 = AsyncTCPSocketListenerCreateImpl("::1", port, AF_INET6,
                                             connectFn, clientData, pollParams,
                                             &tempError6);

   asock4 = AsyncTCPSocketListenerCreateImpl("127.0.0.1", port, AF_INET,
                                             connectFn, clientData, pollParams,
                                             &tempError4);

   if (asock6 && asock4) {
      AsyncTCPSocket *asock;

      asock = AsyncTCPSocketCreate(pollParams);
      AsyncTCPSocketSetState(asock, AsyncSocketListening);
      asock->listenAsock6 = asock6;
      asock->listenAsock4 = asock4;

      return BaseSocket(asock);
   } else if (asock6) {
      return BaseSocket(asock6);
   } else if (asock4) {
      return BaseSocket(asock4);
   }

   if (outError) {
      /* Client only gets one error and the one for IPv6 is favored. */
      if (!asock6) {
         *outError = tempError6;
      } else if (!asock4) {
         *outError = tempError4;
      } else {
         *outError = ASOCKERR_LISTEN;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocket_ListenVMCI --
 *
 *      Listens on the specified port and accepts new connections. Fires the
 *      connect callback with new AsyncTCPSocket object for each connection.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error.
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
   struct sockaddr_vm addr;
   AsyncTCPSocket *asock;
   int vsockDev = -1;

   memset(&addr, 0, sizeof addr);
   addr.svm_family = VMCISock_GetAFValueFd(&vsockDev);
   addr.svm_cid = cid;
   addr.svm_port = port;

   asock = AsyncTCPSocketListenImpl((struct sockaddr_storage *)&addr,
                                    sizeof addr,
                                    connectFn, clientData, pollParams,
                                    outError);

   VMCISock_ReleaseAFValueFd(vsockDev);
   return BaseSocket(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketInit --
 *
 *      This is an internal routine that sets up a SOCK_STREAM (TCP) socket.
 *
 * Results:
 *      New AsyncTCPSocket or NULL on error.
 *
 * Side effects:
 *      Creates new socket.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketInit(int socketFamily,                  // IN
                   AsyncSocketPollParams *pollParams, // IN
                   int *outError)                     // OUT
{
   AsyncTCPSocket *asock = NULL;
   int error = ASOCKERR_GENERIC;
   int sysErr;
   int fd;

   /*
    * Create a new socket
    */

   if ((fd = socket(socketFamily, SOCK_STREAM, 0)) == -1) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "could not create new socket, error %d: %s\n",
              sysErr, Err_Errno2String(sysErr));
      goto errorNoFd;
   }

   /*
    * Wrap it with an asock object
    */

   if ((asock = AsyncTCPSocketAttachToFd(fd, pollParams, &error)) == NULL) {
      goto error;
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
 * AsyncTCPSocketGetPortFromAddr --
 *
 *      This is an internal routine that gets a port given an address.  The
 *      address must be in either AF_INET, AF_INET6 or AF_VMCI format.
 *
 * Results:
 *      Port number (in host byte order for INET).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
AsyncTCPSocketGetPortFromAddr(struct sockaddr_storage *addr)         // IN
{
   ASSERT(NULL != addr);

   if (AF_INET == addr->ss_family) {
      return ntohs(((struct sockaddr_in *)addr)->sin_port);
   } else if (AF_INET6 == addr->ss_family) {
      return ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
#ifndef _WIN32
   } else if (AF_UNIX == addr->ss_family) {
      return MAX_UINT32; // Not applicable
#endif
   } else {
#ifdef VMX86_DEBUG
      int vsockDev = -1;

      ASSERT(VMCISock_GetAFValueFd(&vsockDev) == addr->ss_family);
      VMCISock_ReleaseAFValueFd(vsockDev);

#endif
      return ((struct sockaddr_vm *)addr)->svm_port;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetPort --
 *
 *      Given an AsyncTCPSocket object, returns the port number associated with
 *      the requested address family's file descriptor if available.
 *
 * Results:
 *      Port number in host byte order. MAX_UINT32 on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static unsigned int
AsyncTCPSocketGetPort(AsyncSocket *base)  // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   AsyncTCPSocket *tempAsock;
   struct sockaddr_storage addr;
   socklen_t addrLen = sizeof addr;
   unsigned int ret = MAX_UINT32;

   if (asock->fd != -1) {
      tempAsock = asock;
   } else if (asock->listenAsock6 && asock->listenAsock6->fd != -1) {
      tempAsock = asock->listenAsock6;
   } else if (asock->listenAsock4 && asock->listenAsock4->fd != -1) {
      tempAsock = asock->listenAsock4;
   } else {
      return ret;
   }

   ASSERT(AsyncTCPSocketIsLocked(asock));
   ASSERT(AsyncTCPSocketIsLocked(tempAsock));

   if (AsyncTCPSocketGetAddr(tempAsock, AF_UNSPEC, &addr, &addrLen) ==
       ASOCKERR_SUCCESS) {
      return AsyncTCPSocketGetPortFromAddr(&addr);
   } else {
      return MAX_UINT32;
   }


   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketOSVersionSupportsV4Mapped --
 *
 *      Determine if runtime environment supports IPv4-mapped IPv6 addressed
 *      and all the functionality needed to deal with this scenario.
 *
 * Results:
 *      Returns TRUE if supported.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
AsyncTCPSocketOSVersionSupportsV4Mapped(void)
{
#if defined(_WIN32) && !defined(VM_WIN_UWP)
   OSVERSIONINFOW osvi = {sizeof(OSVERSIONINFOW)};

   /*
    * Starting with msvc-12.0 / SDK v8.1 GetVersionEx is deprecated.
    * Bug 1259185 tracks switching to VerifyVersionInfo.
    */

#pragma warning(suppress : 4996) // 'function': was declared deprecated
   GetVersionExW(&osvi);

   /* Windows version is at least Vista or higher */
   return osvi.dwMajorVersion >= 6;
#else
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketBind --
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

static Bool
AsyncTCPSocketBind(AsyncTCPSocket *asock,          // IN
                   struct sockaddr_storage *addr,  // IN
                   socklen_t addrLen,              // IN
                   int *outError)                  // OUT
{
   int error = ASOCKERR_BIND;
   int sysErr;
   unsigned int port;

   ASSERT(NULL != asock);
   ASSERT(NULL != asock->sslSock);
   ASSERT(NULL != addr);

   port = AsyncTCPSocketGetPortFromAddr(addr);
   TCPSOCKLG0(asock, ("creating new listening socket on port %d\n", port));

#ifndef _WIN32
   /*
    * Don't ever use SO_REUSEADDR on Windows; it doesn't mean what you think
    * it means.
    */

   if (addr->ss_family == AF_INET || addr->ss_family == AF_INET6) {
      int reuse = port != 0;

      if (setsockopt(asock->fd, SOL_SOCKET, SO_REUSEADDR,
                     (const void *) &reuse, sizeof(reuse)) != 0) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "could not set SO_REUSEADDR, error %d: %s\n",
                 sysErr, Err_Errno2String(sysErr));
      }
   }
#else
   /*
    * Always set SO_EXCLUSIVEADDRUSE on Windows, to prevent other applications
    * from stealing this socket. (Yes, Windows is that stupid).
    */

   {
      int exclusive = 1;

      if (setsockopt(asock->fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     (const void *) &exclusive, sizeof(exclusive)) != 0) {
         sysErr = ASOCK_LASTERROR();
         Warning(ASOCKPREFIX "could not set SO_EXCLUSIVEADDRUSE, error %d: "
                 "%s\n", sysErr, Err_Errno2String(sysErr));
      }
   }
#endif

#if defined(IPV6_V6ONLY)
   /*
    * WINDOWS: By default V4MAPPED was not supported until Windows Vista.
    * IPV6_V6ONLY was disabled by default until Windows 7. So if we are binding
    * to a AF_INET6 socket and IPV6_V6ONLY existed, we need to turn it on no
    * matter what the setting is to disable V4 mapping.
    *
    * MAC OSX: Support for IPV6_V6ONLY can be found in 10.5+.
    *
    * LINUX: IPV6_V6ONLY was released after V4MAPPED was implemented. There is
    * no way to turn V4MAPPED off on those systems. The default behavior
    * differs from distro-to-distro so attempt to turn V4MAPPED off on all
    * systems that have IPV6_V6ONLY define. There is no good solution for the
    * case where we cannot enable IPV6_V6ONLY, if we error in this case and do
    * not have a IPv4 option then we render the application useless.
    * See AsyncTCPSocketAcceptInternal for the IN6_IS_ADDR_V4MAPPED validation
    * for incomming addresses to close this loophole.
    */

   if (addr->ss_family == AF_INET6 && AsyncTCPSocketOSVersionSupportsV4Mapped()) {
      int on = 1;

      if (setsockopt(asock->fd, IPPROTO_IPV6, IPV6_V6ONLY,
                     (const void *) &on, sizeof(on)) != 0) {
         Warning(ASOCKPREFIX "Cannot set IPV6_V6ONLY socket option.\n");
      }
   }
#else
#error No compiler definition for IPV6_V6ONLY
#endif

   /*
    * Bind to a port
    */

   if (bind(asock->fd, (struct sockaddr *)addr, addrLen) != 0) {
      sysErr = ASOCK_LASTERROR();
      if (sysErr == ASOCK_EADDRINUSE) {
         error = ASOCKERR_BINDADDRINUSE;
      }
      Warning(ASOCKPREFIX "Could not bind socket, error %d: %s\n", sysErr,
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
 * AsyncTCPSocketListen --
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

static Bool
AsyncTCPSocketListen(AsyncTCPSocket *asock,             // IN
                     AsyncSocketConnectFn connectFn,    // IN
                     void *clientData,                  // IN
                     int *outError)                     // OUT
{
   VMwareStatus pollStatus;
   int error;

   ASSERT(NULL != asock);
   ASSERT(NULL != asock->sslSock);

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

   AsyncTCPSocketLock(asock);
   pollStatus = AsyncTCPSocketPollAdd(asock, TRUE,
                                   POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                   AsyncTCPSocketAcceptCallback);

   if (pollStatus != VMWARE_STATUS_SUCCESS) {
      TCPSOCKWARN(asock,
                ("could not register accept callback!\n"));
      error = ASOCKERR_POLL;
      AsyncTCPSocketUnlock(asock);
      goto error;
   }
   AsyncTCPSocketSetState(asock, AsyncSocketListening);

   asock->connectFn = connectFn;
   asock->clientData = clientData;
   AsyncTCPSocketUnlock(asock);

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
 * AsyncTCPSocketConnectImpl --
 *
 *      AsyncTCPSocket AF_INET/AF_INET6 connect.
 *
 *      NOTE: This function can block.
 *
 * Results:
 *      AsyncTCPSocket * on success and NULL on failure.
 *      On failure, error is returned in *outError.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketConnectImpl(int socketFamily,                  // IN
                          const char *hostname,              // IN
                          unsigned int port,                 // IN
                          int tcpSocketFd,                   // IN
                          AsyncSocketConnectFn connectFn,    // IN
                          void *clientData,                  // IN
                          AsyncSocketConnectFlags flags,     // IN
                          AsyncSocketPollParams *pollParams, // IN
                          int *outError)                     // OUT: optional
{
   struct sockaddr_storage addr;
   int getaddrinfoError;
   int error;
   AsyncTCPSocket *asock;
   char *ipString = NULL;
   socklen_t addrLen;

   /*
    * Resolve the hostname.  Handles dotted decimal strings, too.
    */

   getaddrinfoError = AsyncTCPSocketResolveAddr(hostname, port, socketFamily,
                                                FALSE, &addr, &addrLen,
                                                &ipString);
   if (0 != getaddrinfoError) {
      Log(ASOCKPREFIX "Failed to resolve %s address '%s' and port %u\n",
          socketFamily == AF_INET ? "IPv4" : "IPv6", hostname, port);
      error = ASOCKERR_ADDRUNRESV;
      goto error;
   }

   Log(ASOCKPREFIX "creating new %s socket, connecting to %s (%s)\n",
       socketFamily == AF_INET ? "IPv4" : "IPv6", ipString, hostname);
   free(ipString);

   asock = AsyncTCPSocketConnect(&addr, addrLen, tcpSocketFd,
                                 connectFn, clientData,
                                 flags, pollParams, &error);
   if (!asock) {
      Warning(ASOCKPREFIX "%s connection attempt failed: %s\n",
              socketFamily == AF_INET ? "IPv4" : "IPv6",
              AsyncSocket_MsgError(error));
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
 * AsyncTCPSocket_Connect --
 *
 *      AsyncTCPSocket connect. Connection is attempted with AF_INET socket
 *      family, when that fails AF_INET6 is attempted.
 *
 *      NOTE: This function can block.
 *
 * Results:
 *      AsyncTCPSocket * on success and NULL on failure.
 *      On failure, error is returned in *outError.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_Connect(const char *hostname,                // IN
                    unsigned int port,                   // IN
                    AsyncSocketConnectFn connectFn,      // IN
                    void *clientData,                    // IN
                    AsyncSocketConnectFlags flags,       // IN
                    AsyncSocketPollParams *pollParams,   // IN
                    int *outError)                       // OUT: optional
{
   return AsyncSocket_ConnectWithFd(hostname,
                                    port,
                                    -1,
                                    connectFn,
                                    clientData,
                                    flags,
                                    pollParams,
                                    outError);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectWithFd --
 *
 *      AsyncTCPSocket connect using an existing socket descriptor.
 *      Connection is attempted with AF_INET socket
 *      family, when that fails AF_INET6 is attempted.
 *
 *      Limitation: The ConnectWithFd functionality is currently Windows only.
 *                  Non-Windows platforms & windows-UWP are not supported.
 *
 * Results:
 *      AsyncTCPSocket * on success and NULL on failure.
 *      On failure, error is returned in *outError.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ConnectWithFd(const char *hostname,                // IN
                          unsigned int port,                   // IN
                          int tcpSocketFd,                     // IN
                          AsyncSocketConnectFn connectFn,      // IN
                          void *clientData,                    // IN
                          AsyncSocketConnectFlags flags,       // IN
                          AsyncSocketPollParams *pollParams,   // IN
                          int *outError)                       // OUT: optional
{
   int error = ASOCKERR_CONNECT;
   AsyncTCPSocket *asock = NULL;

   if (!connectFn || !hostname) {
      error = ASOCKERR_INVAL;
      Warning(ASOCKPREFIX "invalid arguments to connect!\n");
      goto error;
   }

   asock = AsyncTCPSocketConnectImpl(AF_INET, hostname, port,
                                     tcpSocketFd, connectFn, clientData,
                                     flags, pollParams, &error);
   if (!asock) {
      asock = AsyncTCPSocketConnectImpl(AF_INET6, hostname, port,
                                        tcpSocketFd, connectFn, clientData,
                                        flags, pollParams, &error);
   }

error:
   if (!asock && outError) {
      *outError = error;
   }

   return BaseSocket(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocket_ConnectVMCI --
 *
 *      AsyncTCPSocket AF_VMCI constructor. Connects to the specified cid:port,
 *      and passes the caller a valid asock via the callback once the
 *      connection has been established.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
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
                        int *outError)                     // OUT: optional
{
   int vsockDev = -1;
   struct sockaddr_vm addr;
   AsyncTCPSocket *asock;

   memset(&addr, 0, sizeof addr);
   addr.svm_family = VMCISock_GetAFValueFd(&vsockDev);
   addr.svm_cid = cid;
   addr.svm_port = port;

   Log(ASOCKPREFIX "creating new socket, connecting to %u:%u\n", cid, port);

   asock = AsyncTCPSocketConnect((struct sockaddr_storage *)&addr,
                                 sizeof addr, -1, connectFn, clientData,
                                 flags, pollParams, outError);

   VMCISock_ReleaseAFValueFd(vsockDev);
   return BaseSocket(asock);
}


#ifndef _WIN32
/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_ConnectUnixDomain --
 *
 *      AsyncTCPSocket AF_UNIX constructor. Connects to the specified unix socket,
 *      and passes the caller a valid asock via the callback once the
 *      connection has been established.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
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
   AsyncTCPSocket *asock;

   memset(&addr, 0, sizeof addr);
   addr.sun_family = AF_UNIX;

   if (strlen(path) + 1 > sizeof addr.sun_path) {
      Warning(ASOCKPREFIX "Path '%s' is too long for a unix domain socket!\n", path);
      return NULL;
   }
   Str_Strcpy(addr.sun_path, path, sizeof addr.sun_path);

   Log(ASOCKPREFIX "creating new socket, connecting to %s\n", path);

   asock = AsyncTCPSocketConnect((struct sockaddr_storage *)&addr,
                              sizeof addr, -1, connectFn, clientData,
                              flags, pollParams, outError);

   return BaseSocket(asock);
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketConnectErrorCheck --
 *
 *      Check for error on a connecting socket and fire the connect callback
 *      is any error is found.  This is only used on Windows.
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
AsyncTCPSocketConnectErrorCheck(void *data)  // IN: AsyncTCPSocket *
{
   AsyncTCPSocket *asock = data;
   Bool removed;
   PollerFunction func = NULL;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) == AsyncSocketConnecting) {
      int sockErr = 0;
      int sockErrLen = sizeof sockErr;

      if (getsockopt(asock->fd, SOL_SOCKET, SO_ERROR, (void *)&sockErr,
                     (void *)&sockErrLen) == 0) {
         if (sockErr == 0) {
            /* There is no error; keep waiting. */
            return;
         }
         asock->genericErrno = sockErr;
      } else {
         asock->genericErrno = ASOCK_LASTERROR();
      }
      TCPSOCKLG0(asock, ("Connection failed: %s\n",
                       Err_Errno2String(asock->genericErrno)));
      /* Remove connect callback. */
      removed = AsyncTCPSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                      asock->internalConnectFn);
      ASSERT(removed);
      func = asock->internalConnectFn;
   }

   /* Remove this callback. */
   removed = AsyncTCPSocketPollRemove(asock, FALSE, POLL_FLAG_PERIODIC,
                                   AsyncTCPSocketConnectErrorCheck);
   ASSERT(removed);
   asock->internalConnectFn = NULL;

   if (func) {
      func(asock);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * SocketProtocolAndTypeMatches --
 *
 *      Discover whether a given socket has the specified protocol family
 *      (PF_INET, PF_INET6, ...) and data transfer type (SOCK_STREAM,
 *      SOCK_DGRAM, ...).
 *
 *      For now, this is supported only on non-UWP Windows platforms.
 *      Other platforms always receive a FALSE result.
 *
 * Results:
 *      True if the socket has the specified family and type, false
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
SocketProtocolAndTypeMatches(int socketFd,   // IN
                             int protocol,   // IN
                             int type)       // IN
{
#if defined( _WIN32) && !defined(VM_WIN_UWP)
   int ret;
   WSAPROTOCOL_INFO protocolInfo;
   int protocolInfoLen = sizeof protocolInfo;

   ret = getsockopt(socketFd, SOL_SOCKET, SO_PROTOCOL_INFO,
                    (void*)&protocolInfo, &protocolInfoLen);
   if (ret != 0) {
      Warning(ASOCKPREFIX "SO_PROTOCOL_INFO failed on sockFd %d, ",
              "error 0x%x\n",
              socketFd, ASOCK_LASTERROR());
      return FALSE;
   }

   /*
    * Windows is confused about protocol families (the "domain" of the
    * socket, passed as the first argument to the socket() call) and
    * address families (specified in the xx_family member of a sockaddr_xx
    * argument passed to bind()).  The protocol family of the socket is
    * reported in the iAddressFamily of the WSAPROTOCOL_INFO structure.
    */
   return ((protocol == protocolInfo.iAddressFamily) &&
           (type == protocolInfo.iSocketType));
#else

   /*
    * If we need to implement this for other platforms then we can use
    * getsockopt(SO_TYPE) to retrieve the socket type, and on Linux we can
    * use getsockopt(SO_DOMAIN) to retrieve the protocol family, but other
    * platforms might not have SO_DOMAIN.  On those platforms we might be
    * able to infer the protocol family by attempting sockopt calls that
    * only work on certain families.
    *
    * BTW, Linux has thrown in the towel on the distinction between
    * protocol families and address families.  Its socket() man page shows
    * AF_* literals being used for the 'domain' argument instead of PF_*
    * literals.  This works because AF_XX is defined to have the same
    * numeric value as PF_XX for all values of XX.
    */
   Warning(ASOCKPREFIX "discovery of socket protocol and type is "
           "not implemented on this platform\n");
   NOT_IMPLEMENTED();
#endif // defined(_WIN32)
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketConnect --
 *
 *      Internal AsyncTCPSocket constructor.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Allocates an AsyncTCPSocket, registers a poll callback.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketConnect(struct sockaddr_storage *addr,         // IN
                      socklen_t addrLen,                     // IN
                      int socketFd,                          // IN: optional
                      AsyncSocketConnectFn connectFn,        // IN
                      void *clientData,                      // IN
                      AsyncSocketConnectFlags flags,         // IN
                      AsyncSocketPollParams *pollParams,     // IN
                      int *outError)                         // OUT
{
   int fd;
   VMwareStatus pollStatus;
   AsyncTCPSocket *asock = NULL;
   int error = ASOCKERR_GENERIC;
   int sysErr;

   ASSERT(addr);

   if (!connectFn) {
      error = ASOCKERR_INVAL;
      Warning(ASOCKPREFIX "invalid arguments to connect!\n");
      goto error;
   }

   /*
    * If we were given a socket, verify that it is of the required
    * protocol family and type before using it.  If no socket was given,
    * create a new socket of the appropriate family.  (For the sockets
    * we care about, the required protocol family is numerically the
    * same as the address family provided in the given destination
    * sockaddr, so we can use addr->ss_family whenever we need to
    * specify a protocol family.)
    *
    * For now, passing in a socket is supported only on non-UWP Windows
    * platforms.  The SocketProtocolAndTypeMatches() call will fail on
    * other platforms.
    */
   if (-1 != socketFd) {
      int protocolFamily = addr->ss_family;
      // XXX Logging here is excessive, remove after testing
      if (SocketProtocolAndTypeMatches(socketFd, protocolFamily,
                                       SOCK_STREAM)) {
         Warning(ASOCKPREFIX "using passed-in socket, family %d\n",
                 protocolFamily);
         fd = socketFd;
      } else {
         Warning(ASOCKPREFIX "rejecting passed-in socket, wanted family %d\n",
                 protocolFamily);
         error = ASOCKERR_INVAL;
         goto error;
      }
   } else if ((fd = socket(addr->ss_family, SOCK_STREAM, 0)) == -1) {
      sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "failed to create socket, error %d: %s\n",
              sysErr, Err_Errno2String(sysErr));
      error = ASOCKERR_CONNECT;
      goto error;
   }

   /*
    * Wrap it with an asock
    */

   if ((asock = AsyncTCPSocketAttachToFd(fd, pollParams, &error)) == NULL) {
      SSLGeneric_close(fd);
      goto error;
   }


   /*
    * Call connect(), which can either succeed immediately or return an error
    * indicating that the connection is in progress. In the latter case, we
    * can poll the fd for write to find out when the connection attempt
    * has succeeded (or failed). In either case, we want to invoke the
    * caller's connect callback from Poll rather than directly, so if the
    * connection succeeds immediately, we just schedule the connect callback
    * as a one-time (RTime) callback instead.
    */

   AsyncTCPSocketLock(asock);
   if (connect(asock->fd, (struct sockaddr *)addr, addrLen) != 0) {
      if (ASOCK_LASTERROR() == ASOCK_ECONNECTING) {
         ASSERT(!(vmx86_server && addr->ss_family == AF_UNIX));
         TCPSOCKLOG(1, asock,
                    ("registering write callback for socket connect\n"));
         pollStatus = AsyncTCPSocketPollAdd(asock, TRUE, POLL_FLAG_WRITE,
                                            AsyncTCPSocketConnectCallback);
         if (vmx86_win32 && pollStatus == VMWARE_STATUS_SUCCESS &&
             AsyncTCPSocketPollParams(asock)->iPoll == NULL) {
            /*
             * Work around WSAPoll's bug of not reporting failed connection
             * by periodically (500 ms) checking for error.
             */
            pollStatus = AsyncTCPSocketPollAdd(asock, FALSE, POLL_FLAG_PERIODIC,
                                               AsyncTCPSocketConnectErrorCheck,
                                               500 * 1000);
            if (pollStatus == VMWARE_STATUS_SUCCESS) {
               asock->internalConnectFn = AsyncTCPSocketConnectCallback;
            } else {
               TCPSOCKLG0(asock, ("failed to register periodic error check\n"));
               AsyncTCPSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                        AsyncTCPSocketConnectCallback);
            }
         }
      } else {
         sysErr = ASOCK_LASTERROR();
         Log(ASOCKPREFIX "connect failed, error %d: %s\n",
             sysErr, Err_Errno2String(sysErr));

         /*
          * If "network unreachable" error happens, explicitly propogate
          * the error to trigger the reconnection if possible.
          */
         error = (sysErr == ASOCK_ENETUNREACH) ? ASOCKERR_NETUNREACH :
                                                 ASOCKERR_CONNECT;
         goto errorHaveAsock;
      }
   } else {
      TCPSOCKLOG(2, asock,
               ("socket connected, registering RTime callback for connect\n"));
      pollStatus = AsyncTCPSocketPollAdd(asock, FALSE, 0,
                                         AsyncTCPSocketConnectCallback, 0);
   }

   if (pollStatus != VMWARE_STATUS_SUCCESS) {
      TCPSOCKWARN(asock, ("failed to register callback in connect!\n"));
      error = ASOCKERR_POLL;
      goto errorHaveAsock;
   }

   AsyncTCPSocketSetState(asock, AsyncSocketConnecting);
   asock->connectFn = connectFn;
   asock->clientData = clientData;

   /* Store a copy of the sockaddr_storage so we can look it up later. */
   memcpy(&(asock->remoteAddr), addr, addrLen);
   asock->remoteAddrLen = addrLen;

   AsyncTCPSocketUnlock(asock);

   return asock;

errorHaveAsock:
   SSL_Shutdown(asock->sslSock);
   AsyncTCPSocketUnlock(asock);
   free(asock);

error:
   if (outError) {
      *outError = error;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketCreate --
 *
 *      AsyncSocket constructor for fields common to all TCP-based
 *      AsyncSocket types.
 *
 * Results:
 *      New AsyncSocket object.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketCreate(AsyncSocketPollParams *pollParams) // IN
{
   AsyncTCPSocket *s;

   s = Util_SafeCalloc(1, sizeof *s);

   AsyncSocketInitSocket(BaseSocket(s), pollParams, &asyncTCPSocketVTable);

   s->fd = -1;
   s->inRecvLoop = FALSE;
   s->sendBufFull = FALSE;
   s->sendBufTail = &(s->sendBufList);
   s->passFd.fd = -1;

   if (pollParams && pollParams->iPoll) {
      s->internalSendFn = AsyncTCPSocketIPollSendCallback;
      s->internalRecvFn = AsyncTCPSocketIPollRecvCallback;
   } else {
      s->internalSendFn = AsyncTCPSocketSendCallback;
      s->internalRecvFn = AsyncTCPSocketRecvCallback;
   }

   return s;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketAttachToSSLSock --
 *
 *      AsyncTCPSocket constructor. Wraps an existing SSLSock object with an
 *      AsyncTCPSocket and returns the latter.
 *
 * Results:
 *      New AsyncTCPSocket object or NULL on error.
 *
 * Side effects:
 *      Allocates memory, makes the underlying fd for the socket non-blocking.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketAttachToSSLSock(SSLSock sslSock,                   // IN
                              AsyncSocketPollParams *pollParams, // IN
                              int *outError)                     // OUT
{
   AsyncTCPSocket *s;
   int fd;
   int error;

   ASSERT(sslSock);

   fd = SSL_GetFd(sslSock);

   if ((AsyncTCPSocketMakeNonBlocking(fd)) != ASOCKERR_SUCCESS) {
      int sysErr = ASOCK_LASTERROR();
      Warning(ASOCKPREFIX "failed to make fd %d non-blocking!: %d, %s\n",
              fd, sysErr, Err_Errno2String(sysErr));
      error = ASOCKERR_GENERIC;
      goto error;
   }

   s = AsyncTCPSocketCreate(pollParams);
   AsyncTCPSocketSetState(s, AsyncSocketConnected);
   s->sslSock = sslSock;
   s->fd = fd;

   /* From now on socket is ours. */
   SSL_SetCloseOnShutdownFlag(sslSock);
   TCPSOCKLOG(1, s, ("new asock id %u attached to fd %d\n", s->base.id, s->fd));

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
 * AsyncTCPSocketAttachToFd --
 *
 *      AsyncTCPSocket constructor. Wraps a valid socket fd with an
 *      AsyncTCPSocket object.
 *
 * Results:
 *      New AsyncTCPSocket or NULL on error.
 *
 * Side effects:
 *      If function succeeds, fd is owned by AsyncTCPSocket and should not be
 *      used (f.e. closed) anymore.
 *
 *----------------------------------------------------------------------------
 */

static AsyncTCPSocket *
AsyncTCPSocketAttachToFd(int fd,                             // IN
                         AsyncSocketPollParams *pollParams,  // IN
                         int *outError)                      // OUT
{
   SSLSock sslSock;
   AsyncTCPSocket *asock;

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
   asock = AsyncTCPSocketAttachToSSLSock(sslSock, pollParams, outError);
   if (asock) {
      return asock;
   }
   SSL_Shutdown(sslSock);

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_AttachToFd --
 *
 *      Wrap a pre-existing file descriptor in an AsyncSocket entity.
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
AsyncSocket_AttachToFd(int fd,                            // IN
                       AsyncSocketPollParams *pollParams, // IN
                       int *outError)                     // OUT
{
   AsyncTCPSocket *asock;
   asock = AsyncTCPSocketAttachToFd(fd, pollParams, outError);
   return BaseSocket(asock);
}

/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_AttachToSSLSock --
 *
 *      Wrap a pre-existing SSLSock in an AsyncSocket entity.
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
AsyncSocket_AttachToSSLSock(SSLSock sslSock,                   // IN
                            AsyncSocketPollParams *pollParams, // IN
                            int *outError)                     // OUT
{
   AsyncTCPSocket *asock;
   asock = AsyncTCPSocketAttachToSSLSock(sslSock, pollParams, outError);
   return BaseSocket(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketRegisterRecvCb --
 *
 *      Register poll callbacks as required to be notified when data is ready
 *      following a AsyncTCPSocket_Recv call.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Could register poll callback.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketRegisterRecvCb(AsyncTCPSocket *asock) // IN:
{
   int retVal = ASOCKERR_SUCCESS;

   if (!asock->recvCb) {
      VMwareStatus pollStatus;

      /*
       * Register the Poll callback
       */

      TCPSOCKLOG(3, asock, ("installing recv periodic poll callback\n"));

      pollStatus = AsyncTCPSocketPollAdd(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      asock->internalRecvFn);

      if (pollStatus != VMWARE_STATUS_SUCCESS) {
         TCPSOCKWARN(asock, ("failed to install recv callback!\n"));
         retVal = ASOCKERR_POLL;
         goto out;
      }
      asock->recvCb = TRUE;
   }

   if (AsyncTCPSocketHasDataPending(asock) && !asock->inRecvLoop) {
      TCPSOCKLOG(0, asock, ("installing recv RTime poll callback\n"));
      if (AsyncTCPSocketPollAdd(asock, FALSE, 0, asock->internalRecvFn, 0) !=
          VMWARE_STATUS_SUCCESS) {
         retVal = ASOCKERR_POLL;
         goto out;
      }
      asock->recvCbTimer = TRUE;
   }

out:
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocket_Recv --
 *
 *      Registers a callback that will fire once the specified amount of data
 *      has been received on the socket.
 *
 *      In the case of AsyncTCPSocket_RecvPartial, the callback is fired
 *      once all or part of the data has been received on the socket.
 *
 *      Data that was not retrieved at the last call of SSL_read() could still
 *      be buffered inside the SSL layer and will be retrieved on the next
 *      call to SSL_read(). However poll/select might not mark the socket as
 *      for reading since there might not be any data in the underlying network
 *      socket layer. Hence in the read callback, we keep spinning until all
 *      all the data buffered inside the SSL layer is retrieved before
 *      returning to the poll loop (See AsyncTCPSocketFillRecvBuffer()).
 *
 *      However, we might not have come out of Poll in the first place, e.g.
 *      if this is the first call to AsyncTCPSocket_Recv() after creating a new
 *      connection. In this situation, if there is buffered SSL data pending,
 *      we have to schedule an RTTime callback to force retrieval of the data.
 *      This could also happen if the client calls AsyncTCPSocket_RecvBlocking,
 *      some data is left in the SSL layer, and the client then calls
 *      AsyncTCPSocket_Recv. We use the inRecvLoop variable to detect and handle
 *      this condition, i.e., if inRecvLoop is FALSE, we need to schedule the
 *      RTime callback.
 *
 *      TCP usage:
 *      AsyncTCPSocket_Recv(AsyncTCPSocket *asock,
 *                       void *buf,
 *                       int len,
 *                       AsyncSocketRecvFn recvFn,
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

static int
AsyncTCPSocketRecv(AsyncSocket *base,   // IN:
                   void *buf,           // IN: unused
                   int len,             // IN: unused
                   Bool fireOnPartial,  // IN:
                   void *cb,            // IN:
                   void *cbData)        // IN:
{
   AsyncTCPSocket *asock = TCPSocket(base);
   int retVal;

   if (!asock->base.errorFn) {
      TCPSOCKWARN(asock, ("%s: no registered error handler!\n", __FUNCTION__));
      return ASOCKERR_INVAL;
   }

   /*
    * XXX We might want to allow passing NULL for the recvFn, to indicate that
    *     the client is no longer interested in reading from the socket. This
    *     would be useful e.g. for HTTP, where the client sends a request and
    *     then the client->server half of the connection is closed.
    */

   if (!buf || !cb || len <= 0) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");
      return ASOCKERR_INVAL;
   }

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) != AsyncSocketConnected) {
      TCPSOCKWARN(asock, ("recv called but state is not connected!\n"));
      return ASOCKERR_NOTCONNECTED;
   }

   if (asock->inBlockingRecv && !asock->inRecvLoop) {
      TCPSOCKWARN(asock, ("Recv called while a blocking recv is pending.\n"));
      return ASOCKERR_INVAL;
   }

   retVal = AsyncTCPSocketRegisterRecvCb(asock);
   if (retVal != ASOCKERR_SUCCESS) {
      return retVal;
   }

   AsyncSocketSetRecvBuf(BaseSocket(asock), buf, len, fireOnPartial,
                         cb, cbData);
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketRecvPassedFd --
 *
 *      See AsyncTCPSocket_Recv.  Besides that it allows for receiving one
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

static int
AsyncTCPSocketRecvPassedFd(AsyncSocket *base,   // IN/OUT: socket
                           void *buf,           // OUT: buffer with data
                           int len,             // IN: length
                           void *cb,            // IN: completion calback
                           void *cbData)        // IN: callback's data
{
   AsyncTCPSocket *asock = TCPSocket(base);
   int err;

   if (!asock->base.errorFn) {
      TCPSOCKWARN(asock, ("%s: no registered error handler!\n", __FUNCTION__));

      return ASOCKERR_INVAL;
   }

   ASSERT(AsyncTCPSocketIsLocked(asock));
   if (asock->passFd.fd != -1) {
      SSLGeneric_close(asock->passFd.fd);
      asock->passFd.fd = -1;
   }
   asock->passFd.expected = TRUE;

   err = AsyncTCPSocketRecv(BaseSocket(asock), buf, len, FALSE, cb, cbData);
   if (err != ASOCKERR_SUCCESS) {
      asock->passFd.expected = FALSE;
   }

   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketPollWork --
 *
 *      Blocks on the specified sockets until there's data pending or a
 *      timeout occurs.
 *
 *      If the asyncsocket is a dual stack listener, parentSock will not be
 *      NULL, and the asock array will contain the IPv4 and v6 sockets.
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
AsyncTCPSocketPollWork(AsyncTCPSocket **asock,     // IN:
                       int numSock,                // IN:
                       void *p,                    // IN:
                       Bool read,                  // IN:
                       int timeoutMS,              // IN:
                       AsyncTCPSocket *parentSock, // IN:
                       AsyncTCPSocket **outAsock)  // OUT:
{
   AsyncTCPSocket *warnSock = parentSock ? parentSock : asock[0];
#ifndef _WIN32
   struct pollfd *pfd = (struct pollfd *)p;
#else
   /*
    * We use select() to do this on Windows, since there ain't no poll().
    * Fortunately, select() doesn't have the 1024 fd value limit.
    */

   struct timeval tv;
   struct fd_set rwfds;
   struct fd_set exceptfds;
#endif
   int i;
   int retval;

   ASSERT(outAsock != NULL && *outAsock == NULL && asock != NULL &&
          numSock > 0);

   for (i = 0; i < numSock; i++) {
      if (read && SSL_Pending(asock[i]->sslSock)) {
         *outAsock = asock[i];
         return ASOCKERR_SUCCESS;
      }
   }

   while (1) {
#ifndef _WIN32
      for (i = 0; i < numSock; i++) {
         pfd[i].fd = asock[i]->fd;
         pfd[i].events = read ? POLLIN : POLLOUT;
      }

      if (parentSock != NULL) {
         AsyncTCPSocketUnlock(parentSock);
         retval = poll(pfd, numSock, timeoutMS);
         AsyncTCPSocketLock(parentSock);
      } else {
         for (i = numSock - 1; i >= 0; i--) {
            AsyncTCPSocketUnlock(asock[i]);
         }
         retval = poll(pfd, numSock, timeoutMS);
         for (i = 0; i < numSock; i++) {
            AsyncTCPSocketLock(asock[i]);
         }
      }
#else
      tv.tv_sec = timeoutMS / 1000;
      tv.tv_usec = (timeoutMS % 1000) * 1000;

      FD_ZERO(&rwfds);
      FD_ZERO(&exceptfds);

      for (i = 0; i < numSock; i++) {
         FD_SET(asock[i]->fd, &rwfds);
         FD_SET(asock[i]->fd, &exceptfds);
      }

      if (parentSock != NULL) {
         AsyncTCPSocketUnlock(parentSock);
         retval = select(1, read ? &rwfds : NULL, read ? NULL : &rwfds,
                         &exceptfds, timeoutMS >= 0 ? &tv : NULL);
         AsyncTCPSocketLock(parentSock);
      } else {
         for (i = numSock - 1; i >= 0; i--) {
            AsyncTCPSocketUnlock(asock[i]);
         }
         retval = select(1, read ? &rwfds : NULL, read ? NULL : &rwfds,
                         &exceptfds, timeoutMS >= 0 ? &tv : NULL);
         for (i = 0; i < numSock; i++) {
            AsyncTCPSocketLock(asock[i]);
         }
      }
#endif

      switch (retval) {
      case 0:
         /*
          * No sockets were ready within the specified time.
          */
         TCPSOCKLG0(warnSock, ("%s: Timeout waiting for a ready socket.\n",
                      __FUNCTION__));
         return ASOCKERR_TIMEOUT;

      case -1: {
         int sysErr = ASOCK_LASTERROR();

         if (sysErr == EINTR) {
            /*
             * We were somehow interrupted by signal. Let's loop and retry.
             * XXX: update the timeout by the amount we had previously waited.
             */

            TCPSOCKLG0(warnSock, ("%s: Socket interrupted by a signal.\n",
                         __FUNCTION__));
            continue;
         }

         if (parentSock != NULL) {
            parentSock->genericErrno = sysErr;
         } else {
            for (i = 0; i < numSock; i++) {
               asock[i]->genericErrno = sysErr;
            }
         }

         TCPSOCKLG0(warnSock, ("%s: Failed with error %d: %s\n", __FUNCTION__,
                   sysErr, Err_Errno2String(sysErr)));
         return ASOCKERR_GENERIC;
      }
      default: {
         Bool failed = FALSE;

#ifndef _WIN32
         for (i = 0; i < numSock; i++) {
            if (pfd[i].revents & (POLLERR | POLLNVAL)) {
               failed = TRUE;
            }
         }
#else
         for (i = 0; i < numSock; i++) {
            if (FD_ISSET(asock[i]->fd, &exceptfds)) {
               failed = TRUE;
            }
         }
#endif

         if (failed) {
            int sockErr = 0;
            int sysErr;
            int sockErrLen = sizeof sockErr;

            for (i = 0; i < numSock; i++) {
               if (getsockopt(asock[i]->fd, SOL_SOCKET, SO_ERROR,
                              (void *) &sockErr, (void *) &sockErrLen) == 0) {
                  if (sockErr) {
                     asock[i]->genericErrno = sockErr;
                     TCPSOCKLG0(asock[i],
                              ("%s: Socket error lookup returned %d: %s\n",
                               __FUNCTION__, sockErr,
                               Err_Errno2String(sockErr)));
                  }
               } else {
                  sysErr = ASOCK_LASTERROR();
                  asock[i]->genericErrno = sysErr;
                  TCPSOCKLG0(asock[i],
                           ("%s: Last socket error %d: %s\n",
                            __FUNCTION__, sysErr, Err_Errno2String(sysErr)));
               }
            }

            return ASOCKERR_GENERIC;
         }

         /*
          * If one socket is ready, and it wasn't in an exception state,
          * everything is ok. The socket is ready for reading/writing.
          */

#ifndef _WIN32
         for (i = 0; i < numSock; i++) {
            if (pfd[i].revents & (read ? POLLIN : POLLOUT)) {
               *outAsock = asock[i];
               return ASOCKERR_SUCCESS;
            }
         }
#else
         for (i = 0; i < numSock; i++) {
            if (FD_ISSET(asock[i]->fd, &rwfds)) {
               *outAsock = asock[i];
               return ASOCKERR_SUCCESS;
            }
         }
#endif

         TCPSOCKWARN(warnSock, ("%s: Failed to return a ready socket.\n",
                         __FUNCTION__));
         return ASOCKERR_GENERIC;
      }
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketPoll --
 *
 *      Blocks on the specified socket until there's data pending or a
 *      timeout occurs.
 *
 *      If the specified socket is a dual stack listener, we will poll on all
 *      listening sockets and will return when one is ready with data for a
 *      connection. If both socket families happen to race with connect data,
 *      we will favor IPv6 for the return.
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
AsyncTCPSocketPoll(AsyncTCPSocket *s,          // IN:
                   Bool read,                  // IN:
                   int timeoutMS,              // IN:
                   AsyncTCPSocket **outAsock)  // OUT:
{
   AsyncTCPSocket *asock[2];
#ifndef _WIN32
   struct pollfd p[2];
#else
   void *p = NULL;
#endif
   int numSock = 0;

   if (read && s->fd == -1) {
      if (!s->listenAsock4 && !s->listenAsock6) {
         TCPSOCKLG0(s, ("%s: Failed to find listener socket.\n", __FUNCTION__));
         return ASOCKERR_GENERIC;
      }

      if (s->listenAsock6 && s->listenAsock6->fd != -1) {
         asock[numSock++] = s->listenAsock6;
      }
      if (s->listenAsock4 && s->listenAsock4->fd != -1) {
         asock[numSock++] = s->listenAsock4;
      }
   } else {
      asock[numSock++] = s;
   }

   return AsyncTCPSocketPollWork(asock, numSock, p, read, timeoutMS, s,
                                 outAsock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketWaitForReadMultiple --
 *
 *      Blocks on the list of sockets until there's data readable or a
 *      timeout occurs.
 *
 *      Please see the comment in asyncSocketInterface.c for more
 *      information about using this function.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on system call
 *        failures
 *      ASOCKERR_TIMEOUT if no sockets were ready with readable data.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketWaitForReadMultiple(AsyncSocket **asock,   // IN:
                                  int numSock,           // IN:
                                  int timeoutMS,         // IN:
                                  int *outIdx)           // OUT:
{
   int i;
   int err;
   AsyncTCPSocket *outAsock  = NULL;
#ifndef _WIN32
   struct pollfd *p          = Util_SafeCalloc(numSock, sizeof *p);
#else
   void *p                   = NULL;
#endif

   for (i = 0; i < numSock; i++) {
      ASSERT(AsyncTCPSocketIsLocked(TCPSocket(asock[i])));
   }
   err = AsyncTCPSocketPollWork((AsyncTCPSocket **)asock, numSock, p, TRUE,
                                timeoutMS, NULL, &outAsock);
   for (i = numSock - 1; i >= 0; i--) {
      AsyncTCPSocket *tcpAsock = TCPSocket(asock[i]);
      if (outAsock == tcpAsock) {
         *outIdx = i;
      }
   }

   free(p);
   return err;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketRecvBlocking --
 * AsyncTCPSocketRecvPartialBlocking --
 * AsyncTCPSocketSendBlocking --
 *
 *      Implement "blocking + timeout" operations on the socket. These are
 *      simple wrappers around the AsyncTCPSocketBlockingWork function, which
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

static int
AsyncTCPSocketRecvBlocking(AsyncSocket *base,     // IN
                           void *buf,             // OUT
                           int len,               // IN
                           int *received,         // OUT
                           int timeoutMS)         // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   return AsyncTCPSocketBlockingWork(s, TRUE, buf, len, received, timeoutMS,
                                     FALSE);
}

static int
AsyncTCPSocketRecvPartialBlocking(AsyncSocket *base,     // IN
                                  void *buf,             // OUT
                                  int len,               // IN
                                  int *received,         // OUT
                                  int timeoutMS)         // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   return AsyncTCPSocketBlockingWork(s, TRUE, buf, len, received, timeoutMS,
                                     TRUE);
}

static int
AsyncTCPSocketSendBlocking(AsyncSocket *base,         // IN
                           void *buf,                 // OUT
                           int len,                   // IN
                           int *sent,                 // OUT
                           int timeoutMS)             // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   return AsyncTCPSocketBlockingWork(s, FALSE, buf, len, sent, timeoutMS, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketBlockingWork --
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

static int
AsyncTCPSocketBlockingWork(AsyncTCPSocket *s,  // IN:
                           Bool read,          // IN:
                           void *buf,          // IN/OUT:
                           int len,            // IN:
                           int *completed,     // OUT:
                           int timeoutMS,      // IN:
                           Bool partial)       // IN:
{
   VmTimeType now, done;
   int sysErr;

   if (s == NULL || buf == NULL || len <= 0) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   if (AsyncTCPSocketGetState(s) != AsyncSocketConnected) {
      TCPSOCKWARN(s, ("recv called but state is not connected!\n"));
      return ASOCKERR_NOTCONNECTED;
   }

   if (completed) {
      *completed = 0;
   }
   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;
   do {
      int numBytes, error;
      AsyncTCPSocket *asock = NULL;

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
         TCPSOCKLG0(s, ("blocking %s detected peer closed connection\n",
                        read ? "recv" : "send"));
         return ASOCKERR_REMOTE_DISCONNECT;
      } else if ((sysErr = ASOCK_LASTERROR()) != ASOCK_EWOULDBLOCK) {
         s->genericErrno = sysErr;
         TCPSOCKWARN(s, ("blocking %s error %d: %s\n", read ? "recv" : "send",
                         sysErr, Err_Errno2String(sysErr)));

         return ASOCKERR_GENERIC;
      }

      now = Hostinfo_SystemTimerUS() / 1000;
      if (now >= done && timeoutMS >= 0) {
         return ASOCKERR_TIMEOUT;
      }

      /*
       * Only call in to Poll if we weren't able to send/recv directly
       * off the socket.  But always make sure that the call to Poll()
       * is followed by a read/send.
       */
      error = AsyncTCPSocketPoll(s, read, done - now, &asock);
      if (error != ASOCKERR_SUCCESS) {
         return error;
      }
      ASSERT(asock == s);
   } while (TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSend --
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

static int
AsyncTCPSocketSend(AsyncSocket *base,         // IN
                   void *buf,                 // IN
                   int len,                   // IN
                   AsyncSocketSendFn sendFn,  // IN
                   void *clientData)          // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   int retVal;
   Bool bufferListWasEmpty = FALSE;
   SendBufList **pcur;
   SendBufList *newBuf;

   /*
    * Note: I think it should be fine to send with a length of zero and a
    * buffer of NULL or any other garbage value.  However the code
    * downstream of here is unprepared for it (silently misbehaves).  Hence
    * the <= zero check instead of just a < zero check.  --Jeremy.
    */

   if (!buf || len <= 0) {
      Warning(ASOCKPREFIX "Send called with invalid arguments!"
              "buffer: %p length: %d\n", buf, len);

      return ASOCKERR_INVAL;
   }

   LOG(2, ("%s: sending %d bytes\n", __FUNCTION__, len));

   ASSERT(AsyncTCPSocketIsLocked(asock));

   /*
    * In low-latency mode, we want to guard against recursive calls to
    * Send from within the send callback, as these have the capacity
    * to blow up the stack.  However some operations generate implicit
    * sends (such as Close on a websocket) seem like they should be
    * legal from the send callback.  So, allow a small degree of
    * recursive use of the send callback to accomodate these internal
    * paths.
    */
   ASSERT(asock->inLowLatencySendCb < 2);

   if (AsyncTCPSocketGetState(asock) != AsyncSocketConnected) {
      TCPSOCKWARN(asock, ("send called but state is not connected!\n"));
      return ASOCKERR_NOTCONNECTED;
   }

   /*
    * Allocate and initialize new send buffer entry
    */
   newBuf = Util_SafeCalloc(1, sizeof *newBuf);
   newBuf->buf = buf;
   newBuf->len = len;
   newBuf->sendFn = sendFn;
   newBuf->clientData = clientData;

   /*
    * Append new send buffer to the tail of list.
    */
   *asock->sendBufTail = newBuf;
   asock->sendBufTail = &(newBuf->next);
   bufferListWasEmpty = (asock->sendBufList == newBuf);

   if (bufferListWasEmpty && !asock->sendCb) {
      if (asock->sendLowLatency) {
         /*
          * For low-latency sockets, call the callback directly from
          * this thread.  It is non-blocking and will schedule device
          * callbacks if necessary to complete the operation.
          *
          * Unfortunately we can't make this the default as current
          * consumers of asyncsocket are not expecting the completion
          * callback to be invoked prior to the call to
          * AsyncTCPSocket_Send() returning.
          *
          * Add and release asock reference around the send callback
          * since asock may be closed by a callback invoked during
          * the send workflow.
          */
         AsyncTCPSocketAddRef(asock);
         asock->inLowLatencySendCb++;
         asock->internalSendFn((void *)asock);
         asock->inLowLatencySendCb--;
         AsyncTCPSocketRelease(asock);
      } else {
#ifdef _WIN32
         /*
          * If the send buffer list was empty, we schedule a one-time
          * callback to "prime" the output. This is necessary to
          * support the FD_WRITE network event semantic for sockets on
          * Windows (see WSAEventSelect documentation). The event
          * won't signal unless a previous write() on the socket
          * failed with WSAEWOULDBLOCK, so we have to perform at least
          * one partial write before we can start polling for write.
          *
          * XXX: This can be a device callback once all poll
          * implementations know to get around this Windows quirk.
          * Both PollVMX and PollDefault already make 0-byte send() to
          * force WSAEWOULDBLOCK.
          */
         if (AsyncTCPSocketPollAdd(asock, FALSE, 0, asock->internalSendFn,
                                   AsyncTCPSocketPollParams(asock)->iPoll
                                   != NULL ? 1 : 0)
             != VMWARE_STATUS_SUCCESS) {
            retVal = ASOCKERR_POLL;
            TCPSOCKLOG(1, asock,
                       ("Failed to register poll callback for send\n"));
            goto outUndoAppend;
         }
         asock->sendCbTimer = TRUE;
         asock->sendCb = TRUE;
#else
         if (AsyncTCPSocketPollAdd(asock, TRUE, POLL_FLAG_WRITE,
                                   asock->internalSendFn)
             != VMWARE_STATUS_SUCCESS) {
            retVal = ASOCKERR_POLL;
            TCPSOCKLOG(1, asock,
                       ("Failed to register poll callback for send\n"));
            goto outUndoAppend;
         }
         asock->sendCb = TRUE;
#endif
      }
   }

   return ASOCKERR_SUCCESS;

outUndoAppend:
   /*
    * Remove the appended buffer from the sendBufList. We always append the
    * buffer to the tail of the list.
    */
   pcur = &asock->sendBufList;
   if (*pcur != NULL) {
      if (!bufferListWasEmpty) {
         do {
            pcur = &((*pcur)->next);
         } while ((*pcur)->next != NULL);
      }

      if ((*pcur)->buf == buf) {
         free(*pcur);
         *pcur = NULL;
         asock->sendBufTail = pcur;
      }
   }

   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketResolveAddr --
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

static int
AsyncTCPSocketResolveAddr(const char *hostname,          // IN
                          unsigned int port,             // IN
                          int family,                    // IN
                          Bool passive,                  // IN
                          struct sockaddr_storage *addr, // OUT
                          socklen_t *addrLen,            // OUT
                          char **addrString)             // OUT
{
   struct addrinfo hints;
   struct addrinfo *aiTop = NULL;
   struct addrinfo *aiIterator = NULL;
   int getaddrinfoError = 0;
   char portString[PORT_STRING_LEN];

   ASSERT(NULL != addr);

   if (port > MAX_UINT16) {
      Log(ASOCKPREFIX "port number requested (%d) is out of range.\n", port);
      return EAI_SERVICE;
   }

   Str_Sprintf(portString, sizeof(portString), "%d", port);
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = family;
   hints.ai_socktype = SOCK_STREAM;
   if (passive) {
      hints.ai_flags = AI_PASSIVE;
   }

   getaddrinfoError = Posix_GetAddrInfo(hostname, portString, &hints, &aiTop);
   if (0 != getaddrinfoError) {
      Log(ASOCKPREFIX "getaddrinfo failed for host %s: %s\n", hostname,
                      gai_strerror(getaddrinfoError));
      goto bye;
   }

   for (aiIterator = aiTop; NULL != aiIterator ; aiIterator =
                                                       aiIterator->ai_next) {
      if ((family == AF_UNSPEC && (aiIterator->ai_family == AF_INET ||
                                   aiIterator->ai_family == AF_INET6)) ||
          family == aiIterator->ai_family) {
         if (addrString != NULL) {
            char tempAddrString[ADDR_STRING_LEN];
            static char unknownAddr[] = "(Unknown)";
#if defined(_WIN32)
            DWORD len = ARRAYSIZE(tempAddrString);

            if (WSAAddressToStringA(aiIterator->ai_addr, aiIterator->ai_addrlen,
                                    NULL, tempAddrString, &len)) {
               *addrString = Util_SafeStrdup(unknownAddr);
            } else {
               *addrString = Util_SafeStrdup(tempAddrString);
            }
#else

            if (aiIterator->ai_family == AF_INET &&
                !inet_ntop(aiIterator->ai_family,
                     &(((struct sockaddr_in *)aiIterator->ai_addr)->sin_addr),
                     tempAddrString, INET6_ADDRSTRLEN)) {
               *addrString = Util_SafeStrdup(unknownAddr);
            } else if (aiIterator->ai_family == AF_INET6 &&
                       !inet_ntop(aiIterator->ai_family,
                  &(((struct sockaddr_in6 *)aiIterator->ai_addr)->sin6_addr),
                  tempAddrString, INET6_ADDRSTRLEN)) {
               *addrString = Util_SafeStrdup(unknownAddr);
            } else {
               *addrString = Str_SafeAsprintf(NULL, aiIterator->ai_family ==
                                                    AF_INET6 ? "[%s]:%u" :
                                                               "%s:%u",
                                              tempAddrString, port);
            }
#endif
         }

         memcpy(addr, aiIterator->ai_addr, aiIterator->ai_addrlen);
         *addrLen = aiIterator->ai_addrlen;

         break;
      }
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
 * AsyncTCPSocketFillRecvBuffer --
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

static int
AsyncTCPSocketFillRecvBuffer(AsyncTCPSocket *s)         // IN
{
   int recvd;
   int needed;
   int sysErr = 0;
   int result;
   int pending = 0;

   ASSERT(AsyncTCPSocketIsLocked(s));
   ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);

   /*
    * When a socket has received all its desired content and FillRecvBuffer is
    * called again for the same socket, just return ASOCKERR_SUCCESS. The
    * reason we need this hack is that if a client which registered a receive
    * callback asynchronously later changes its mind to do it synchronously,
    * (e.g. aioMgr wait function), then FillRecvBuffer can be potentially be
    * called twice for the same receive event.
    */

   needed = s->base.recvLen - s->base.recvPos;
   if (!s->base.recvBuf && needed == 0) {
      return ASOCKERR_SUCCESS;
   }

   ASSERT(needed > 0);

   AsyncTCPSocketAddRef(s);

   /*
    * See comment in AsyncTCPSocket_Recv
    */

   s->inRecvLoop = TRUE;

   do {

      /*
       * Try to read the remaining bytes to complete the current recv request.
       */

      if (s->passFd.expected) {
         int fd;

         recvd = SSL_RecvDataAndFd(s->sslSock,
                                   (uint8 *) s->base.recvBuf +
                                   s->base.recvPos,
                                   needed, &fd);
         if (fd != -1) {
            s->passFd.fd = fd;
            s->passFd.expected = FALSE;
         }
      } else {
         recvd = SSL_Read(s->sslSock,
                          (uint8 *) s->base.recvBuf +
                          s->base.recvPos,
                          needed);
      }
      /*
       * Do NOT make any system call directly or indirectly here
       * unless you can preserve the system error number
       */
      if (recvd > 0) {
         TCPSOCKLOG(3, s, ("need\t%d\trecv\t%d\tremain\t%d\n", needed, recvd,
                           needed - recvd));
         s->sslConnected = TRUE;
         s->base.recvPos += recvd;
         if (AsyncSocketCheckAndDispatchRecv(&s->base, &result)) {
            goto exit;
         }
      } else if (recvd == 0) {
         TCPSOCKLG0(s, ("recv detected client closed connection\n"));
         /*
          * We treat this as an error so that the owner can detect closing
          * of connection by peer (via the error handler callback).
          */
         result = ASOCKERR_REMOTE_DISCONNECT;
         goto exit;
      } else if ((sysErr = ASOCK_LASTERROR()) == ASOCK_EWOULDBLOCK) {
         TCPSOCKLOG(4, s, ("recv would block\n"));
         break;
      } else {
         TCPSOCKLG0(s, ("recv error %d: %s\n", sysErr,
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

      needed = s->base.recvLen - s->base.recvPos;
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
   AsyncTCPSocketRelease(s);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketDispatchSentBuffer --
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

static int
AsyncTCPSocketDispatchSentBuffer(AsyncTCPSocket *s)         // IN
{
   int result = ASOCKERR_SUCCESS;

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
   free(head);

   if (tmp.sendFn) {
      /*
       * Firing the send completion cannot trigger immediate
       * destruction of the socket because we hold a refCount across
       * this and all other application callbacks.  If the socket is
       * closed, however, we need to bubble the information up to the
       * caller in the same way as we do in the Recv callback case.
       */
      ASSERT(s->base.refCount > 1);
      tmp.sendFn(tmp.buf, tmp.len, BaseSocket(s), tmp.clientData);
      if (AsyncTCPSocketGetState(s) == AsyncSocketClosed) {
         TCPSOCKLG0(s, ("owner closed connection in send callback\n"));
         result = ASOCKERR_CLOSED;
      }
   }

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketWriteBuffers --
 *
 *      The meat of AsyncTCPSocket's sending functionality.  This function
 *      actually writes to the wire assuming there's space in the buffers
 *      for the socket.
 *
 * Results:
 *      ASOCKERR_SUCCESS if everything worked, else ASOCKERR_GENERIC.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketWriteBuffers(AsyncTCPSocket *s)         // IN
{
   int result;

   ASSERT(AsyncTCPSocketIsLocked(s));

   if (s->sendBufList == NULL) {
      return ASOCKERR_SUCCESS;     /* Vacuously true */
   }

   if (AsyncTCPSocketGetState(s) != AsyncSocketConnected) {
      TCPSOCKWARN(s, ("write buffers on a disconnected socket!\n"));
      return ASOCKERR_GENERIC;
   }

   AsyncTCPSocketAddRef(s);

   while (s->sendBufList && AsyncTCPSocketGetState(s) == AsyncSocketConnected) {
      SendBufList *head = s->sendBufList;
      int error = 0;
      int sent = 0;
      int left = head->len - s->sendPos;
      int sizeToSend = head->len;

      sent = SSL_Write(s->sslSock,
                       (uint8 *) head->buf + s->sendPos, left);
      /*
       * Do NOT make any system call directly or indirectly here
       * unless you can preserve the system error number
       */
      if (sent > 0) {
         TCPSOCKLOG(3, s, ("left\t%d\tsent\t%d\tremain\t%d\n",
                        left, sent, left - sent));
         s->sendBufFull = FALSE;
         s->sslConnected = TRUE;
         if ((s->sendPos += sent) == sizeToSend) {
            result = AsyncTCPSocketDispatchSentBuffer(s);
            if (result != ASOCKERR_SUCCESS) {
               goto exit;
            }
         }
      } else if (sent == 0) {
         TCPSOCKLG0(s, ("socket write() should never return 0.\n"));
         NOT_REACHED();
      } else if ((error = ASOCK_LASTERROR()) != ASOCK_EWOULDBLOCK) {
         TCPSOCKLG0(s, ("send error %d: %s\n", error, Err_Errno2String(error)));
         s->genericErrno = error;
         if (error == ASOCK_EPIPE || error == ASOCK_ECONNRESET) {
            result = ASOCKERR_REMOTE_DISCONNECT;
         } else {
            result = ASOCKERR_GENERIC;
         }
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
   AsyncTCPSocketRelease(s);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketAcceptInternal --
 *
 *      The meat of 'accept'.  This function can be invoked either via a
 *      poll callback or blocking. We call accept to get the new socket fd,
 *      create a new asock, and call the newFn callback previously supplied
 *      by the call to AsyncTCPSocket_Listen.
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
AsyncTCPSocketAcceptInternal(AsyncTCPSocket *s)         // IN
{
   AsyncTCPSocket *newsock;
   int sysErr;
   int fd;
   struct sockaddr_storage remoteAddr;
   socklen_t remoteAddrLen = sizeof remoteAddr;

   ASSERT(AsyncTCPSocketIsLocked(s));
   ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketListening);

   if ((fd = accept(s->fd, (struct sockaddr *)&remoteAddr,
                    &remoteAddrLen)) == -1) {
      sysErr = ASOCK_LASTERROR();
      s->genericErrno = sysErr;
      if (sysErr == ASOCK_EWOULDBLOCK) {
         TCPSOCKWARN(s, ("spurious accept notification\n"));
#if TARGET_OS_IPHONE
         /*
          * For iOS, while the app is suspended and device's screen is locked,
          * system will reclaim resources from underneath socket(see Apple
          * Technical Note TN2277), the callback function AsyncTCPSocketAcceptCallback()
          * will be invoked repeatedly, to deal with this issue, we need to
          * handle error EWOULDBLOCK.
          */
         return ASOCKERR_ACCEPT;
#else
         return ASOCKERR_GENERIC;
#endif
#ifndef _WIN32
         /*
          * This sucks. Linux accept() can return ECONNABORTED for connections
          * that closed before we got to actually call accept(), but Windows
          * just ignores this case. So we have to special case for Linux here.
          * We return ASOCKERR_GENERIC here because we still want to continue
          * accepting new connections.
          */

      } else if (sysErr == ECONNABORTED) {
         TCPSOCKLG0(s, ("accept: new connection was aborted\n"));

         return ASOCKERR_GENERIC;
#endif
      } else {
         TCPSOCKWARN(s, ("accept failed on fd %d, error %d: %s\n",
                       s->fd, sysErr, Err_Errno2String(sysErr)));

         return ASOCKERR_ACCEPT;
      }
   }

   if (remoteAddr.ss_family == AF_INET6 &&
       AsyncTCPSocketOSVersionSupportsV4Mapped()) {
      struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&remoteAddr;

      /*
       * Remote address should not be a V4MAPPED address. Validate for the rare
       * case that IPV6_V6ONLY is not defined and V4MAPPED is enabled by
       * default when setting up socket listener.
       */

      if (IN6_IS_ADDR_V4MAPPED(&(addr6->sin6_addr))) {
         TCPSOCKWARN(s,
                   ("accept rejected on fd %d due to a IPv4-mapped IPv6 "
                    "remote connection address.\n", s->fd));
         SSLGeneric_close(fd);

         return ASOCKERR_ACCEPT;
      }
   }

   newsock = AsyncTCPSocketAttachToFd(fd, AsyncTCPSocketPollParams(s), NULL);
   if (!newsock) {
      SSLGeneric_close(fd);

      return ASOCKERR_ACCEPT;
   }

   newsock->remoteAddr = remoteAddr;
   newsock->remoteAddrLen = remoteAddrLen;
   AsyncTCPSocketSetState(newsock, AsyncSocketConnected);
   newsock->internalRecvFn = s->internalRecvFn;
   newsock->internalSendFn = s->internalSendFn;

   /*
    * Fire the connect callback:
    */
   s->connectFn(BaseSocket(newsock), s->clientData);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketConnectInternal --
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
AsyncTCPSocketConnectInternal(AsyncTCPSocket *s)         // IN
{
   int optval = 0, optlen = sizeof optval, sysErr;

   ASSERT(AsyncTCPSocketIsLocked(s));
   ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnecting);

   /* Remove when bug 859728 is fixed */
   if (vmx86_server && s->remoteAddr.ss_family == AF_UNIX) {
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
      TCPSOCKLOG(1, s, ("connection SO_ERROR: %s\n", Err_Errno2String(optval)));

      return ASOCKERR_GENERIC;
   }

   s->localAddrLen = sizeof s->localAddr;
   if (getsockname(s->fd, (struct sockaddr *)&s->localAddr,
                   &s->localAddrLen) != 0) {
      sysErr = ASOCK_LASTERROR();
      s->genericErrno = sysErr;
      Warning(ASOCKPREFIX "getsockname for connect on fd %d failed with "
              "error %d: %s\n", s->fd, sysErr, Err_Errno2String(sysErr));

      return ASOCKERR_GENERIC;
   }

done:
   AsyncTCPSocketSetState(s, AsyncSocketConnected);
   s->connectFn(BaseSocket(s), s->clientData);

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetGenericErrno --
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

static int
AsyncTCPSocketGetGenericErrno(AsyncSocket *base)  // IN:
{
   AsyncTCPSocket *asock = TCPSocket(base);
   ASSERT(asock);
   return asock->genericErrno;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketWaitForConnection --
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

static int
AsyncTCPSocketWaitForConnection(AsyncSocket *base,  // IN:
                                int timeoutMS)      // IN:
{
   AsyncTCPSocket *s = TCPSocket(base);
   Bool read = FALSE;
   int error;
   VmTimeType now, done;
   Bool removed = FALSE;

   ASSERT(AsyncTCPSocketIsLocked(s));

   if (AsyncTCPSocketGetState(s) == AsyncSocketConnected) {
      return ASOCKERR_SUCCESS;
   }

   if (AsyncTCPSocketGetState(s) != AsyncSocketListening &&
       AsyncTCPSocketGetState(s) != AsyncSocketConnecting) {
      return ASOCKERR_GENERIC;
   }

   read = AsyncTCPSocketGetState(s) == AsyncSocketListening;

   /*
    * For listening sockets, unregister AsyncTCPSocketAcceptCallback before
    * starting polling and re-register before returning.
    *
    * ConnectCallback() is either registered as a device or rtime callback
    * depending on the prior return value of connect(). So we try to remove it
    * from both.
    */
   if (read) {
      if (s->fd == -1) {
         if (s->listenAsock4) {
            ASSERT(AsyncTCPSocketIsLocked(s->listenAsock4));
            AsyncTCPSocketCancelListenCb(s->listenAsock4);
         }
         if (s->listenAsock6) {
            ASSERT(AsyncTCPSocketIsLocked(s->listenAsock6));
            AsyncTCPSocketCancelListenCb(s->listenAsock6);
         }
      } else {
         AsyncTCPSocketCancelListenCb(s);
      }

      removed = TRUE;
   } else {
      removed = (AsyncTCPSocketPollRemove(s, TRUE, POLL_FLAG_WRITE,
                                          AsyncTCPSocketConnectCallback) ||
                 AsyncTCPSocketPollRemove(s, FALSE, 0,
                                          AsyncTCPSocketConnectCallback));
      ASSERT(removed);
      if (s->internalConnectFn) {
         removed = AsyncTCPSocketPollRemove(s, FALSE, POLL_FLAG_PERIODIC,
                                            AsyncTCPSocketConnectErrorCheck);
         ASSERT(removed);
         s->internalConnectFn = NULL;
      }
   }

   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;

   do {
      AsyncTCPSocket *asock = NULL;

      error = AsyncTCPSocketPoll(s, read, done - now, &asock);
      if (error != ASOCKERR_SUCCESS) {
         goto out;
      }

      now = Hostinfo_SystemTimerUS() / 1000;

      if (read) {
         if (AsyncTCPSocketAcceptInternal(asock) != ASOCKERR_SUCCESS) {
            TCPSOCKLG0(s, ("wait for connection: accept failed\n"));

            /*
             * Just fall through, we'll loop and try again as long as we still
             * have time remaining.
             */

         } else {
            error = ASOCKERR_SUCCESS;
            goto out;
         }
      } else {
         error = AsyncTCPSocketConnectInternal(asock);
         goto out;
      }
   } while ((now < done && timeoutMS > 0) || (timeoutMS < 0));

   error = ASOCKERR_TIMEOUT;

out:
   if (read && removed) {
      if (s->fd == -1) {
         if (s->listenAsock4 &&
             AsyncTCPSocketGetState(s->listenAsock4) != AsyncSocketClosed) {
            if (!AsyncTCPSocketAddListenCb(s->listenAsock4)) {
               error = ASOCKERR_POLL;
            }
         }

         if (s->listenAsock6 &&
             AsyncTCPSocketGetState(s->listenAsock6) != AsyncSocketClosed) {
            if (!AsyncTCPSocketAddListenCb(s->listenAsock6)) {
               error = ASOCKERR_POLL;
            }
         }
      } else if (AsyncTCPSocketGetState(s) != AsyncSocketClosed) {
         if (!AsyncTCPSocketAddListenCb(s)) {
            error = ASOCKERR_POLL;
         }
      }
   }

   return error;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketDoOneMsg --
 *
 *      Spins a socket until the specified amount of time has elapsed or
 *      data has arrived / been sent.
 *
 * Results:
 *      ASOCKERR_SUCCESS if it worked, ASOCKERR_GENERIC on system call
 *         failures
 *      ASOCKERR_BUSY if another thread is in the read callback.
 *      ASOCKERR_TIMEOUT if nothing happened in the allotted time.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketDoOneMsg(AsyncSocket *base, // IN
                       Bool read,         // IN
                       int timeoutMS)     // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   AsyncTCPSocket *asock = NULL;
   int retVal;

   ASSERT(AsyncTCPSocketIsLocked(s));
   ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);

   if (read) {
      if (s->inRecvLoop) {
         /*
          * The recv loop would read the data if there is any and it is
          * not safe to proceed and race with the recv loop.
          */
         TCPSOCKLG0(s, ("busy: another thread in recv loop\n"));
         return ASOCKERR_BUSY;
      }

      /*
       * Bug 158571: There could other threads polling on the same asyncsocket.
       * If two threads land up polling  on the same socket at the same time,
       * the first thread to be scheduled reads the data from the socket,
       * while the second one blocks infinitely. This hangs the VM. To prevent
       * this, we temporarily remove the poll callback and then reinstate it
       * after reading the data.
       */

      ASSERT(s->recvCb); /* We are supposed to call someone... */
      AsyncTCPSocketAddRef(s);
      AsyncTCPSocketCancelRecvCb(s);
      s->recvCb = TRUE;  /* We need to know if the callback cancel recv. */

      s->inBlockingRecv++;
      retVal = AsyncTCPSocketPoll(s, read, timeoutMS, &asock);
      if (retVal != ASOCKERR_SUCCESS) {
         if (retVal == ASOCKERR_GENERIC) {
            TCPSOCKWARN(s, ("%s: failed to poll on the socket during read.\n",
                       __FUNCTION__));
         }
      } else {
         ASSERT(asock == s);
         retVal = AsyncTCPSocketFillRecvBuffer(s);
      }
      s->inBlockingRecv--;

      /*
       * If socket got closed in AsyncTCPSocketFillRecvBuffer, we
       * cannot add poll callback - AsyncSocket_Close() would remove
       * it if we would not remove it above.
       */

      if (AsyncTCPSocketGetState(s) != AsyncSocketClosed && s->recvCb) {
         ASSERT(s->base.refCount > 1); /* We shouldn't be last user of socket. */
         ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);
         /*
          * If AsyncTCPSocketPoll or AsyncTCPSocketFillRecvBuffer fails, do not
          * add the recv callback as it may never fire.
          */
         s->recvCb = FALSE;  /* For re-registering the poll callback. */
         if (retVal == ASOCKERR_SUCCESS || retVal == ASOCKERR_TIMEOUT) {
            retVal = AsyncTCPSocketRegisterRecvCb(s);
            Log("SOCKET reregister recvCb after DoOneMsg (ref %d)\n",
                BaseSocket(s)->refCount);
         }
         if (retVal != ASOCKERR_SUCCESS) {
            s->base.recvBuf = NULL;
         }
      }
      AsyncTCPSocketRelease(s);
   } else {
      AsyncTCPSocketAddRef(s);
      retVal = AsyncTCPSocketPoll(s, read, timeoutMS, &asock);
      if (retVal != ASOCKERR_SUCCESS) {
         if (retVal == ASOCKERR_GENERIC) {
            TCPSOCKWARN(s, ("%s: failed to poll on the socket during write.\n",
                            __FUNCTION__));
         }
      } else {
         ASSERT(asock == s);
         retVal = AsyncTCPSocketWriteBuffers(s);
      }
      AsyncTCPSocketRelease(s);
   }

   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_TCPDrainRecv --
 *
 *      This function can be used to drain all the messages from a socket
 *      disconnected on the remote end.  It spins a socket until the specified
 *      amount of time has elapsed or an error is encountered, with backoff
 *      between read attempts if there is a conflict with another thread.  The
 *      recv callback is restored at the end of this only if not all the
 *      messages have been read, the socket is still connected and recv callack
 *      has not been cancelled.
 *
 * Results:
 *      ASOCKERR_SUCCESS if all messages are have been read, or if the callback
 *      has canceled the recv, or if the socket is closed
 *      ASOCKERR_GENERIC on system call failures
 *      ASOCKERR_TIMEOUT if there may still be unread messages at the end of
 *      the speccified time.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_TCPDrainRecv(AsyncSocket *base, // IN
                         int timeoutMS)     // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   int retVal;
   Bool cbRemoved = FALSE;
   Bool releaseLock = FALSE;
   unsigned count = 0;
   VmTimeType startMS = Hostinfo_SystemTimerMS();
   VmTimeType nowMS;

   ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);
   ASSERT(s->recvCb); /* We are supposed to call someone... */

   if (!AsyncTCPSocketIsLocked(s) || !Poll_LockingEnabled()) {
      AsyncTCPSocketLock(s);
      releaseLock = TRUE;
   }
   AsyncTCPSocketAddRef(s);

   while (TRUE) {
      AsyncTCPSocket *asock = NULL;

      count++;
      if (s->inRecvLoop) {
         /*
          * The recv loop would read the data if there is any and it is
          * not safe to proceed and race with the recv loop.
          */
         TCPSOCKLG0(s, ("busy: another thread in recv loop\n"));
         retVal = ASOCKERR_BUSY;
         /* Add a bit of backoff. */
         AsyncTCPSocketUnlock(s);
         Util_Usleep(MIN(100 << (mssb32(count) / 2), timeoutMS));
         AsyncTCPSocketLock(s);
         goto retry;
      }

      if (!cbRemoved) {
         /*
          * Cancel the recv callback, but pretend that it is still registered
          * so we know if the callback cancel recv.
          */
         AsyncTCPSocketCancelRecvCb(s);
         s->recvCb = TRUE;
         cbRemoved = TRUE;
      }

      s->inBlockingRecv++;
      retVal = AsyncTCPSocketPoll(s, TRUE, 0, &asock);
      if (retVal != ASOCKERR_SUCCESS) {
         if (retVal == ASOCKERR_GENERIC) {
            TCPSOCKWARN(s, ("%s: failed to poll on the socket during read.\n",
                       __FUNCTION__));
         }
      } else if (AsyncTCPSocketGetState(s) == AsyncSocketConnected) {
         ASSERT(asock == s);
         retVal = AsyncTCPSocketFillRecvBuffer(s);
      }
      s->inBlockingRecv--;

retry:
      if (retVal == ASOCKERR_REMOTE_DISCONNECT ||
          AsyncTCPSocketGetState(s) == AsyncSocketClosed ||
          !s->recvCb) {
         /* No more messages to recv. */
         retVal = ASOCKERR_SUCCESS;
         break;
      }
      if (retVal == ASOCKERR_GENERIC) {
         break;
      }

      nowMS = Hostinfo_SystemTimerMS();
      if (nowMS >= startMS + timeoutMS) {
         retVal = ASOCKERR_TIMEOUT;
         break;
      }
      timeoutMS -= nowMS - startMS;
      startMS = nowMS;
      ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected && s->recvCb);
   }

   if (cbRemoved) {
      s->recvCb = FALSE;
      /*
       * If AsyncTCPSocketPoll or AsyncTCPSocketFillRecvBuffer fails, do not
       * add the recv callback as it may never fire.
       */
      if (retVal == ASOCKERR_TIMEOUT) {
         ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);
         ASSERT(s->base.refCount > 1); /* We better not be the last user */
         retVal = AsyncTCPSocketRegisterRecvCb(s);
         Log("SOCKET reregister recvCb after DrainRecv (ref %d)\n",
             BaseSocket(s)->refCount);
      }
   }
   if (!s->recvCb) {
      s->base.recvBuf = NULL;
   }

   AsyncTCPSocketRelease(s);
   if (releaseLock) {
      AsyncTCPSocketUnlock(s);
   }
   return retVal;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketFlush --
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

static int
AsyncTCPSocketFlush(AsyncSocket *base,  // IN
                    int timeoutMS)      // IN
{
   AsyncTCPSocket *s = TCPSocket(base);
   VmTimeType now, done;
   int retVal;

   if (s == NULL) {
      Warning(ASOCKPREFIX "Flush called with invalid arguments!\n");

      return ASOCKERR_INVAL;
   }

   ASSERT(AsyncTCPSocketIsLocked(s));
   AsyncTCPSocketAddRef(s);

   if (AsyncTCPSocketGetState(s) != AsyncSocketConnected) {
      TCPSOCKWARN(s, ("flush called but state is not connected!\n"));
      retVal = ASOCKERR_INVAL;
      goto outHaveLock;
   }

   now = Hostinfo_SystemTimerUS() / 1000;
   done = now + timeoutMS;

   while (s->sendBufList) {
      AsyncTCPSocket *asock = NULL;

      retVal = AsyncTCPSocketPoll(s, FALSE, done - now, &asock);
      if (retVal != ASOCKERR_SUCCESS) {
         TCPSOCKWARN(s, ("flush failed\n"));
         goto outHaveLock;
      }

      ASSERT(asock == s);
      if ((retVal = AsyncTCPSocketWriteBuffers(s)) != ASOCKERR_SUCCESS) {
         goto outHaveLock;
      }
      ASSERT(AsyncTCPSocketGetState(s) == AsyncSocketConnected);

      /* Setting timeoutMS to -1 means never timeout. */
      if (timeoutMS >= 0) {
         now = Hostinfo_SystemTimerUS() / 1000;

         /* Don't timeout if you've sent everything */
         if (now > done && s->sendBufList) {
            TCPSOCKWARN(s, ("flush timed out\n"));
            retVal = ASOCKERR_TIMEOUT;
            goto outHaveLock;
         }
      }
   }

   retVal = ASOCKERR_SUCCESS;

outHaveLock:
   AsyncTCPSocketRelease(s);

   return retVal;
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

static void
AsyncTCPSocketCancelListenCb(AsyncTCPSocket *asock)  // IN:
{
   Bool removed;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   removed = AsyncTCPSocketPollRemove(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      AsyncTCPSocketAcceptCallback);
   ASSERT(removed);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketAddListenCb --
 *
 *      Socket specific code for adding callbacks for a listening socket.
 *
 * Results:
 *      TRUE if Poll callback successfully added.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
AsyncTCPSocketAddListenCb(AsyncTCPSocket *asock)  // IN:
{
   VMwareStatus pollStatus;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   pollStatus = AsyncTCPSocketPollAdd(asock, TRUE,
                                      POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                      AsyncTCPSocketAcceptCallback);

   if (pollStatus != VMWARE_STATUS_SUCCESS) {
      TCPSOCKWARN(asock, ("failed to install listen accept callback!\n"));
   }

   return pollStatus == VMWARE_STATUS_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketCancelRecvCb --
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

static void
AsyncTCPSocketCancelRecvCb(AsyncTCPSocket *asock)  // IN:
{
   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (asock->recvCbTimer) {
      AsyncTCPSocketPollRemove(asock, FALSE, 0, asock->internalRecvFn);
      asock->recvCbTimer = FALSE;
   }
   if (asock->recvCb) {
      Bool removed;
      TCPSOCKLOG(1, asock,
                 ("Removing poll recv callback while cancelling recv.\n"));
      removed = AsyncTCPSocketPollRemove(asock, TRUE,
                                         POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                         asock->internalRecvFn);

      /*
       * A recv callback registered on a bad FD can be deleted by
       * PollHandleInvalidFd if POLL_FLAG_ACCEPT_INVALID_FDS flag
       * is added to asyncsocket.
       */
      ASSERT(removed || AsyncTCPSocketPollParams(asock)->iPoll ||
             (AsyncTCPSocketPollParams(asock)->flags &
              POLL_FLAG_ACCEPT_INVALID_FDS) != 0);
      asock->recvCb = FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketCancelCbForClose --
 *
 *      Cancel future asynchronous send and recv by unregistering
 *      their Poll callbacks, and change the socket state to
 *      AsyncTCPSocketCBCancelled if the socket state is AsyncTCPSocketConnected.
 *
 *      The function can be called in a send/recv error handler before
 *      actually closing the socket in a separate thread, to prevent other
 *      code calling AsyncTCPSocket_Send/Recv from re-registering the
 *      callbacks again. The next operation should be just AsyncSocket_Close().
 *      This helps to avoid unnecessary send/recv callbacks before the
 *      socket is closed.
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      Unregisters send/recv Poll callbacks, and fires the send
 *      triggers for any remaining output buffers. May also change
 *      the socket state.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketCancelCbForClose(AsyncSocket *base)  // IN:
{
   AsyncTCPSocket *asock = TCPSocket(base);
   Bool removed;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) == AsyncSocketConnected) {
      AsyncTCPSocketSetState(asock, AsyncSocketCBCancelled);
   }

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

   ASSERT(!asock->base.recvBuf || asock->base.recvFn);

   if (asock->recvCbTimer) {
      AsyncTCPSocketPollRemove(asock, FALSE, 0, asock->internalRecvFn);
      asock->recvCbTimer = FALSE;
   }
   if (asock->recvCb) {
      TCPSOCKLOG(1, asock, ("recvCb is non-NULL, removing recv callback\n"));
      removed = AsyncTCPSocketPollRemove(asock, TRUE,
                                         POLL_FLAG_READ | POLL_FLAG_PERIODIC,
                                         asock->internalRecvFn);
      /* Callback might be temporarily removed in AsyncSocket_DoOneMsg. */
      ASSERT_NOT_TESTED(removed ||
                        asock->inBlockingRecv ||
                        AsyncTCPSocketPollParams(asock)->iPoll);

      asock->recvCb = FALSE;
      asock->base.recvBuf = NULL;
   }

   if (asock->sendCb) {
      TCPSOCKLOG(1, asock,
                 ("sendBufList is non-NULL, removing send callback\n"));

      /*
       * The send callback could be either a device or RTime callback, so
       * we check the latter if it wasn't the former.
       */

      if (asock->sendCbTimer) {
         removed = AsyncTCPSocketPollRemove(asock, FALSE, 0,
                                         asock->internalSendFn);
      } else {
         removed = AsyncTCPSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                         asock->internalSendFn);
      }
      ASSERT(removed || AsyncTCPSocketPollParams(asock)->iPoll);
      asock->sendCb = FALSE;
      asock->sendCbTimer = FALSE;
   }

   /*
    * Go through any send buffers on the list and fire their
    * callbacks, reflecting back how much of each buffer has been
    * submitted to the kernel.  For the first buffer in the list that
    * may be non-zero, for subsequent buffers it will be zero.
    */
   AsyncTCPSocketAddRef(asock);
   while (asock->sendBufList) {
      /*
       * Pop each remaining buffer and fire its completion callback.
       */

      SendBufList *cur = asock->sendBufList;
      int pos = asock->sendPos;

      asock->sendBufList = asock->sendBufList->next;
      asock->sendPos = 0;

      if (cur->sendFn) {
         cur->sendFn(cur->buf, pos, BaseSocket(asock), cur->clientData);
      }
      free(cur);
   }
   AsyncTCPSocketRelease(asock);
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketCancelCbForConnectingClose --
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

static Bool
AsyncTCPSocketCancelCbForConnectingClose(AsyncTCPSocket *asock) // IN
{
   return (AsyncTCPSocketPollRemove(asock, TRUE, POLL_FLAG_WRITE,
                                    AsyncTCPSocketConnectCallback) ||
           AsyncTCPSocketPollRemove(asock, FALSE, 0,
                                    AsyncTCPSocketConnectCallback));
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSetCloseOptions --
 *
 *      Enables optional behavior for AsyncSocket_Close():
 *
 *      - If flushEnabledMaxWaitMsec is non-zero, the output stream
 *        will be flushed synchronously before the socket is closed.
 *        (default is zero: close socket right away without flushing)
 *
 *      - If closeCb is set, the callback will be called asynchronously
 *        when the socket is actually destroyed.
 *        (default is NULL: no callback)
 *
 * Results:
 *      ASOCKERR_*.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketSetCloseOptions(AsyncSocket *base,           // IN
                              int flushEnabledMaxWaitMsec, // IN
                              AsyncSocketCloseFn closeCb)  // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   asock->flushEnabledMaxWaitMsec = flushEnabledMaxWaitMsec;
   asock->closeCb = closeCb;
   VERIFY(closeCb == NULL);
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketClose --
 *
 *      AsyncTCPSocket destructor. The destructor should be safe to call at any
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

static int
AsyncTCPSocketClose(AsyncSocket *base)   // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) == AsyncSocketClosed) {
      Warning("%s() called on already closed asock!\n", __FUNCTION__);
      return ASOCKERR_CLOSED;
   }

   if (asock->listenAsock4 || asock->listenAsock6) {
      if (asock->listenAsock4) {
         AsyncSocket_Close(BaseSocket(asock->listenAsock4));
      }
      if (asock->listenAsock6) {
         AsyncSocket_Close(BaseSocket(asock->listenAsock6));
      }
   } else {
      Bool removed;
      AsyncSocketState oldState;

      /* Flush output if requested via AsyncTCPSocket_SetCloseOptions(). */
      if (asock->flushEnabledMaxWaitMsec &&
          AsyncTCPSocketGetState(asock) == AsyncSocketConnected &&
          !asock->base.errorSeen) {
         int ret = AsyncTCPSocketFlush(BaseSocket(asock),
                                       asock->flushEnabledMaxWaitMsec);
         if (ret != ASOCKERR_SUCCESS) {
            TCPSOCKWARN(asock,
                        ("AsyncTCPSocket_Flush failed: %s. Closing now.\n",
                         AsyncSocket_Err2String(ret)));
         }
      }

      /*
       * Set the new state to closed, and then check the old state and do the
       * right thing accordingly
       */

      TCPSOCKLOG(1, asock, ("closing socket\n"));
      oldState = AsyncTCPSocketGetState(asock);
      AsyncTCPSocketSetState(asock, AsyncSocketClosed);

      switch(oldState) {
      case AsyncSocketListening:
         TCPSOCKLOG(1, asock, ("old state was listening, removing accept "
                               "callback\n"));
         AsyncTCPSocketCancelListenCb(asock);
         break;

      case AsyncSocketConnecting:
         TCPSOCKLOG(1, asock, ("old state was connecting, removing connect "
                               "callback\n"));
         removed = AsyncTCPSocketCancelCbForConnectingClose(asock);
         if (!removed) {
            TCPSOCKLOG(1, asock, ("connect callback is not present in the poll "
                                  "list.\n"));
         }
         break;

      case AsyncSocketConnected:
         TCPSOCKLOG(1, asock, ("old state was connected\n"));
         AsyncTCPSocketCancelCbForClose(BaseSocket(asock));
         break;

      case AsyncSocketCBCancelled:
         TCPSOCKLOG(1, asock, ("old state was CB-cancelled\n"));
         break;

      default:
         NOT_REACHED();
      }

      if (asock->internalConnectFn) {
         removed = AsyncTCPSocketPollRemove(asock, FALSE, POLL_FLAG_PERIODIC,
                                            AsyncTCPSocketConnectErrorCheck);
         ASSERT(removed);
         asock->internalConnectFn = NULL;
      }

      if (asock->sslConnectFn && asock->sslPollFlags > 0) {
         removed = AsyncTCPSocketPollRemove(asock, TRUE, asock->sslPollFlags,
                                            AsyncTCPSocketSslConnectCallback);
         ASSERT(removed);
      }

      if (asock->sslAcceptFn && asock->sslPollFlags > 0) {
         removed = AsyncTCPSocketPollRemove(asock, TRUE, asock->sslPollFlags,
                                            AsyncTCPSocketSslAcceptCallback);
         ASSERT(removed);
      }
      asock->sslPollFlags = 0;

      /*
       * Close the underlying SSL sockets.
       */
      SSL_Shutdown(asock->sslSock);

      if (asock->passFd.fd != -1) {
         SSLGeneric_close(asock->passFd.fd);
      }
   }

   AsyncSocketTeardownSocket(base);
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketIsSendBufferFull --
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

static int
AsyncTCPSocketIsSendBufferFull(AsyncSocket *base)         // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   return asock->sendBufFull;
}




/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketHasDataPending --
 *
 *      Determine if SSL has any pending/unread data.
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
AsyncTCPSocketHasDataPending(AsyncTCPSocket *asock)   // IN:
{
   return SSL_Pending(asock->sslSock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketMakeNonBlocking --
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
AsyncTCPSocketMakeNonBlocking(int fd)         // IN
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
 * AsyncSocketAcceptCallback --
 *
 *      Poll callback for listening fd waiting to complete an accept
 *      operation. We call accept to get the new socket fd, create a new
 *      asock, and call the newFn callback previously supplied by the call to
 *      AsyncTCPSocket_Listen.
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
AsyncTCPSocketAcceptCallback(void *clientData)         // IN
{
   AsyncTCPSocket *asock = clientData;
   int retval;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll == NULL);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   AsyncTCPSocketAddRef(asock);
   retval = AsyncTCPSocketAcceptInternal(asock);

   /*
    * See comment for return value of AsyncTCPSocketAcceptInternal().
    */

   if (retval == ASOCKERR_ACCEPT) {
      AsyncTCPSocketHandleError(asock, retval);
   }
   AsyncTCPSocketRelease(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketConnectCallback --
 *
 *      Poll callback for connecting fd. Calls through to
 *      AsyncTCPSocketConnectInternal to do the real work.
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
AsyncTCPSocketConnectCallback(void *clientData)         // IN
{
   AsyncTCPSocket *asock = clientData;
   int retval;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll == NULL);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   AsyncTCPSocketAddRef(asock);
   retval = AsyncTCPSocketConnectInternal(asock);
   if (retval != ASOCKERR_SUCCESS) {
      ASSERT(retval == ASOCKERR_GENERIC); /* Only one we're expecting */
      AsyncTCPSocketHandleError(asock, retval);
   }
   AsyncTCPSocketRelease(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketRecvCallback --
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

static void
AsyncTCPSocketRecvCallback(void *clientData)         // IN
{
   AsyncTCPSocket *asock = clientData;
   int error;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   AsyncTCPSocketAddRef(asock);

   error = AsyncTCPSocketFillRecvBuffer(asock);
   if (error == ASOCKERR_GENERIC || error == ASOCKERR_REMOTE_DISCONNECT) {
      AsyncTCPSocketHandleError(asock, error);
   }

   AsyncTCPSocketRelease(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketIPollRecvCallback --
 *
 *      Poll callback for input waiting on the socket.  IVmdbPoll does not
 *      handle callback locks, so this function first locks the asyncsocket
 *      and verify that the recv callback has not been cancelled before
 *      calling AsyncTCPSocketFillRecvBuffer to do the real work.
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
AsyncTCPSocketIPollRecvCallback(void *clientData)  // IN:
{
#ifdef VMX86_TOOLS
   NOT_IMPLEMENTED();
#else
   AsyncTCPSocket *asock = clientData;
   MXUserRecLock *lock;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketPollParams(asock)->lock == NULL ||
          !MXUser_IsCurThreadHoldingRecLock(
             AsyncTCPSocketPollParams(asock)->lock));

   AsyncTCPSocketLock(asock);
   if (asock->recvCbTimer) {
      /* IVmdbPoll only has periodic timer callbacks. */
      AsyncTCPSocketIPollRemove(asock, FALSE, 0, asock->internalRecvFn);
      asock->recvCbTimer = FALSE;
   }
   lock = AsyncTCPSocketPollParams(asock)->lock;
   if (asock->recvCb && asock->inBlockingRecv == 0) {
      asock->inIPollCb |= IN_IPOLL_RECV;
      AsyncTCPSocketRecvCallback(clientData);
      asock->inIPollCb &= ~IN_IPOLL_RECV;
      /*
       * Re-register the callback if it has not been canceled.  Lock may have
       * been dropped to fire recv callback so re-check inBlockingRecv.
       */
      if (asock->recvCb && asock->inBlockingRecv == 0) {
         AsyncTCPSocketIPollAdd(asock, TRUE, POLL_FLAG_READ,
                                asock->internalRecvFn, asock->fd);
      }
   } else {
      TCPSOCKLG0(asock, ("Skip recv because %s\n",
                         asock->recvCb ? "blocking recv is in progress"
                                       : "recv callback is cancelled"));
   }

   /* This is a one-shot callback so we always release the reference taken. */
   AsyncTCPSocketRelease(asock);
   AsyncTCPSocketUnlock(asock);
   if (lock != NULL) {
      MXUser_DecRefRecLock(lock);
   }
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSendCallback --
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

static void
AsyncTCPSocketSendCallback(void *clientData)         // IN
{
   AsyncTCPSocket *s = clientData;
   int retval;

   ASSERT(s);
   ASSERT(AsyncTCPSocketIsLocked(s));

   AsyncTCPSocketAddRef(s);
   s->sendCb = FALSE; /* AsyncTCPSocketSendCallback is never periodic */
   s->sendCbTimer = FALSE;
   retval = AsyncTCPSocketWriteBuffers(s);
   if (retval != ASOCKERR_SUCCESS &&
       retval != ASOCKERR_CLOSED) {
      AsyncTCPSocketHandleError(s, retval);
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
         pollStatus = AsyncTCPSocketPollAdd(s, FALSE, 0,
                                         s->internalSendFn, 100000);
         VERIFY(pollStatus == VMWARE_STATUS_SUCCESS);
         s->sendCbTimer = TRUE;
      } else
#endif
      {
         pollStatus = AsyncTCPSocketPollAdd(s, TRUE, POLL_FLAG_WRITE,
                                         s->internalSendFn);
         VERIFY(pollStatus == VMWARE_STATUS_SUCCESS);
      }
      s->sendCb = TRUE;
   }
   AsyncTCPSocketRelease(s);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketIPollSendCallback --
 *
 *      IVmdbPoll callback for output socket buffer space available.  IVmdbPoll
 *      does not handle callback locks, so this function first locks the
 *      asyncsocket and verify that the send callback has not been cancelled.
 *      IVmdbPoll only has periodic callbacks, so this function unregisters
 *      itself before calling AsyncTCPSocketSendCallback to do the real work.
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
AsyncTCPSocketIPollSendCallback(void *clientData)  // IN:
{
#ifdef VMX86_TOOLS
   NOT_IMPLEMENTED();
#else
   AsyncTCPSocket *s = clientData;
   MXUserRecLock *lock;

   ASSERT(s);

   AsyncTCPSocketLock(s);
   s->inIPollCb |= IN_IPOLL_SEND;
   lock = AsyncTCPSocketPollParams(s)->lock;
   if (s->sendCbTimer) {
      /* IVmdbPoll only has periodic timer callback. */
      AsyncTCPSocketIPollRemove(s, FALSE, 0, AsyncTCPSocketIPollSendCallback);
      s->sendCbTimer = FALSE;
   }
   if (s->sendCb) {
      AsyncTCPSocketSendCallback(s);
   } else {
      TCPSOCKLG0(s, ("cancelled send callback fired\n"));
   }

   s->inIPollCb &= ~IN_IPOLL_SEND;
   AsyncTCPSocketRelease(s);
   AsyncTCPSocketUnlock(s);
   if (lock != NULL) {
      MXUser_DecRefRecLock(lock);
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketPollAdd --
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

static VMwareStatus
AsyncTCPSocketPollAdd(AsyncTCPSocket *asock,         // IN
                      Bool socket,                   // IN
                      int flags,                     // IN
                      PollerFunction callback,       // IN
                      ...)                           // IN
{
   int type, info;

   if (socket) {
      ASSERT(asock->fd != -1);
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

   if (AsyncTCPSocketPollParams(asock)->iPoll != NULL) {
      return AsyncTCPSocketIPollAdd(asock, socket, flags, callback, info);
   }

   return Poll_Callback(AsyncTCPSocketPollParams(asock)->pollClass,
                        flags | AsyncTCPSocketPollParams(asock)->flags,
                        callback, asock, type, info,
                        AsyncTCPSocketPollParams(asock)->lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketPollRemove --
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

static Bool
AsyncTCPSocketPollRemove(AsyncTCPSocket *asock,         // IN
                         Bool socket,                   // IN
                         int flags,                     // IN
                         PollerFunction callback)       // IN
{
   int type;

   if (AsyncTCPSocketPollParams(asock)->iPoll != NULL) {
      return AsyncTCPSocketIPollRemove(asock, socket, flags, callback);
   }

   if (socket) {
      ASSERT(asock->fd != -1);
      type = POLL_DEVICE;
      flags |= POLL_FLAG_SOCKET;
   } else {
      type = POLL_REALTIME;
   }

   return Poll_CallbackRemove(AsyncTCPSocketPollParams(asock)->pollClass,
                              flags | AsyncTCPSocketPollParams(asock)->flags,
                              callback, asock, type);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketIPollAdd --
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
AsyncTCPSocketIPollAdd(AsyncTCPSocket *asock,         // IN
                       Bool socket,                   // IN
                       int flags,                     // IN
                       PollerFunction callback,       // IN
                       int info)                      // IN
{
#ifdef VMX86_TOOLS
   return VMWARE_STATUS_ERROR;
#else
   VMwareStatus status = VMWARE_STATUS_SUCCESS;
   VmdbRet ret;
   IVmdbPoll *poll;

   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   /* Protect asyncsocket and lock from disappearing */
   AsyncTCPSocketAddRef(asock);
   if (AsyncTCPSocketPollParams(asock)->lock != NULL) {
      MXUser_IncRefRecLock(AsyncTCPSocketPollParams(asock)->lock);
   }

   poll = AsyncTCPSocketPollParams(asock)->iPoll;

   if (socket) {
      int pollFlags = VMDB_PRF_ONE_SHOT |
                      ((flags & POLL_FLAG_READ) != 0 ? VMDB_PRF_READ
                                                     : VMDB_PRF_WRITE);

      ret = poll->Register(poll, pollFlags, callback, asock, info);
   } else {
      ret = poll->RegisterTimer(poll, callback, asock, info);
   }

   if (ret != VMDB_S_OK) {
      Log(ASOCKPREFIX "failed to register callback (%s %d): error %d\n",
          socket ? "socket" : "delay", info, ret);
      if (AsyncTCPSocketPollParams(asock)->lock != NULL) {
         MXUser_DecRefRecLock(AsyncTCPSocketPollParams(asock)->lock);
      }
      AsyncTCPSocketRelease(asock);
      status = VMWARE_STATUS_ERROR;
   }

   return status;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketIPollRemove --
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
AsyncTCPSocketIPollRemove(AsyncTCPSocket *asock,         // IN
                          Bool socket,                   // IN
                          int flags,                     // IN
                          PollerFunction callback)       // IN
{
#ifdef VMX86_TOOLS
   return FALSE;
#else
   IVmdbPoll *poll;
   Bool ret;

   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   poll = AsyncTCPSocketPollParams(asock)->iPoll;

   if (socket) {
      int pollFlags = VMDB_PRF_ONE_SHOT |
                      ((flags & POLL_FLAG_READ) != 0 ? VMDB_PRF_READ
                                                     : VMDB_PRF_WRITE);

      ret = poll->Unregister(poll, pollFlags, callback, asock);
   } else {
      ret = poll->UnregisterTimer(poll, callback, asock);
   }

   if (ret &&
       !((asock->inIPollCb & IN_IPOLL_RECV) != 0 &&
         callback == asock->internalRecvFn) &&
       !((asock->inIPollCb & IN_IPOLL_SEND) != 0 &&
         callback == asock->internalSendFn)) {
      MXUserRecLock *lock = AsyncTCPSocketPollParams(asock)->lock;

      /*
       * As the callback has been unregistered and we are not currently in
       * the callback being removed, we can safely release the reference taken
       * when registering the callback.
       */
      AsyncTCPSocketRelease(asock);
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
 * AsyncTCPSocketCancelRecv --
 *
 *    Call this function if you know what you are doing. This should be
 *    called if you want to synchronously receive the outstanding data on
 *    the socket. It removes the recv poll callback. It also returns number of
 *    partially read bytes (if any). A partially read response may exist as
 *    AsyncTCPSocketRecvCallback calls the recv callback only when all the data
 *    has been received.
 *
 * Results:
 *    ASOCKERR_SUCCESS or ASOCKERR_INVAL.
 *
 * Side effects:
 *    Subsequent client call to AsyncTCPSocket_Recv can reinstate async behaviour.
 *
 *-----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketCancelRecv(AsyncSocket *base,          // IN
                         int *partialRecvd,          // OUT
                         void **recvBuf,             // OUT
                         void **recvFn,              // OUT
                         Bool cancelOnSend)          // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) != AsyncSocketConnected) {
      Warning(ASOCKPREFIX "Failed to cancel request on disconnected socket!\n");
      return ASOCKERR_INVAL;
   }

   if (asock->inBlockingRecv && !asock->inRecvLoop) {
      Warning(ASOCKPREFIX "Cannot cancel request while a blocking recv is "
                          "pending.\n");
      return ASOCKERR_INVAL;
   }

   if (!cancelOnSend && (asock->sendBufList || asock->sendCb)) {
      Warning(ASOCKPREFIX "Can't cancel request as socket has send operation "
              "pending.\n");
      return ASOCKERR_INVAL;
   }

   AsyncTCPSocketCancelRecvCb(asock);
   AsyncSocketCancelRecv(BaseSocket(asock), partialRecvd, recvBuf, recvFn);

   if (asock->passFd.fd != -1) {
      SSLGeneric_close(asock->passFd.fd);
      asock->passFd.fd = -1;
   }
   asock->passFd.expected = FALSE;

   return ASOCKERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetReceivedFd --
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

static int
AsyncTCPSocketGetReceivedFd(AsyncSocket *base)      // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   int fd;

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (AsyncTCPSocketGetState(asock) != AsyncSocketConnected) {
      Warning(ASOCKPREFIX "Failed to receive fd on disconnected socket!\n");
      return -1;
   }
   fd = asock->passFd.fd;
   asock->passFd.fd = -1;
   asock->passFd.expected = FALSE;

   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketConnectSSL --
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

static Bool
AsyncTCPSocketConnectSSL(AsyncSocket *base,           // IN
                         SSLVerifyParam *verifyParam, // IN/OPT
                         void *sslContext)            // IN/OPT
{
#ifndef USE_SSL_DIRECT
   AsyncTCPSocket *asock = TCPSocket(base);
   ASSERT(asock);

   if (sslContext == NULL) {
      sslContext = SSL_DefaultContext();
   }

   return SSL_ConnectAndVerifyWithContext(asock->sslSock, verifyParam,
                                          sslContext);
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketAcceptSSL --
 *
 *    Initialize the socket's SSL object, by calling SSL_Accept or
 *    SSL_AcceptWithContext.
 *
 * Results:
 *    TRUE if SSL_Accept/SSL_AcceptWithContext succeeded, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
AsyncTCPSocketAcceptSSL(AsyncSocket *base,  // IN
                        void *sslCtx)       // IN: optional
{
#ifndef USE_SSL_DIRECT
   AsyncTCPSocket *asock = TCPSocket(base);
   ASSERT(asock);

   if (sslCtx) {
      return SSL_AcceptWithContext(asock->sslSock, sslCtx);
   } else {
      return SSL_Accept(asock->sslSock);
   }
#else
   return FALSE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSslConnectCallback --
 *
 *      Poll callback to redrive an outstanding ssl connect operation.
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
AsyncTCPSocketSslConnectCallback(void *clientData)  // IN
{
#ifndef USE_SSL_DIRECT
   int sslOpCode;
   VMwareStatus pollStatus;
   AsyncTCPSocket *asock = clientData;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll == NULL);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   AsyncTCPSocketAddRef(asock);

   /* Only set if poll callback is registered */
   asock->sslPollFlags = 0;

   sslOpCode = SSL_TryCompleteConnect(asock->sslSock);
   if (sslOpCode > 0) {
      (*asock->sslConnectFn)(TRUE, BaseSocket(asock), asock->clientData);
   } else if (sslOpCode < 0) {
      (*asock->sslConnectFn)(FALSE, BaseSocket(asock), asock->clientData);
   } else {
      asock->sslPollFlags = SSL_WantRead(asock->sslSock) ?
                            POLL_FLAG_READ : POLL_FLAG_WRITE;

      /* register the poll callback to redrive the SSL connect */
      pollStatus = AsyncTCPSocketPollAdd(asock, TRUE, asock->sslPollFlags,
                                      AsyncTCPSocketSslConnectCallback);

      if (pollStatus != VMWARE_STATUS_SUCCESS) {
         TCPSOCKWARN(asock, ("failed to reinstall ssl connect callback!\n"));
         asock->sslPollFlags = 0;
         (*asock->sslConnectFn)(FALSE, BaseSocket(asock), asock->clientData);
      }
   }

   AsyncTCPSocketRelease(asock);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketStartSslConnect --
 *
 *    Start an asynchronous SSL connect operation.
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
 * Results:
 *    ASOCKERR_SUCCESS or ASOCKERR_*.
 *    Errors during async processing are reported using the callback supplied.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketStartSslConnect(AsyncSocket *base,                    // IN
                              SSLVerifyParam *verifyParam,          // IN/OPT
                              void *sslCtx,                         // IN
                              AsyncSocketSslConnectFn sslConnectFn, // IN
                              void *clientData)                     // IN
{
#ifndef USE_SSL_DIRECT
   AsyncTCPSocket *asock = TCPSocket(base);
   Bool ok;

   ASSERT(asock);
   ASSERT(sslConnectFn);

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (asock->sslConnectFn || asock->sslAcceptFn) {
      TCPSOCKWARN(asock, ("An SSL operation was already initiated.\n"));
      return ASOCKERR_GENERIC;
   }

   ok = SSL_SetupConnectAndVerifyWithContext(asock->sslSock, verifyParam,
                                             sslCtx);
   if (!ok) {
      /* Something went wrong already */
      (*sslConnectFn)(FALSE, BaseSocket(asock), clientData);
      return ASOCKERR_GENERIC;
   }

   asock->sslConnectFn = sslConnectFn;
   asock->clientData = clientData;

   AsyncTCPSocketSslConnectCallback(asock);
   return ASOCKERR_SUCCESS;
#else
   return ASOCKERR_INVAL;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSslAcceptCallback --
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
AsyncTCPSocketSslAcceptCallback(void *clientData)         // IN
{
   int sslOpCode;
   AsyncTCPSocket *asock = clientData;
   VMwareStatus pollStatus;

   ASSERT(asock);
   ASSERT(AsyncTCPSocketPollParams(asock)->iPoll == NULL);
   ASSERT(AsyncTCPSocketIsLocked(asock));

   AsyncTCPSocketAddRef(asock);

   /* Only set if poll callback is registered */
   asock->sslPollFlags = 0;

   sslOpCode = SSL_TryCompleteAccept(asock->sslSock);
   if (sslOpCode > 0) {
      (*asock->sslAcceptFn)(TRUE, BaseSocket(asock), asock->clientData);
   } else if (sslOpCode < 0) {
      (*asock->sslAcceptFn)(FALSE, BaseSocket(asock), asock->clientData);
   } else {
      asock->sslPollFlags = SSL_WantRead(asock->sslSock) ?
                            POLL_FLAG_READ : POLL_FLAG_WRITE;

      /* register the poll callback to redrive the SSL accept */
      pollStatus = AsyncTCPSocketPollAdd(asock, TRUE, asock->sslPollFlags,
                                      AsyncTCPSocketSslAcceptCallback);

      if (pollStatus != VMWARE_STATUS_SUCCESS) {
         TCPSOCKWARN(asock, ("failed to reinstall ssl accept callback!\n"));
         asock->sslPollFlags = 0;
         (*asock->sslAcceptFn)(FALSE, BaseSocket(asock), asock->clientData);
      }
   }

   AsyncTCPSocketRelease(asock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketStartSslAccept --
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
 *    ASOCKERR_SUCCESS or ASOCKERR_*.
 *    Errors during async processing reported using the callback supplied.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketStartSslAccept(AsyncSocket *base,                  // IN
                             void *sslCtx,                       // IN
                             AsyncSocketSslAcceptFn sslAcceptFn, // IN
                             void *clientData)                   // IN
{
   AsyncTCPSocket *asock = TCPSocket(base);
   Bool ok;

   ASSERT(asock);
   ASSERT(sslAcceptFn);

   ASSERT(AsyncTCPSocketIsLocked(asock));

   if (asock->sslAcceptFn || asock->sslConnectFn) {
      TCPSOCKWARN(asock, ("An SSL operation was already initiated.\n"));
      return ASOCKERR_GENERIC;
   }

   ok = SSL_SetupAcceptWithContext(asock->sslSock, sslCtx);
   if (!ok) {
      /* Something went wrong already */
      (*sslAcceptFn)(FALSE, BaseSocket(asock), clientData);
      return ASOCKERR_GENERIC;
   }

   asock->sslAcceptFn = sslAcceptFn;
   asock->clientData = clientData;

   AsyncTCPSocketSslAcceptCallback(asock);
   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketSetOption --
 *
 *      This implementation of ->setOption() supports the following
 *      options. Exact behavior of each cited optID is documented in the
 *      comment header for that enum value declaration (for non-native options),
 *      or `man setsockopt`/equivalent (for native options).
 *
 *         - layer = SOL_SOCKET, optID =
 *           SO_SNDBUF, SO_RCVBUF.
 *
 *         - layer = IPPROTO_TCP, optID =
 *           TCP_NODELAY, TCP_KEEPINTVL, TCP_KEEPIDLE, TCP_KEEPCNT.
 *
 *         - layer = ASYNC_SOCKET_OPTS_LAYER_BASE, optID (type) =
 *           ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE (Bool).
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *      Invalid option+layer yields ASOCKERR_INVAL.
 *      Failure to set a native OS option yields ASOCKERR_GENERIC.
 *      inBufLen being wrong (for the given option) yields undefined behavior.
 *
 * Side effects:
 *      Depends on option.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketSetOption(AsyncSocket *asyncSocket,     // IN/OUT
                        AsyncSocketOpts_Layer layer,  // IN
                        AsyncSocketOpts_ID optID,     // IN
                        const void *valuePtr,         // IN
                        socklen_t inBufLen)           // IN
{
   /* Maintenance: Keep this in sync with ...GetOption(). */

   AsyncTCPSocket *tcpSocket = TCPSocket(asyncSocket);
   Bool isSupported;

   switch ((int)layer)
   {
   case SOL_SOCKET:
   case IPPROTO_TCP:
   case ASYNC_SOCKET_OPTS_LAYER_BASE:
      break;
   default:
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer [%d] (option [%d]) is not "
                     "supported for TCP socket.\n",
                  __FUNCTION__, (int)layer, optID));
      return ASOCKERR_INVAL;
   }

   /*
    * layer is supported.
    * Handle non-native options first.
    */

   if ((layer == ASYNC_SOCKET_OPTS_LAYER_BASE) &&
       (optID == ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE)) {
      ASSERT(inBufLen == sizeof(Bool));
      tcpSocket->sendLowLatency = *((const Bool *)valuePtr);
      TCPSOCKLG0(tcpSocket,
                 ("%s: sendLowLatencyMode set to [%d].\n",
                  __FUNCTION__, (int)tcpSocket->sendLowLatency));
      return ASOCKERR_SUCCESS;
   }

   /*
    * Handle native (setsockopt()) options from this point on.
    *
    * We need the level and option_name arguments for that call.
    * Our design dictates that, for native options, simply option_name=optID.
    * So just determine level from our layer enum (for native layers, the enum's
    * ordinal value is set to the corresponding int level value). Therefore,
    * level=layer.
    *
    * level and option_name are known. However, we only allow the setting of
    * certain specific options. Anything else is an error.
    */
   isSupported = FALSE;
   if (layer == SOL_SOCKET) {
      switch (optID) {
      case SO_SNDBUF:
      case SO_RCVBUF:
         isSupported = TRUE;
      }
   } else {
      ASSERT((int)layer == IPPROTO_TCP);

      switch (optID) {
         /*
          * Note: All but TCP_KEEPIDLE are available in Mac OS X (at least
          * 10.11). iOS and Android are TBD. For now, let's keep it simple and
          * make all these available in the two known OS where all 3 exist
          * together, as they're typically often set as a group.
          * TODO: Possibly enable for other OS in more fine-grained fashion.
          */
#if defined(__linux__) || defined(VMX86_SERVER)
      case TCP_KEEPIDLE:
      case TCP_KEEPINTVL:
      case TCP_KEEPCNT:
#endif
      case TCP_NODELAY:
         isSupported = TRUE;
      }
   }

   if (!isSupported) {
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer/level [%d], option/name [%d]: "
                     "could not set OS option for TCP socket; "
                     "option not supported.\n",
                  __FUNCTION__, (int)layer, optID));
      return ASOCKERR_INVAL;
   }

   /* All good. Ready to actually set the OS option. */

   if (setsockopt(tcpSocket->fd, layer, optID,
                  valuePtr, inBufLen) != 0) {
      tcpSocket->genericErrno = Err_Errno();
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer/level [%d], option/name [%d]: "
                     "could not set OS option for TCP socket; "
                     "error [%d: %s].\n",
                  __FUNCTION__, (int)layer, optID,
                  tcpSocket->genericErrno,
                  Err_Errno2String(tcpSocket->genericErrno)));
      return ASOCKERR_GENERIC;
   }

   TCPSOCKLG0(tcpSocket,
              ("%s: Option layer/level [%d], option/name [%d]: successfully "
                  "set OS option for TCP socket.\n",
               __FUNCTION__, (int)layer, optID));

   return ASOCKERR_SUCCESS;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncTCPSocketGetOption --
 *
 *      This is the reverse of AsyncTCPSocketSetOption().
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *      Invalid option+layer yields ASOCKERR_INVAL.
 *      Failure to get a native OS option yields ASOCKERR_GENERIC.
 *      *outBufLen being wrong (for the given option) at entry to function
 *      yields undefined behavior.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
AsyncTCPSocketGetOption(AsyncSocket *asyncSocket,     // IN/OUT
                        AsyncSocketOpts_Layer layer,  // IN
                        AsyncSocketOpts_ID optID,     // IN
                        void *valuePtr,               // OUT
                        socklen_t *outBufLen)         // IN/OUT
{
   /*
    * Maintenance: Keep this in sync with ...GetOption().
    * Substantive comments are kept light to avoid redundancy (refer to the
    * other function).
    */

   AsyncTCPSocket *tcpSocket = TCPSocket(asyncSocket);
   Bool isSupported;

   switch ((int)layer) {
   case SOL_SOCKET:
   case IPPROTO_TCP:
   case ASYNC_SOCKET_OPTS_LAYER_BASE:
      break;
   default:
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer [%d] (option [%d]) is not "
                     "supported for TCP socket.\n",
                  __FUNCTION__, (int)layer, optID));
      return ASOCKERR_INVAL;
   }

   if ((layer == ASYNC_SOCKET_OPTS_LAYER_BASE) &&
       (optID == ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE)) {
      ASSERT(*outBufLen >= sizeof(Bool));
      *outBufLen = sizeof(Bool);
      *((Bool *)valuePtr) = tcpSocket->sendLowLatency;
      TCPSOCKLG0(tcpSocket,
                 ("%s: sendLowLatencyMode is [%d].\n",
                  __FUNCTION__, (int)tcpSocket->sendLowLatency));
      return ASOCKERR_SUCCESS;
   }

   isSupported = FALSE;
   if (layer == SOL_SOCKET) {
      switch (optID) {
      case SO_SNDBUF:
      case SO_RCVBUF:
         isSupported = TRUE;
      }
   } else {
      ASSERT((int)layer == IPPROTO_TCP);

      switch (optID) {
#ifdef __linux__
      case TCP_KEEPIDLE:
      case TCP_KEEPINTVL:
      case TCP_KEEPCNT:
#endif
      case TCP_NODELAY:
         isSupported = TRUE;
      }
   }

   if (!isSupported) {
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer/level [%d], option/name [%d]: "
                     "could not get OS option for TCP socket; "
                     "option not supported.\n",
                  __FUNCTION__, (int)layer, optID));
      return ASOCKERR_INVAL;
   }

   if (getsockopt(tcpSocket->fd, layer, optID,
                  valuePtr, outBufLen) != 0) {
      tcpSocket->genericErrno = Err_Errno();
      TCPSOCKLG0(tcpSocket,
                 ("%s: Option layer/level [%d], option/name [%d]: "
                     "could not get OS option for TCP socket; "
                     "error [%d: %s].\n",
                  __FUNCTION__, (int)layer, optID,
                  tcpSocket->genericErrno,
                  Err_Errno2String(tcpSocket->genericErrno)));
      return ASOCKERR_GENERIC;
   }

   TCPSOCKLG0(tcpSocket,
              ("%s: Option layer/level [%d], option/name [%d]: successfully "
                  "got OS option for TCP socket.\n",
               __FUNCTION__, (int)layer, optID));

   return ASOCKERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketDestroy --
 *
 *    Free the AsyncTCPSocket struct and all of its child storage.
 *
 * Result
 *    None
 *
 * Side-effects
 *    Releases memory.
 *
 *-----------------------------------------------------------------------------
 */

static void
AsyncTCPSocketDestroy(AsyncSocket *base)         // IN/OUT
{
   free(base);
}


#ifndef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_ListenSocketUDS --
 *
 *      Listens on the specified unix domain socket, and accepts new
 *      socket connections. Fires the connect callback with new
 *      AsyncTCPSocket object for each connection.
 *
 * Results:
 *      New AsyncTCPSocket in listening state or NULL on error
 *
 * Side effects:
 *      Creates new Unix domain socket, binds and listens.
 *
 *-----------------------------------------------------------------------------
 */

AsyncSocket *
AsyncSocket_ListenSocketUDS(const char *pipeName,               // IN
                            AsyncSocketConnectFn connectFn,     // IN
                            void *clientData,                   // IN
                            AsyncSocketPollParams *pollParams,  // IN
                            int *outError)                      // OUT
{
   struct sockaddr_un addr;
   AsyncTCPSocket *asock;

   memset(&addr, 0, sizeof addr);
   addr.sun_family = AF_UNIX;
   Str_Strcpy(addr.sun_path, pipeName, sizeof addr.sun_path);

   Log(ASOCKPREFIX "creating new socket listening on %s\n", pipeName);

   asock = AsyncTCPSocketListenImpl((struct sockaddr_storage *)&addr,
                                    sizeof addr, connectFn, clientData,
                                    pollParams, outError);

   return BaseSocket(asock);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncTCPSocketListenerError --
 *
 *    Call the error handler from parent AsyncSocket object. The passed in
 *    parameter clientData is the parent AsyncSocket object.
 *
 * Result
 *    None
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
AsyncTCPSocketListenerError(int error,           // IN
                            AsyncSocket *asock,  // IN
                            void *clientData)    // IN
{
   AsyncSocket *s = clientData;
   ASSERT(s);

   AsyncSocketHandleError(s, error);
}

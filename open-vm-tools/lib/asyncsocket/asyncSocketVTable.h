/*********************************************************
 * Copyright (C) 2011,2014-2017,2019-2021 VMware, Inc. All rights reserved.
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

#ifndef __ASYNC_SOCKET_VTABLE_H__
#define __ASYNC_SOCKET_VTABLE_H__

#ifdef USE_SSL_DIRECT
#include "sslDirect.h"
#else
#include "ssl.h"
#endif

/*
 * If we change the AsyncSocketVTable, we also need to change the follow files:
 * lib/blastSockets/asyncProxySocket.c
 * lib/asyncsocket/asyncsocket.c
 * lib/asyncsocket/asyncWebSocket.c
 * lib/asyncsocket/asyncNamedPipe.c
 * lib/udpfec/fecAsyncSocket.c
 * lib/udpfec/fecAsyncSslSocket.c
 * devices/vsock/asyncVmciSocket.c
 */
typedef struct AsyncSocketVTable {
   AsyncSocketState (*getState)(AsyncSocket *sock);

   /*
    * The socket options mechanism is discussed in asyncsocket.h.
    * If you're considering adding a new virtual function table entry whose
    * effect is to call setsockopt() and/or save a value inside the socket
    * structure and/or forward such a call to a contained AsyncSocket,
    * strongly consider using this setOption() mechanism instead.
    * Your life is likely to be made easier by this.
    */
   int (*setOption)(AsyncSocket *asyncSocket,
                    AsyncSocketOpts_Layer layer,
                    AsyncSocketOpts_ID optID,
                    const void *valuePtr,
                    socklen_t inBufLen);
   /*
    * A setOption() implementation must have a symmetrical getOption()
    * counterpart.  The converse is not true -- a getOption()
    * implementation need not have a setOption() counterpart.  (One
    * way to look at this is that an option may be read-only, but it
    * must not be write-only.)
    */
   int (*getOption)(AsyncSocket *asyncSocket,
                    AsyncSocketOpts_Layer layer,
                    AsyncSocketOpts_ID optID,
                    void *valuePtr,
                    socklen_t *outBufLen);

   int (*getGenericErrno)(AsyncSocket *s);
   int (*getFd)(AsyncSocket *asock);
   int (*getRemoteIPStr)(AsyncSocket *asock, const char **ipStr);
   int (*getRemotePort)(AsyncSocket *asock, uint32 *port);
   int (*getINETIPStr)(AsyncSocket *asock, int socketFamily, char **ipRetStr);
   unsigned int (*getPort)(AsyncSocket *asock);
   int (*setCloseOptions)(AsyncSocket *asock, int flushEnabledMaxWaitMsec,
                           AsyncSocketCloseFn closeCb);
   Bool (*connectSSL)(AsyncSocket *asock, struct _SSLVerifyParam *verifyParam,
                      const char *hostname, void *sslContext);
   int (*startSslConnect)(AsyncSocket *asock,
                           struct _SSLVerifyParam *verifyParam,
                           const char *hostname, void *sslCtx,
                           AsyncSocketSslConnectFn sslConnectFn,
                           void *clientData);
   Bool (*acceptSSL)(AsyncSocket *asock, void *sslCtx);
   int (*startSslAccept)(AsyncSocket *asock, void *sslCtx,
                          AsyncSocketSslAcceptFn sslAcceptFn,
                          void *clientData);
   int (*flush)(AsyncSocket *asock, int timeoutMS);
   int (*recv)(AsyncSocket *asock, void *buf, int len, Bool partial, void *cb,
               void *cbData);
   int (*recvPassedFd)(AsyncSocket *asock, void *buf, int len, void *cb,
                       void *cbData);
   int (*getReceivedFd)(AsyncSocket *asock);
   int (*send)(AsyncSocket *asock, void *buf, int len,
               AsyncSocketSendFn sendFn, void *clientData);
   int (*isSendBufferFull)(AsyncSocket *asock);
   int (*getNetworkStats)(AsyncSocket *asock,
                          AsyncSocketNetworkStats *stats);
   int (*close)(AsyncSocket *asock);
   int (*cancelRecv)(AsyncSocket *asock, int *partialRecvd, void **recvBuf,
                     void **recvFn, Bool cancelOnSend);
   int (*cancelCbForClose)(AsyncSocket *asock);
   int (*getLocalVMCIAddress)(AsyncSocket *asock, uint32 *cid, uint32 *port);
   int (*getRemoteVMCIAddress)(AsyncSocket *asock, uint32 *cid, uint32 *port);
   int (*getWebSocketError)(AsyncSocket *asock);
   char *(*getWebSocketURI)(AsyncSocket *asock);
   char *(*getWebSocketCookie)(AsyncSocket *asock);
   uint16 (*getWebSocketCloseStatus)(AsyncSocket *asock);
   const char *(*getWebSocketProtocol)(AsyncSocket *asock);
   int (*setWebSocketCookie)(AsyncSocket *asock, void *clientData,
                             const char *path, const char *sessionId);
   int (*setDelayWebSocketUpgradeResponse)(AsyncSocket *asock,
                                           Bool delayWebSocketUpgradeResponse);
   int (*recvBlocking)(AsyncSocket *s, void *buf, int len, int *received,
                      int timeoutMS);
   int (*recvPartialBlocking)(AsyncSocket *s, void *buf, int len,
                              int *received, int timeoutMS);
   int (*sendBlocking)(AsyncSocket *s, void *buf, int len, int *sent,
                       int timeoutMS);
   int (*doOneMsg)(AsyncSocket *s, Bool read, int timeoutMS);
   int (*waitForConnection)(AsyncSocket *s, int timeoutMS);
   int (*waitForReadMultiple)(AsyncSocket **asock, int numSock, int timeoutMS,
                              int *outIdx);
   int (*peek)(AsyncSocket *asock, void *buf, int len, void *cb, void *cbData);

   /*
    * Internal function, called when refcount drops to zero:
    */
   void (*destroy)(AsyncSocket *asock);
} AsyncSocketVTable;


#define VT(x) ((x)->vt)
#define VALID(asock, x) LIKELY(asock && VT(asock)->x)


#endif

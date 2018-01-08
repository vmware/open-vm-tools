/*********************************************************
 * Copyright (C) 2011,2014-2016 VMware, Inc. All rights reserved.
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

#ifndef __ASYNC_SOCKET_BASE_H__
#define __ASYNC_SOCKET_BASE_H__

#ifdef USE_SSL_DIRECT
#include "sslDirect.h"
#else
#include "ssl.h"
#endif

#include "asyncSocketVTable.h"

/*
 * The abstract base class for all asyncsocket implementations.
 */
struct AsyncSocket {
   uint32 id;
   uint32 refCount;
   AsyncSocketPollParams pollParams;
   AsyncSocketState state;

   Bool inited;
   Bool errorSeen;
   AsyncSocketErrorFn errorFn;
   void *errorClientData;

   void *recvBuf;
   int recvPos;
   int recvLen;
   AsyncSocketRecvFn recvFn;
   void *recvClientData;
   Bool recvFireOnPartial;

   const AsyncSocketVTable *vt;
};

void AsyncSocketInitSocket(AsyncSocket *asock,
                           AsyncSocketPollParams *params,
                           const AsyncSocketVTable *vtable);
void AsyncSocketTeardownSocket(AsyncSocket *s);

void AsyncSocketLock(AsyncSocket *asock);
void AsyncSocketUnlock(AsyncSocket *asock);
Bool AsyncSocketIsLocked(AsyncSocket *asock);
void AsyncSocketAddRef(AsyncSocket *asock);
void AsyncSocketRelease(AsyncSocket *s);
AsyncSocketState AsyncSocketGetState(AsyncSocket *sock);
void AsyncSocketSetState(AsyncSocket *sock, AsyncSocketState state);
int AsyncSocketSetRecvBuf(AsyncSocket *asock, void *buf, int len,
                           Bool fireOnPartial, void *cb, void *cbData);
Bool AsyncSocketCheckAndDispatchRecv(AsyncSocket *s, int *result);
AsyncSocketPollParams *AsyncSocketGetPollParams(AsyncSocket *s);
void AsyncSocketHandleError(AsyncSocket *asock, int asockErr);
void AsyncSocketCancelRecv(AsyncSocket *asock, int *partialRecvd,
                           void **recvBuf, void **recvFn);


int AsyncTCPSocket_Init(void);

#endif

/*********************************************************
 * Copyright (C) 2016-2021 VMware, Inc. All rights reserved.
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
 * asyncSocketBase.c --
 *
 *      This exposes the public functions of the AsyncSocket library.
 *      This file itself just contains stubs which call the function
 *      pointers in the socket's virtual table.
 *
 */

#include "vmware.h"
#include "asyncsocket.h"
#include "asyncSocketBase.h"
#include "msg.h"
#include "log.h"

#define LOGLEVEL_MODULE asyncsocket
#include "loglevel_user.h"

/*
 * A version of ASOCKLOG() which is safe to call from inside IncRef,
 * DecRef or any of the other functions which the regular ASOCKLOG()
 * implicitly calls.  We don't log fd as that isn't available at the
 * base class level.
 */

/* gcc needs special syntax to handle zero-length variadic arguments */
#if defined(_MSC_VER)
#define ASOCKLOG_NORECURSION(_level, _asock, fmt, ...)               \
   do {                                                              \
      if (((_level) == 0) || DOLOG_BYNAME(asyncsocket, (_level))) {  \
         Log(ASOCKPREFIX "%d " fmt, (_asock)->id, __VA_ARGS__);      \
      }                                                              \
   } while(0)
#else
#define ASOCKLOG_NORECURSION(_level, _asock, fmt, ...)               \
   do {                                                              \
      if (((_level) == 0) || DOLOG_BYNAME(asyncsocket, (_level))) {  \
         Log(ASOCKPREFIX "%d " fmt, (_asock)->id, ##__VA_ARGS__);    \
      }                                                              \
   } while(0)
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketInternalIncRef --
 *
 *    Increments reference count on AsyncSocket struct and optionally
 *    takes the lock.  This function is used to implement both Lock
 *    and AddRef.
 *
 * Results:
 *    New reference count.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
AsyncSocketInternalIncRef(AsyncSocket *asock,   // IN
                          Bool lock)            // IN
{
   if (lock && asock->pollParams.lock) {
      MXUser_AcquireRecLock(asock->pollParams.lock);
   }
   ASSERT(asock->refCount > 0);
   ++asock->refCount;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketInternalDecRef --
 *
 *    Decrements reference count on AsyncSocket struct, freeing it when it
 *    reaches 0.  If "unlock" is TRUE, releases the lock after decrementing
 *    the count.
 *
 *    This function is used to implement both Unlock and DecRef.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May free struct.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
AsyncSocketInternalDecRef(AsyncSocket *s, // IN
                          Bool unlock)    // IN
{
   int count = --s->refCount;

   if (unlock && s->pollParams.lock) {
      MXUser_ReleaseRecLock(s->pollParams.lock);
   }

   ASSERT(count >= 0);
   if (UNLIKELY(count == 0)) {
      ASOCKLOG_NORECURSION(1, s, "Final release; freeing asock struct\n");
      VT(s)->destroy(s);
   } else {
      ASOCKLOG_NORECURSION(1, s, "Release (count now %d)\n", count);
   }
}


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

void
AsyncSocketLock(AsyncSocket *asock)   // IN:
{
   AsyncSocketInternalIncRef(asock, TRUE);
}

void
AsyncSocketUnlock(AsyncSocket *asock)   // IN:
{
   AsyncSocketInternalDecRef(asock, TRUE);
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

Bool
AsyncSocketIsLocked(AsyncSocket *asock)   // IN:
{
   if (asock->pollParams.lock && Poll_LockingEnabled()) {
      return MXUser_IsCurThreadHoldingRecLock(asock->pollParams.lock);
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketAddRef --
 *
 *    Increments reference count on AsyncSocket struct.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocketAddRef(AsyncSocket *s)         // IN
{
   AsyncSocketInternalIncRef(s, FALSE);
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
 *    None.
 *
 * Side effects:
 *    May free struct.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocketRelease(AsyncSocket *s)  // IN:
{
   AsyncSocketInternalDecRef(s, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketGetState --
 *
 *      Accessor function for the state in the base class.
 *
 *----------------------------------------------------------------------------
 */

AsyncSocketState
AsyncSocketGetState(AsyncSocket *asock)         // IN
{
   ASSERT(AsyncSocketIsLocked(asock));
   return asock->state;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSetState --
 *
 *      Modifier function for the state in the base class.
 *
 *----------------------------------------------------------------------------
 */

void
AsyncSocketSetState(AsyncSocket *asock,         // IN/OUT
                    AsyncSocketState state)     // IN
{
   asock->state = state;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketGetPollParams --
 *
 *    Accessor function for the pollParams struct in the base socket.
 *
 *-----------------------------------------------------------------------------
 */

AsyncSocketPollParams *
AsyncSocketGetPollParams(AsyncSocket *s)         // IN
{
   return &s->pollParams;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketInitSocket --
 *
 *    Initialize the AsyncSocket base struct.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocketInitSocket(AsyncSocket *s,                          // IN/OUT
                      AsyncSocketPollParams *pollParams,       // IN
                      const AsyncSocketVTable *vtable)         // IN
{
   /*
    * The sockets each have a "unique" ID, which is just an
    * incrementing integer.
    */
   static Atomic_uint32 nextid = { 1 };

   s->id = Atomic_ReadInc32(&nextid);
   s->refCount = 1;
   s->vt = vtable;
   s->inited = TRUE;
   if (pollParams) {
      s->pollParams = *pollParams;
   } else {
      s->pollParams.pollClass = POLL_CS_MAIN;
      s->pollParams.flags = 0;
      s->pollParams.lock = NULL;
      s->pollParams.iPoll = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketTeardownSocket --
 *
 *    Tear down the AsyncSocket base struct.  Currently this just
 *    clears the inited flag and releases the initial (user) refcount.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocketTeardownSocket(AsyncSocket *asock)         // IN/OUT
{
   /*
    * Release the initial refcount created when we initialize the
    * socket struct.
    */
   ASSERT(AsyncSocketIsLocked(asock));
   ASSERT(asock->refCount >= 1);
   ASSERT(asock->inited);
   asock->inited = FALSE;
   AsyncSocketRelease(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Init --
 *
 *      Initialize the various socket subsytems.  Currently just TCP, this
 *      will expand.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_GENERIC.
 *
 * Side effects:
 *      See subsystems.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Init(void)
{
   return AsyncTCPSocket_Init();
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
AsyncSocket_GetID(AsyncSocket *asock)         // IN
{
   if (!asock) {
      return ASOCKERR_INVAL;    /* For some reason we return ID 5
                                   for null pointers! */
   } else {
      return asock->id;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetErrorFn --
 *
 *      Sets the error handling function for the asock. The error function
 *      is invoked automatically on I/O errors. This should be done
 *      before an internal callback that may call the error handler can be
 *      fired. This usually means doing so immediately after the asyncsocket
 *      is created, either from the poll thread or with the asyncsocket lock
 *      (passed in pollParams) held throughout both calls.
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
      return ASOCKERR_INVAL;
   } else {
      AsyncSocketLock(asock);
      asock->errorFn = errorFn;
      asock->errorClientData = clientData;
      AsyncSocketUnlock(asock);
      return ASOCKERR_SUCCESS;
   }
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
AsyncSocketHandleError(AsyncSocket *asock,   // IN
                       int asockErr)         // IN
{
   ASSERT(asock);
   asock->errorSeen = TRUE;
   if (asock->errorFn) {
      ASOCKLOG(3, asock, "firing error callback (%s)\n",
               AsyncSocket_Err2String(asockErr));
      asock->errorFn(asockErr, asock, asock->errorClientData);
   } else {
      ASOCKLOG(3, asock, "no error callback, closing socket (%s)\n",
               AsyncSocket_Err2String(asockErr));
      AsyncSocket_Close(asock);
   }
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
   ASSERT(s->recvPos > 0);
   ASSERT(s->recvPos <= s->recvLen);

   /*
    * The application may close the socket in this callback.  This
    * asserts that even if that happens, the socket will not be
    * immediately freed in the middle of our function.
    */
   ASSERT(s->refCount > 1);

   if (s->recvPos == s->recvLen || s->recvFireOnPartial) {
      void *recvBuf = s->recvBuf;
      ASOCKLOG(3, s, "recv buffer full, calling recvFn\n");

      /*
       * We do this dance in case the handler frees the buffer (so
       * that there's no possible window where there are dangling
       * references here.  Obviously if the handler frees the buffer,
       * but then fails to register a new one, we'll put back the
       * dangling reference in the automatic reset case below, but
       * there's currently a limit to how far we go to shield clients
       * who use our API in a broken way.
       */

      s->recvBuf = NULL;
      s->recvFn(recvBuf, s->recvPos, s, s->recvClientData);
      if (s->state == AsyncSocketClosed) {
         ASOCKLG0(s, "owner closed connection in recv callback\n");
         *result = ASOCKERR_CLOSED;
         return TRUE;
      } else if (s->recvFn == NULL && s->recvLen == 0) {
         /*
          * Further recv is cancelled from within the last recvFn, see
          * AsyncSocket_CancelRecv(). So exit from the loop.
          */
         *result = ASOCKERR_SUCCESS;
         return TRUE;
      } else if (s->recvPos > 0) {
         /*
          * Automatically reset keeping the current handler.  Checking
          * that recvPos is still non-zero implies that the
          * application has not called AsyncSocket_Recv or
          * _RecvPartial in the callback.
          */
         s->recvPos = 0;
         s->recvBuf = recvBuf;
         *result = ASOCKERR_SUCCESS;
         return FALSE;
      } else {
         *result = ASOCKERR_SUCCESS;
         return FALSE;
      }
   } else {
      *result = ASOCKERR_SUCCESS;
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocketSetRecvBuf --
 *
 *      Helper function to validate socket state and recvBuf
 *      parameters before setting the recvBuf values in the base
 *      class.
 *
 * Results:
 *      ASOCKERR_SUCCESS or ASOCKERR_*.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocketSetRecvBuf(AsyncSocket *asock,  // IN:
                      void *buf,           // IN:
                      int len,             // IN:
                      Bool fireOnPartial,  // IN:
                      void *cb,            // IN:
                      void *cbData)        // IN:
{
   ASSERT(AsyncSocketIsLocked(asock));

   if (!asock->errorFn) {
      ASOCKWARN(asock, "%s: no registered error handler!\n", __FUNCTION__);
      return ASOCKERR_INVAL;
   }

   if (!buf || !cb || len <= 0) {
      ASOCKWARN(asock, "Recv called with invalid arguments!\n");
      return ASOCKERR_INVAL;
   }

   if (AsyncSocketGetState(asock) != AsyncSocketConnected) {
      ASOCKWARN(asock, "recv called but state is not connected!\n");
      return ASOCKERR_NOTCONNECTED;
   }

   if (asock->recvBuf && asock->recvPos != 0) {
      ASOCKWARN(asock, "Recv called -- partially read buffer discarded.\n");
   }

   asock->recvBuf = buf;
   asock->recvLen = len;
   asock->recvFireOnPartial = fireOnPartial;
   asock->recvFn = cb;
   asock->recvClientData = cbData;
   asock->recvPos = 0;

   return ASOCKERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocketCancelRecv --
 *
 *    Call this function if you know what you are doing. This should
 *    be called if you want to synchronously receive the outstanding
 *    data on the socket.  It returns number of partially read bytes
 *    (if any). A partially read response may exist as
 *    AsyncSocketRecvCallback calls the recv callback only when all
 *    the data has been received.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Subsequent client call to AsyncSocket_Recv can reinstate async behaviour.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocketCancelRecv(AsyncSocket *asock,         // IN
                      int *partialRecvd,          // OUT
                      void **recvBuf,             // OUT
                      void **recvFn)              // IN
{
   if (partialRecvd) {
      *partialRecvd = asock->recvPos;
   }
   if (recvFn) {
      *recvFn = asock->recvFn;
   }
   if (recvBuf) {
      *recvBuf = asock->recvBuf;
   }

   asock->recvBuf = NULL;
   asock->recvFn = NULL;
   asock->recvPos = 0;
   asock->recvLen = 0;
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
      result = MSGID(asyncsocket.remotedisconnect) "Remote disconnected";
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
   case ASOCKERR_CONNECTSSL:
      result = MSGID(asyncsocket.connectssl) "Connection error: could not negotiate SSL";
      break;
   case ASOCKERR_NETUNREACH:
      result = MSGID(asyncsocket.netunreach) "Network unreachable";
      break;
   case ASOCKERR_ADDRUNRESV:
      result = MSGID(asyncsocket.addrunresv) "Address unresolvable";
      break;
   case ASOCKERR_BUSY:
      result = MSGID(asyncsocket.busy) "Concurrent operations on socket";
      break;
   case ASOCKERR_PROXY_NEEDS_AUTHENTICATION:
      result = MSGID(asyncsocket.proxyneedsauthentication)
                     "Proxy needs authentication";
      break;
   case ASOCKERR_PROXY_CONNECT_FAILED:
      result = MSGID(asyncsocket.proxyconnectfailed)
                     "Connection failed through proxy";
      break;
   case ASOCKERR_PROXY_INVALID_OR_NOT_SUPPORTED:
      result = MSGID(asyncsocket.proxyinvalidornotsupported)
                     "Invalid or not supported type proxy";
      break;
   case ASOCKERR_WEBSOCK_UPGRADE_NOT_FOUND:
      result = MSGID(asyncsocket.websocketupgradefailed)
                     "Upgrade to websocket error: NOT FOUND, status code 404";
      break;
   case ASOCKERR_WEBSOCK_TOO_MANY_CONNECTION:
      result = MSGID(asyncsocket.websockettoomanyconnection)
                     "The server-side WebSocket connection limit has been exceeded,"
                     " HTTP status code 429";
      break;
   }

   if (!result) {
      Warning("%s was passed bad code %d\n", __FUNCTION__, asyncSockError);
      result = MSGID(asyncsocket.unknown) "Unknown error";
   }
   return result;
}


/**
 *-----------------------------------------------------------------------------
 *
 * stristr --
 *
 *    Do you know strstr from <string.h>?
 *    So this one is the same, but without the case sensitivity.
 *
 * Results:
 *    return a pointer to the first occurrence of needle in haystack,
 *    or NULL if needle does not appear in haystack. If needle is zero
 *    length, the function returns haystack.
 *
 * Side effects:
 *    none
 *
 *-----------------------------------------------------------------------------
 */

const char *
stristr(const char *haystack,         // IN
        const char *needle)           // IN
{
   if (*needle) {
      int len = strlen(needle);
      for (; *haystack; haystack++) {
         if (strncasecmp(haystack, needle, len) == 0) {
            return haystack;
         }
      }
      return NULL;
   } else {
      return haystack;
   }
}

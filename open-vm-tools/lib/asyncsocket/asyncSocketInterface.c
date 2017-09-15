/*********************************************************
 * Copyright (C) 2016 VMware, Inc. All rights reserved.
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
 * asyncSocketInterface.c --
 *
 *      This exposes the public functions of the AsyncSocket library.
 *      This file itself just contains stubs which call the function
 *      pointers in the socket's virtual table.
 *
 * Which entrypoints are virtual and which are base functionality?
 * Guidelines:
 * - functions affecting the underlying transport (e.g. TCP timeouts)
 *   are backend-specific and generally ARE virtualized.
 * - functions with an immediate effect (e.g. queue bytes for send)
 *   generally ARE virtualized.
 * - functions affecting the socket abstraction (e.g. how it reports errors
 *   to the caller) are basic functionality and generally are NOT virtualized.
 * - functions affecting state which is queried later (e.g. close behavior)
 *   generally are NOT virtualized.
 */

#include "asyncsocket.h"
#include "asyncSocketInt.h"


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
   if (!asock) {
      return ASOCKERR_INVAL;
   }
   ASSERT(asock->vt->getState);
   return asock->vt->getState(asock);
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
AsyncSocket_GetGenericErrno(AsyncSocket *asock)  // IN:
{
   ASSERT(asock);
   ASSERT(asock->vt->getGenericErrno);
   return asock->vt->getGenericErrno(asock);
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
AsyncSocket_GetFd(AsyncSocket *asock)
{
   ASSERT(asock->vt->getFd);
   return asock->vt->getFd(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetRemoteIPStr --
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
AsyncSocket_GetRemoteIPStr(AsyncSocket *asock,      // IN
                           const char **ipRetStr)   // OUT
{
   ASSERT(asock);
   ASSERT(asock->vt->getRemoteIPStr);
   return asock->vt->getRemoteIPStr(asock, ipRetStr);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetINETIPStr --
 *
 *      Given an AsyncSocket object, returns the IP addresses associated with
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

int
AsyncSocket_GetINETIPStr(AsyncSocket *asock,  // IN
                         int socketFamily,    // IN
                         char **ipRetStr)     // OUT
{
   ASSERT(asock->vt->getINETIPStr);
   return asock->vt->getINETIPStr(asock, socketFamily, ipRetStr);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetPort --
 *
 *      Given an AsyncSocket object, returns the port number associated with
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

unsigned int
AsyncSocket_GetPort(AsyncSocket *asock)  // IN
{
   ASSERT(asock->vt->getPort);
   return asock->vt->getPort(asock);
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
   ASSERT(asock->vt->useNodelay);
   return asock->vt->useNodelay(asock, nodelay);
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
   ASSERT(asock->vt->setTCPTimeouts);
   return asock->vt->setTCPTimeouts(asock, keepIdle, keepIntvl, keepCnt);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_SetBufferSizes --
 *
 *    Set socket level recv/send buffer sizes if they are less than given sizes
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
   if (!asock) {
      return FALSE;
   }
   ASSERT(asock->vt->setBufferSizes);
   return asock->vt->setBufferSizes(asock, sendSz, recvSz);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_SetSendLowLatencyMode --
 *
 *    Put the socket into a mode where we attempt to issue sends
 *    directly from within AsyncSocket_Send().  Ordinarily, we would
 *    set up a Poll callback from within AsyncSocket_Send(), which
 *    introduces some non-zero latency to the send path.  In
 *    low-latency-send mode, that delay is potentially avoided.  This
 *    does introduce a behavioural change; the send completion
 *    callback may be triggered before the call to Send() returns.  As
 *    not all clients may be expecting this, we don't enable this mode
 *    unless requested by the client.
 *
 * Result
 *    None
 *
 * Side-effects
 *    See description above.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocket_SetSendLowLatencyMode(AsyncSocket *asock,  // IN
                                  Bool enable)         // IN
{
   ASSERT(asock->vt->setSendLowLatencyMode);
   asock->vt->setSendLowLatencyMode(asock, enable);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_StartSslConnect --
 *
 *    Start an asynchronous SSL connect operation.
 *
 *    The supplied callback function is called when the operation is complete
 *    or an error occurs. The caller should only free the verifyParam argument
 *    after the sslConnectFn callback is called.
 *
 * Results:
 *    None.
 *    Error is always reported using the callback supplied. Detailed SSL
 *    verification error can be retrieved from verifyParam structure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
AsyncSocket_StartSslConnect(AsyncSocket *asock,                   // IN
                            SSLVerifyParam *verifyParam,          // IN/OPT
                            void *sslCtx,                         // IN
                            AsyncSocketSslConnectFn sslConnectFn, // IN
                            void *clientData)                     // IN
{
   ASSERT(asock);
   ASSERT(asock->vt->startSslConnect);
   asock->vt->startSslConnect(asock, verifyParam, sslCtx, sslConnectFn,
                              clientData);
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
                       SSLVerifyParam *verifyParam, // IN/OPT
                       void *sslContext)            // IN/OPT
{
   ASSERT(asock);
   ASSERT(asock->vt->connectSSL);
   return asock->vt->connectSSL(asock, verifyParam, sslContext);
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
   ASSERT(asock->vt->acceptSSL);
   return asock->vt->acceptSSL(asock);
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
   ASSERT(asock);
   ASSERT(asock->vt->startSslAccept);
   asock->vt->startSslAccept(asock, sslCtx, sslAcceptFn, clientData);
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
AsyncSocket_Flush(AsyncSocket *asock,  // IN
                  int timeoutMS)       // IN
{
   if (asock == NULL) {
      Warning(ASOCKPREFIX "Flush called with invalid arguments!\n");
      return ASOCKERR_INVAL;
   }
   ASSERT(asock->vt->flush);
   return asock->vt->flush(asock, timeoutMS);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_Recv --
 * AsyncSocket_RecvPartial --
 *
 *      Registers a callback that will fire once the specified amount of data
 *      has been received on the socket.
 *
 *      In the case of AsyncSocket_RecvPartial, the callback is fired
 *      once all or part of the data has been received on the socket.
 *
 *      TCP usage:
 *      AsyncSocket_Recv(AsyncSocket *asock,
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

int
AsyncSocket_Recv(AsyncSocket *asock,
                 void *buf,
                 int len,
                 void *cb,
                 void *cbData)
{
   ASSERT(asock->vt->recv);
   if (!asock) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");
      return ASOCKERR_INVAL;
   }

   return asock->vt->recv(asock, buf, len, FALSE, cb, cbData);
}

int
AsyncSocket_RecvPartial(AsyncSocket *asock,
                        void *buf,
                        int len,
                        void *cb,
                        void *cbData)
{
   ASSERT(asock->vt->recv);
   return asock->vt->recv(asock, buf, len, TRUE, cb, cbData);
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
   if (!asock) {
      Warning(ASOCKPREFIX "Recv called with invalid arguments!\n");
      return ASOCKERR_INVAL;
   }

   ASSERT(asock->vt->recvPassedFd);
   return asock->vt->recvPassedFd(asock, buf, len, cb, cbData);
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
   if (!asock) {
      Warning(ASOCKPREFIX "Invalid socket while receiving fd!\n");
      return -1;
   }

   ASSERT(asock->vt->getReceivedFd);
   return asock->vt->getReceivedFd(asock);
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
   if (!asock || !buf || len <= 0) {
      Warning(ASOCKPREFIX "Send called with invalid arguments! asynchSock: %p "
              "buffer: %p length: %d\n", asock, buf, len);
      return ASOCKERR_INVAL;
   }
   ASSERT(asock->vt->send);
   return asock->vt->send(asock, buf, len, sendFn, clientData);
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
   if (!asock) {
      return ASOCKERR_GENERIC;
   }
   ASSERT(asock->vt->isSendBufferFull);
   return asock->vt->isSendBufferFull(asock);
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
   if (!asock) {
      return ASOCKERR_INVAL;
   }
   ASSERT(asock->vt->close);
   return asock->vt->close(asock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_CancelRecv --
 * AsyncSocket_CancelRecvEx --
 *
 *    Call this function if you know what you are doing. This should be
 *    called if you want to synchronously receive the outstanding data on
 *    the socket. It removes the recv poll callback. It also returns number of
 *    partially read bytes (if any). A partially read response may exist as
 *    AsyncSocketRecvCallback calls the recv callback only when all the data
 *    has been received.
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
   if (!asock) {
      Warning(ASOCKPREFIX "Invalid socket while cancelling recv request!\n");
      return ASOCKERR_INVAL;
   }
   ASSERT(asock->vt->cancelRecv);
   return asock->vt->cancelRecv(asock, partialRecvd, recvBuf, recvFn,
                                cancelOnSend);
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
   ASSERT(asock->vt->cancelCbForClose);
   asock->vt->cancelCbForClose(asock);
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
   ASSERT(asock->vt->getLocalVMCIAddress);
   return asock->vt->getLocalVMCIAddress(asock, cid, port);
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
   ASSERT(asock->vt->getRemoteVMCIAddress);
   return asock->vt->getRemoteVMCIAddress(asock, cid, port);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetWebSocketURI --
 *
 *      Return the URI supplied during a WebSocket connection request.
 *
 * Results:
 *      URI or Null if no URI was specified.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

char *
AsyncSocket_GetWebSocketURI(AsyncSocket *asock)    // IN
{
   ASSERT(asock);
   ASSERT(asock->vt->getWebSocketURI);
   return asock->vt->getWebSocketURI(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetWebSocketCookie --
 *
 *      Return the Cookie field value supplied during a WebSocket
 *      connection request.
 *
 * Results:
 *      Cookie, if asock is WebSocket.
 *      NULL, if asock is not WebSocket.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

char *
AsyncSocket_GetWebSocketCookie(AsyncSocket *asock)    // IN
{
   ASSERT(asock);
   if (asock->vt->getWebSocketCookie) {
      return asock->vt->getWebSocketCookie(asock);
   }
   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetWebSocketCloseStatus --
 *
 *      Retrieve the close status, if received, for a websocket connection.
 *
 * Results:
 *      Websocket close status code (>= 1000), or 0 if never received.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

uint16
AsyncSocket_GetWebSocketCloseStatus(const AsyncSocket *asock)   // IN
{
   ASSERT(asock);
   ASSERT(asock->vt->getWebSocketCloseStatus);
   return asock->vt->getWebSocketCloseStatus(asock);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetWebSocketProtocol --
 *
 *      Return the negotiated websocket protocol.  Only valid until asock is
 *      destroyed.
 *
 * Results:
 *      NULL, if asock is not WebSocket.
 *      AsyncWebSocketProtocol *, if asock is WebSocket.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

const char *
AsyncSocket_GetWebSocketProtocol(AsyncSocket *asock)  // IN
{
   ASSERT(asock);
   if (asock->vt->getWebSocketProtocol) {
      return asock->vt->getWebSocketProtocol(asock);
   }
   return NULL;
}

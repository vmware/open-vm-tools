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

#include "vmware.h"
#include "asyncsocket.h"
#include "asyncSocketBase.h"
#include "msg.h"
#include "log.h"

#define LOGLEVEL_MODULE asyncsocket
#include "loglevel_user.h"




/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetCloseOptions --
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
 *      ASOCKERR_SUCCESS or ASOCKERR_*.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetCloseOptions(AsyncSocket *asock,           // IN
                            int flushEnabledMaxWaitMsec,  // IN
                            AsyncSocketCloseFn closeCb)   // IN
{
   int ret;
   if (VALID(asock, setCloseOptions)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setCloseOptions(asock, flushEnabledMaxWaitMsec, closeCb);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
AsyncSocket_GetState(AsyncSocket *asock)         // IN
{
   AsyncSocketState ret;
   if (VALID(asock, getState)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getState(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getGenericErrno)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getGenericErrno(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = -1;
   }
   return ret;
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
AsyncSocket_GetFd(AsyncSocket *asock)         // IN
{
   int ret;
   if (VALID(asock, getFd)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getFd(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = -1;
   }
   return ret;
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
 *      ASOCKERR_SUCCESS or ASOCKERR_INVAL.
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
   int ret;
   if (VALID(asock, getRemoteIPStr)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getRemoteIPStr(asock, ipRetStr);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getINETIPStr)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getINETIPStr(asock, socketFamily, ipRetStr);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getPort)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getPort(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = MAX_UINT32;
   }
   return ret;
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
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
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
   int ret;
   if (VALID(asock, useNodelay)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->useNodelay(asock, nodelay);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetTCPTimeouts(AsyncSocket *asock,  // IN/OUT:
                           int keepIdle,        // IN
                           int keepIntvl,       // IN
                           int keepCnt)         // IN
{
   int ret;
   if (VALID(asock, setTCPTimeouts)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setTCPTimeouts(asock, keepIdle, keepIntvl, keepCnt);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


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
   Bool ret;
   if (VALID(asock, setBufferSizes)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setBufferSizes(asock, sendSz, recvSz);
      AsyncSocketUnlock(asock);
   } else {
      ret = FALSE;
   }
   return ret;
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
 *    ASOCKERR_SUCCESS or ASOCKERR_*
 *
 * Side-effects
 *    See description above.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocket_SetSendLowLatencyMode(AsyncSocket *asock,  // IN
                                  Bool enable)         // IN
{
   int ret;
   if (VALID(asock, setSendLowLatencyMode)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setSendLowLatencyMode(asock, enable);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
 *    ASOCKERR_SUCCESS indicates we have started async connect.
 *    ASOCKERR_* indicates a failure to start the connect.
 *
 *    Errors during asynchronous processing is reported using the
 *    callback supplied. Detailed SSL verification error can be
 *    retrieved from verifyParam structure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocket_StartSslConnect(AsyncSocket *asock,                   // IN
                            SSLVerifyParam *verifyParam,          // IN/OPT
                            void *sslCtx,                         // IN
                            AsyncSocketSslConnectFn sslConnectFn, // IN
                            void *clientData)                     // IN
{
   int ret;
   if (VALID(asock, startSslConnect)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->startSslConnect(asock, verifyParam, sslCtx, sslConnectFn,
                                       clientData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   Bool ret;
   if (VALID(asock, connectSSL)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->connectSSL(asock, verifyParam, sslContext);
      AsyncSocketUnlock(asock);
   } else {
      ret = FALSE;
   }
   return ret;
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
AsyncSocket_AcceptSSL(AsyncSocket *asock,    // IN
                      void *sslCtx)          // IN: optional
{
   Bool ret;
   if (VALID(asock, acceptSSL)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->acceptSSL(asock, sslCtx);
      AsyncSocketUnlock(asock);
   } else {
      ret = FALSE;
   }
   return ret;
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
 *    ASOCKERR_SUCCESS indicates we have started async accept.
 *    ASOCKERR_* indicates a failure to start the accept.
 *
 *    Errors during asynchronous processing are reported using the
 *    callback supplied.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
AsyncSocket_StartSslAccept(AsyncSocket *asock,                 // IN
                           void *sslCtx,                       // IN
                           AsyncSocketSslAcceptFn sslAcceptFn, // IN
                           void *clientData)                   // IN
{
   int ret;
   if (VALID(asock, startSslAccept)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->startSslAccept(asock, sslCtx, sslAcceptFn, clientData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
 *      before the timeout expired.  ASOCKERR_INVAL on invalid
 *      parameters or operation not implemented on this socket.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_Flush(AsyncSocket *asock,  // IN
                  int timeoutMS)       // IN
{
   int ret;
   if (VALID(asock, flush)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->flush(asock, timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
AsyncSocket_Recv(AsyncSocket *asock,         // IN
                 void *buf,                  // IN (buffer to fill)
                 int len,                    // IN
                 void *cb,                   // IN
                 void *cbData)               // IN
{
   int ret;
   if (VALID(asock, recv)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->recv(asock, buf, len, FALSE, cb, cbData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}

int
AsyncSocket_RecvPartial(AsyncSocket *asock,         // IN
                        void *buf,                  // IN (buffer to fill)
                        int len,                    // IN
                        void *cb,                   // IN
                        void *cbData)               // IN
{
   int ret;
   if (VALID(asock, recv)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->recv(asock, buf, len, TRUE, cb, cbData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, recvPassedFd)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->recvPassedFd(asock, buf, len, cb, cbData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getReceivedFd)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getReceivedFd(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = -1;
   }
   return ret;
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
AsyncSocket_Send(AsyncSocket *asock,         // IN
                 void *buf,                  // IN
                 int len,                    // IN
                 AsyncSocketSendFn sendFn,   // IN
                 void *clientData)           // IN
{
   int ret;
   if (VALID(asock, send)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->send(asock, buf, len, sendFn, clientData);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
 *      ASOCKERR_INVAL: null socket or operation not supported.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_IsSendBufferFull(AsyncSocket *asock)         // IN
{
   int ret;
   if (VALID(asock, isSendBufferFull)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->isSendBufferFull(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetNetworkStats --
 *
 *      Get network statistics from the active socket.
 *
 * Results:
 *      ASOCKERR_*
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetNetworkStats(AsyncSocket *asock,              // IN
                            AsyncSocketNetworkStats *stats)  // OUT
{
   int ret;
   if (VALID(asock, getNetworkStats)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getNetworkStats(asock, stats);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
AsyncSocket_Close(AsyncSocket *asock)         // IN
{
   int ret;
   if (VALID(asock, close)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->close(asock);
      AsyncSocketRelease(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, cancelRecv)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->cancelRecv(asock, partialRecvd, recvBuf, recvFn,
                                  cancelOnSend);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
 *      ASOCKERR_*.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_CancelCbForClose(AsyncSocket *asock)  // IN:
{
   int ret;
   if (VALID(asock, cancelCbForClose)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->cancelCbForClose(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getLocalVMCIAddress)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getLocalVMCIAddress(asock, cid, port);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
   int ret;
   if (VALID(asock, getRemoteVMCIAddress)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getRemoteVMCIAddress(asock, cid, port);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetWebSocketError --
 *
 *      Return the HTTP error code supplied during a failed WebSocket
 *      upgrade negotiation.
 *
 * Results:
 *      Numeric HTTP error code, 0 if no error, or -1 on invalid arguments.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetWebSocketError(AsyncSocket *asock)    // IN
{
   int ret;
   if (VALID(asock, getWebSocketError)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getWebSocketError(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = -1;
   }
   return ret;
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
   char *ret;
   if (VALID(asock, getWebSocketURI)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getWebSocketURI(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = NULL;
   }
   return ret;
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
   char *ret;
   if (VALID(asock, getWebSocketCookie)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getWebSocketCookie(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = NULL;
   }
   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetWebSocketCookie --
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

int
AsyncSocket_SetWebSocketCookie(AsyncSocket *asock,      // IN
                               void *clientData,        // IN
                               const char *path,        // IN
                               const char *sessionId)   // IN
{
   int ret = ASOCKERR_GENERIC;
   if (VALID(asock, setWebSocketCookie)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setWebSocketCookie(asock, clientData, path, sessionId);
      AsyncSocketUnlock(asock);
   }
   return ret;
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
AsyncSocket_GetWebSocketCloseStatus(AsyncSocket *asock)   // IN
{
   uint16 ret;
   if (VALID(asock, getWebSocketCloseStatus)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getWebSocketCloseStatus(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = 0;
   }
   return ret;
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
   const char *ret;
   if (VALID(asock, getWebSocketProtocol)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getWebSocketProtocol(asock);
      AsyncSocketUnlock(asock);
   } else {
      ret = NULL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_RecvBlocking --
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

int
AsyncSocket_RecvBlocking(AsyncSocket *asock,         // IN
                         void *buf,                  // OUT
                         int len,                    // IN
                         int *received,              // OUT
                         int timeoutMS)              // IN
{
   int ret;
   if (VALID(asock, recvBlocking)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->recvBlocking(asock, buf, len, received, timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_RecvPartialBlocking --
 *
 *      Implement "blocking + timeout" version of RecvPartial
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
AsyncSocket_RecvPartialBlocking(AsyncSocket *asock,         // IN
                                void *buf,                  // OUT
                                int len,                    // IN
                                int *received,              // OUT
                                int timeoutMS)              // IN
{
   int ret;
   if (VALID(asock, recvPartialBlocking)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->recvPartialBlocking(asock, buf, len, received,
                                           timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SendBlocking --
 *
 *      Implement "blocking + timeout" version of Send
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
AsyncSocket_SendBlocking(AsyncSocket *asock,         // IN
                         void *buf,                  // IN
                         int len,                    // IN
                         int *sent,                  // OUT
                         int timeoutMS)              // IN
{
   int ret;
   if (VALID(asock, sendBlocking)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->sendBlocking(asock, buf, len, sent, timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
AsyncSocket_DoOneMsg(AsyncSocket *asock,         // IN
                     Bool read,                  // IN
                     int timeoutMS)              // IN
{
   int ret;
   if (VALID(asock, doOneMsg)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->doOneMsg(asock, read, timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
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
AsyncSocket_WaitForConnection(AsyncSocket *asock,         // IN
                              int timeoutMS)              // IN
{
   int ret;
   if (VALID(asock, waitForConnection)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->waitForConnection(asock, timeoutMS);
      AsyncSocketUnlock(asock);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}

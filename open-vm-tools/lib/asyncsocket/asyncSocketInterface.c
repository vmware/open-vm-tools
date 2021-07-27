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

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

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
 * AsyncSocket_GetRemotePort --
 *
 *      Given an AsyncSocket object, returns the remote port associated
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
AsyncSocket_GetRemotePort(AsyncSocket *asock,  // IN
                          uint32 *port)        // OUT
{
   int ret;
   if (VALID(asock, getRemotePort)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->getRemotePort(asock, port);
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
 *      THIS IS DEPRECATED in favor of AsyncSocket_SetOption(...TCP_NODELAY...).
 *      It exists for now to avoid having to change all existing calling code.
 *      TODO: Remove it fully and fix up all calling code accordingly.
 *
 *      Sets the setsockopt() value TCP_NODELAY.
 *      asyncSocket may be an AsyncTCPSocket itself
 *      or contain one on which the option will be set.
 *
 *      This fails if there is no applicable AsyncTCPSocket (asyncSocket or
 *      one inside it).
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *      There being no applicable AsyncTCPSocket yields ASOCKERR_INVAL.
 *      OS error when setting value yields ASOCKERR_GENERIC.
 *
 * Side effects:
 *      Possibly increased bandwidth usage for short messages on this socket
 *      due to TCP overhead, in exchange for lower latency.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_UseNodelay(AsyncSocket *asyncSocket,  // IN/OUT
                       Bool noDelay)              // IN
{
   const int noDelayNative = noDelay ? 1 : 0;
   return AsyncSocket_SetOption(asyncSocket,
                                IPPROTO_TCP, TCP_NODELAY,
                                &noDelayNative, sizeof noDelayNative);
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetTCPTimeouts --
 *
 *      Sets setsockopt() TCP_KEEP{INTVL|IDLE|CNT} if available in the OS.
 *      asyncSocket may be an AsyncTCPSocket itself
 *      or contain one on which the option will be set.
 *
 *      This fails if there is no applicable AsyncTCPSocket (asyncSocket or
 *      one inside it).
 *
 * Results:
 *      ASOCKERR_SUCCESS if no error, or OS doesn't support options.
 *      There being no applicable AsyncTCPSocket yields ASOCKERR_INVAL.
 *      OS error when setting any one value yields ASOCKERR_GENERIC.
 *
 * Side effects:
 *      None.
 *      Note that in case of error ASOCKERR_GENERIC, 0, 1, or 2 of the values
 *      may have still been successfully set (the successful changes are
 *      not rolled back).
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetTCPTimeouts(AsyncSocket *asyncSocket,  // IN/OUT
                           int keepIdleSec,           // IN
                           int keepIntvlSec,          // IN
                           int keepCnt)               // IN
{
   /*
    * This function is NOT deprecated like the nearby setOption()-wrapping
    * functions. It's valuable because it: enapsulates OS-dependent logic; and
    * performs one lock before settong all applicable options together.
    */

#if defined(__linux__) || defined(VMX86_SERVER)
   /*
    * Tempting to call AsyncSocket_SetOption() x 3 instead of worrying about
    * locking and VT() ourselves, but this way we can reduce amount of
    * locking/unlocking at the cost of code verbosity.
    *
    * Reason for bailing on first error instead of trying all three:
    * it's what the original code (that this adapts) did. TODO: Find out from
    * author, explain here.
    */

   int ret;
   if (VALID(asyncSocket, setOption)) {
      AsyncSocketLock(asyncSocket);

      ret = VT(asyncSocket)->setOption
               (asyncSocket,
                IPPROTO_TCP, TCP_KEEPIDLE,
                &keepIdleSec, sizeof keepIdleSec);
      if (ret == ASOCKERR_SUCCESS) {
         ret = VT(asyncSocket)->setOption
                  (asyncSocket,
                   IPPROTO_TCP, TCP_KEEPINTVL,
                   &keepIntvlSec, sizeof keepIntvlSec);
         if (ret == ASOCKERR_SUCCESS) {
            ret = VT(asyncSocket)->setOption
                     (asyncSocket,
                      IPPROTO_TCP, TCP_KEEPCNT,
                      &keepCnt, sizeof keepCnt);
         }
      }

      AsyncSocketUnlock(asyncSocket);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
#else // #ifndef __linux__
   return ASOCKERR_SUCCESS;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * AsyncSocket_EstablishMinBufferSizes --
 *
 *      Meant to be invoked around socket creation time, this tries to ensure
 *      that SO_{SND|RCV}BUF setsockopt() values are set to at least the values
 *      provided as arguments. That is, it sets the given buffer size but only
 *      if the current value reported by the OS is smaller.
 *
 *      This fails unless asyncSocket is of the "applicable socket type."
 *      Being of "applicable socket type" is defined as supporting the
 *      option:
 *
 *        layer = SOL_SOCKET,
 *        optID = SO_{SND|RCV}BUF.
 *
 *      As of this writing, only AsyncTCPSockets (or derivations thereof)
 *      are supported, but (for example) UDP sockets could be added over time.
 *
 * Results:
 *      TRUE: on success; FALSE: on failure.
 *      Determining that no setsockopt() is required is considered success.
 *
 * Side effects:
 *      None.
 *      Note that in case of a setsockopt() failing, 0 or 1 of the values
 *      may have still been successfully set (the successful changes are
 *      not rolled back).
 *
 *-----------------------------------------------------------------------------
 */

Bool
AsyncSocket_EstablishMinBufferSizes(AsyncSocket *asyncSocket,  // IN/OUT
                                    int sendSz,                // IN
                                    int recvSz)                // IN
{
   Bool ok;

   if (VALID(asyncSocket, setOption)) {
      int curSendSz;
      socklen_t curSendSzSz = sizeof curSendSz;
      int curRecvSz;
      socklen_t curRecvSzSz = sizeof curRecvSz;

      AsyncSocketLock(asyncSocket);

      /*
       * For each buffer size, see if the current reported size is already
       * at least as large (in which case we needn't do anything for that one).
       * Bail out the moment anything fails, but don't worry about undoing any
       * change already made (as advertised in doc comment).
       *
       * Reason for bailing on first error instead of trying everything:
       * it's what the original code (that this adapts) did. TODO: Find out from
       * author, explain here.
       *
       * Note that depending on the type of socket and the particular
       * implementation (e.g., the TCP stack), asking for buffer size N might
       * result in an even larger buffer, like a multiple 2N. It's not an exact
       * science.
       */

      ok = (VT(asyncSocket)->getOption(asyncSocket,
                                       SOL_SOCKET, SO_SNDBUF,
                                       &curSendSz, &curSendSzSz) ==
            ASOCKERR_SUCCESS) &&
           (VT(asyncSocket)->getOption(asyncSocket,
                                       SOL_SOCKET, SO_RCVBUF,
                                       &curRecvSz, &curRecvSzSz) ==
            ASOCKERR_SUCCESS);
      if (ok && (curSendSz < sendSz)) {
         ok = VT(asyncSocket)->setOption(asyncSocket,
                                         SOL_SOCKET, SO_SNDBUF,
                                         &sendSz, sizeof sendSz) ==
              ASOCKERR_SUCCESS;
      }
      if (ok && (curRecvSz < recvSz)) {
         ok = VT(asyncSocket)->setOption(asyncSocket,
                                         SOL_SOCKET, SO_RCVBUF,
                                         &recvSz, sizeof recvSz) ==
              ASOCKERR_SUCCESS;
      }

      AsyncSocketUnlock(asyncSocket);
   } else {
      ok = FALSE;
   }

   return ok;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetSendLowLatencyMode --
 *
 *      THIS IS DEPRECATED in favor of
 *      AsyncSocket_SetOption(ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE).
 *      It exists for now to avoid having to change all existing calling code.
 *      TODO: Remove it fully and fix up all calling code accordingly.
 *
 *      Sets the aforementioned value. See doc comment on
 *      ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE for more info.
 *
 *      This fails unless asyncSocket is of the "applicable socket type."
 *      Being of "applicable socket type" is defined as supporting the
 *      option:
 *
 *        layer = ASYNC_SOCKET_OPTS_LAYER_BASE,
 *        optID = ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE.
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *      asyncSocket being of inapplicable socket type yields ASOCKERR_INVAL.
 *
 * Side effects:
 *      See ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE doc comment.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetSendLowLatencyMode(AsyncSocket *asyncSocket,  // IN
                                  Bool enable)               // IN
{
   int ret;
   if (VALID(asyncSocket, setOption)) {
      AsyncSocketLock(asyncSocket);
      ret = VT(asyncSocket)->setOption
               (asyncSocket, ASYNC_SOCKET_OPTS_LAYER_BASE,
                ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE,
                &enable, sizeof enable);
      AsyncSocketUnlock(asyncSocket);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_SetOption --
 *
 *      Sets the value of the given socket option belonging to the given
 *      option layer to the given value. The socket options mechanism is
 *      discussed in more detail in asyncsocket.h.
 *
 *      The exact behavior and supported options are dependent on the socket
 *      type. See the doc header for the specific implementation for details.
 *      If ->setOption is NULL, all options are invalid for that socket.
 *      Setting an invalid layer+option results in a no-op + error result.
 *
 *      For native options, layer = setsockopt() level,
 *      optID = setsockopt() option_name.
 *
 *      For non-native options, optID is obtained as follows: it is converted
 *      from an enum option ID value for your socket type; for example,
 *      from ASYNC_TCP_SOCKET_OPT_ALLOW_DECREASING_BUFFER_SIZE,
 *      where the latter is of type AsyncTCPSocket_OptID.
 *
 *      The option's value must reside at the buffer valuePtr that is
 *      inBufLen long. If inBufLen does not match the expected size for
 *      the given option, behavior is undefined.
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

int
AsyncSocket_SetOption(AsyncSocket *asyncSocket,     // IN/OUT
                      AsyncSocketOpts_Layer layer,  // IN
                      AsyncSocketOpts_ID optID,     // IN
                      const void *valuePtr,         // IN
                      socklen_t inBufLen)           // IN
{
   int ret;
   /*
    * Lacking a setOption() implementation is conceptually the same as
    * ->setOption() existing but determining layer+optID to be invalid
    * (ASOCKERR_INVAL results).
    */
   if (VALID(asyncSocket, setOption)) {
      AsyncSocketLock(asyncSocket);
      ret = VT(asyncSocket)->setOption(asyncSocket, layer, optID,
                                       valuePtr, inBufLen);
      AsyncSocketUnlock(asyncSocket);
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_GetOption --
 *
 *      Gets the value of the given socket option belonging to the given
 *      option layer. The socket options mechanism is
 *      discussed in more detail in asyncsocket.h.
 *      This is generally symmetrical to ..._SetOption(); most comments applying
 *      to that function apply to this one in common-sense ways.
 *      In particular a layer+optID combo is supported here if and only if it
 *      is supported for ..._SetOption().
 *
 *      The length of the output buffer at valuePtr must reside at *outBufLen
 *      at entry to this function. If *outBufLen does not match or exceed the
 *      expected size for the given option, behavior is undefined.
 *      At successful return from function, *outBufLen will be set to the
 *      length of the value written to at valuePtr.
 *
 * Results:
 *      ASOCKERR_SUCCESS on success, ASOCKERR_* otherwise.
 *      Invalid option+layer yields ASOCKERR_INVAL.
 *      Failure to get a native OS option yields ASOCKERR_GENERIC.
 *      *outBufLen being wrong (for the given option) yields undefined behavior.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_GetOption(AsyncSocket *asyncSocket,     // IN/OUT
                      AsyncSocketOpts_Layer layer,  // IN
                      AsyncSocketOpts_ID optID,     // IN
                      void *valuePtr,               // OUT
                      socklen_t *outBufLen)         // IN/OUT
{
   int ret;
   /*
    * Lacking a getOption() implementation is conceptually the same as
    * ->getOption() existing but determining layer+optID to be invalid
    * (ASOCKERR_INVAL results).
    */
   if (VALID(asyncSocket, getOption)) {
      AsyncSocketLock(asyncSocket);
      ret = VT(asyncSocket)->getOption(asyncSocket, layer, optID,
                                       valuePtr, outBufLen);
      AsyncSocketUnlock(asyncSocket);
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
                            const char *hostname,                 // IN/OPT
                            void *sslCtx,                         // IN
                            AsyncSocketSslConnectFn sslConnectFn, // IN
                            void *clientData)                     // IN
{
   int ret;
   if (VALID(asock, startSslConnect)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->startSslConnect(asock, verifyParam, hostname, sslCtx,
                                       sslConnectFn, clientData);
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
                       const char *hostname,        // IN/OPT
                       void *sslContext)            // IN/OPT
{
   Bool ret;
   if (VALID(asock, connectSSL)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->connectSSL(asock, verifyParam, hostname, sslContext);
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
 * AsyncSocket_Peek --
 *
 *      Similar to AsyncSocket_RecvPartial, AsyncSocket_Peek reads the socket
 *      buffer contents into the provided buffer by registering a callback that
 *      will fire when data becomes available.
 *
 *      Due to underying poll implementation, peeks are always "partial" ie.
 *      callback returns when less than or equal amount of requested length is
 *      available to read. Peek callers may use recv() to drain smaller amounts
 *      notified by peek-callback and then peek for more data if that helps.
 *
 *      There are some noteworthy differences compared to Recv():
 *
 *      - By definition, Recv() drains the socket buffer while Peek() does not
 *
 *      - Asyncsocket Recv() is post-SSL since it internally calls SSL_Read()
 *        so application always gets decrypted data when entire SSL record is
 *        decrypted. Peek() on the other hand is SSL agnostic; it reads
 *        directly from the underlying host socket and makes no attempt to
 *        decrypt it or check for any data buffered within SSL. So asyncsocket
 *        user doing a recv() followed by peek() may get different results.
 *        That is why is it most safe to use peek() before SSL is setup on the
 *        TCP connection.
 *
 *      - Peek is one-shot in nature, meaning that peek callbacks are
 *        unregistered from poll once fired.
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
AsyncSocket_Peek(AsyncSocket *asock,         // IN
                 void *buf,                  // IN (buffer to fill)
                 int len,                    // IN
                 void *cb,                   // IN
                 void *cbData)               // IN
{
   int ret;
   if (VALID(asock, recv)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->peek(asock, buf, len, cb, cbData);
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
      ASSERT(!asock->inited);
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
 *      Insert Set-Cookie HTTP response header during WebSocket connection.
 *
 * Results:
 *      ASOCKERR_SUCCESS if we finished the operation, ASOCKERR_* error codes
 *      otherwise.
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
   int ret = ASOCKERR_INVAL;
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
 * AsyncSocket_SetDelayWebSocketUpgradeResponse --
 *
 *      Set a flag for whether or not to not automatically send the websocket
 *      upgrade response upon receiving the websocket upgrade request.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_SetDelayWebSocketUpgradeResponse(AsyncSocket *asock,                  // IN
                                             Bool delayWebSocketUpgradeResponse)  // IN
{
   int ret = ASOCKERR_INVAL;
   if (VALID(asock, setDelayWebSocketUpgradeResponse)) {
      AsyncSocketLock(asock);
      ret = VT(asock)->setDelayWebSocketUpgradeResponse(asock,
                                                        delayWebSocketUpgradeResponse);
      AsyncSocketUnlock(asock);
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


/*
 *----------------------------------------------------------------------------
 *
 * AsyncSocket_WaitForReadMultiple --
 *
 *      Waits on a list of sockets, returning when a socket becomes
 *      available for read, or when the allowed time elapses.
 *
 *      Note, if this function is called by two threads with overlapping
 *      sets of sockets, a deadlock can occur. The caller should guard
 *      against such scenarios from happening, or making sure that there
 *      is a consistent ordering to the lists of sockets.
 *
 *      The caller must also make sure synchronous and asynchronous
 *      operations do not mix, as this function does not hold locks
 *      for the entirety of the call.
 *
 * Results:
 *      ASOCKERR_SUCCESS if one of the sockets is ready for read,
 *      ASOCKERR_GENERIC on failures, and ASOCKERR_TIMEOUT if nothing
 *      happened in the allotted time.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
AsyncSocket_WaitForReadMultiple(AsyncSocket **asock,  // IN
                                int numSock,          // IN
                                int timeoutMS,        // IN
                                int *outIdx)          // OUT
{
   int ret;

   if (numSock > 0 && VALID(asock[0], waitForReadMultiple)) {
      int i;

      for (i = 0; i < numSock; i++) {
         AsyncSocketLock(asock[i]);
      }
      ret = VT(asock[0])->waitForReadMultiple(asock, numSock,
                                              timeoutMS, outIdx);
      for (i = numSock - 1; i >= 0; i--) {
         AsyncSocketUnlock(asock[i]);
      }
   } else {
      ret = ASOCKERR_INVAL;
   }
   return ret;
}

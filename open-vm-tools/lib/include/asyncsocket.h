/*********************************************************
 * Copyright (C) 2003-2016 VMware, Inc. All rights reserved.
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

#ifndef __ASYNC_SOCKET_H__
#define __ASYNC_SOCKET_H__

/*
 * asyncsocket.h --
 *
 *      The AsyncSocket object is a fairly simple wrapper around a basic TCP
 *      socket. It's potentially asynchronous for both read and write
 *      operations. Reads are "requested" by registering a receive function
 *      that is called once the requested amount of data has been read from
 *      the socket. Similarly, writes are queued along with a send function
 *      that is called once the data has been written. Errors are reported via
 *      a separate callback.
 */

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/*
 * Error codes
 */
#define ASOCKERR_SUCCESS           0
#define ASOCKERR_GENERIC           1
#define ASOCKERR_TIMEOUT           2
#define ASOCKERR_NOTCONNECTED      3
#define ASOCKERR_REMOTE_DISCONNECT 4
#define ASOCKERR_INVAL             5
#define ASOCKERR_CONNECT           6
#define ASOCKERR_ACCEPT            7
#define ASOCKERR_POLL              8
#define ASOCKERR_CLOSED            9
#define ASOCKERR_BIND              10
#define ASOCKERR_BINDADDRINUSE     11
#define ASOCKERR_LISTEN            12
#define ASOCKERR_CONNECTSSL        13


/*
 * Websocket close status codes --
 *
 * enum has numbers in names because RFC6455 refers to the numbers frequently.
 */
enum {
   WEB_SOCKET_CLOSE_STATUS_1000_NORMAL = 1000,
   WEB_SOCKET_CLOSE_STATUS_1001_GOING_AWAY = 1001,
   WEB_SOCKET_CLOSE_STATUS_1002_PROTOCOL_ERROR = 1002,
   WEB_SOCKET_CLOSE_STATUS_1003_INVALID_DATA = 1003,
   WEB_SOCKET_CLOSE_STATUS_1005_EMPTY = 1005,
   WEB_SOCKET_CLOSE_STATUS_1006_ABNORMAL = 1006,
   WEB_SOCKET_CLOSE_STATUS_1007_INCONSISTENT_DATA = 1007,
   WEB_SOCKET_CLOSE_STATUS_1008_POLICY_VIOLATION = 1008,
   WEB_SOCKET_CLOSE_STATUS_1009_MESSAGE_TOO_BIG = 1009,
   WEB_SOCKET_CLOSE_STATUS_1010_UNSUPPORTED_EXTENSIONS = 1010,
   WEB_SOCKET_CLOSE_STATUS_1015_TLS_HANDSHAKE_ERROR = 1015,
};

/*
 * Flags passed into AsyncSocket_Connect*().
 * Default value is '0'.
 * The first two flags allow explicitly selecting
 * an ESX network stack. They no longer make sense because the
 * COS is gone. The flags are left around just to ensure we don't have
 * any flag collisions from users of the library.
 * The 3rd is for code that uses inet_pton() to get an IP address.
 * inet_pton() returns address in network-byte-order,
 * instead of the expected host-byte-order.
 */
typedef enum {
// ASOCKCONN_USE_ESX_SHADOW_STACK       = 1<<0,
// ASOCKCONN_USE_ESX_NATIVE_STACK       = 1<<1,
   ASOCKCONN_ADDR_IN_NETWORK_BYTE_ORDER = 1<<2
} AsyncSocketConnectFlags;

/*
 * SSL opaque type declarations (so we don't have to include ssl.h)
 */
struct SSLSockStruct;
struct _SSLVerifyParam;

/*
 * AsyncSocket type is opaque
 */
typedef struct AsyncSocket AsyncSocket;

/*
 * AsyncSocket registers poll callbacks, so give client the opportunity
 * to control how this is done.
 *
 * All the AsyncSocket constructors (Listen, Connect, Attach) take an
 * optional AsyncSocketPollParam* argument; if NULL the default behavior is
 * used (callback is registered in POLL_CS_MAIN and locked by the BULL).
 * Or the client can specify its favorite poll class and locking behavior.
 * Use of IVmdbPoll is only supported for regular sockets and for Attach.
 */
#include "poll.h"
struct IVmdbPoll;
typedef struct AsyncSocketPollParams {
   int flags;               /* Default 0, only POLL_FLAG_NO_BULL is valid */
   MXUserRecLock *lock;     /* Default: none but BULL */
   PollClassSet pollClass;  /* Default is POLL_CS_MAIN */
   struct IVmdbPoll *iPoll; /* Default NULL: use Poll_Callback */
} AsyncSocketPollParams;

/*
 * Initialize platform libraries
 */
int AsyncSocket_Init(void);

/*
 * Check the current state of the socket
 */
typedef enum AsyncSocketState {
   AsyncSocketListening,
   AsyncSocketConnecting,
   AsyncSocketConnected,
   AsyncSocketCBCancelled,
   AsyncSocketClosed,
} AsyncSocketState;

AsyncSocketState AsyncSocket_GetState(AsyncSocket *sock);

const char * AsyncSocket_Err2String(int err);

const char * AsyncSocket_MsgError(int asyncSockErr);

int AsyncSocket_GetGenericErrno(AsyncSocket *s);

/*
 * Return a "unique" ID
 */
int AsyncSocket_GetID(AsyncSocket *asock);

/*
 * Return the fd corresponding to the socket.
 */
int AsyncSocket_GetFd(AsyncSocket *asock);

/*
 * Return the remote IP address associated with this socket if applicable
 */
int AsyncSocket_GetRemoteIPStr(AsyncSocket *asock,
                               const char **ipStr);

int AsyncSocket_GetLocalVMCIAddress(AsyncSocket *asock,
                                    uint32 *cid, uint32 *port);
int AsyncSocket_GetRemoteVMCIAddress(AsyncSocket *asock,
                                     uint32 *cid, uint32 *port);

int AsyncSocket_GetINETIPStr(AsyncSocket *asock, int socketFamily,
                             char **ipRetStr);
unsigned int AsyncSocket_GetPort(AsyncSocket *asock);

/*
 * Recv callback fires once previously requested data has been received
 */
typedef void (*AsyncSocketRecvFn) (void *buf, int len, AsyncSocket *asock,
                                   void *clientData);

/*
 * Send callback fires once previously queued data has been sent
 */
typedef void (*AsyncSocketSendFn) (void *buf, int len, AsyncSocket *asock,
                                   void *clientData);

/*
 * Error callback fires on I/O errors during read/write operations
 */
typedef void (*AsyncSocketErrorFn) (int error, AsyncSocket *asock,
                                    void *clientData);

typedef void (*AsyncSocketConnectFn) (AsyncSocket *asock, void *clientData);

typedef void (*AsyncSocketSslAcceptFn) (Bool status, AsyncSocket *asock,
                                        void *clientData);
typedef void (*AsyncSocketSslConnectFn) (Bool status, AsyncSocket *asock,
                                         void *clientData);
typedef void (*AsyncSocketCloseCb) (AsyncSocket *asock);

/*
 * Listen on port and fire callback with new asock
 */
AsyncSocket *AsyncSocket_Listen(const char *addrStr,
                                unsigned int port,
                                AsyncSocketConnectFn connectFn,
                                void *clientData,
                                AsyncSocketPollParams *pollParams,
                                int *outError);
AsyncSocket *AsyncSocket_ListenLoopback(unsigned int port,
                                        AsyncSocketConnectFn connectFn,
                                        void *clientData,
                                        AsyncSocketPollParams *pollParams,
                                        int *outError);
AsyncSocket *AsyncSocket_ListenVMCI(unsigned int cid,
                                    unsigned int port,
                                    AsyncSocketConnectFn connectFn,
                                    void *clientData,
                                    AsyncSocketPollParams *pollParams,
                                    int *outError);
#ifndef VMX86_TOOLS
AsyncSocket *AsyncSocket_ListenWebSocket(const char *addrStr,
                                         unsigned int port,
                                         Bool useSSL,
                                         const char *protocols[],
                                         AsyncSocketConnectFn connectFn,
                                         void *clientData,
                                         AsyncSocketPollParams *pollParams,
                                         int *outError);
#ifndef _WIN32
AsyncSocket *AsyncSocket_ListenWebSocketUDS(const char *pipeName,
                                            Bool useSSL,
                                            const char *protocols[],
                                            AsyncSocketConnectFn connectFn,
                                            void *clientData,
                                            AsyncSocketPollParams *pollParams,
                                            int *outError);

AsyncSocket *AsyncSocket_ListenSocketUDS(const char *pipeName,
                                         AsyncSocketConnectFn connectFn,
                                         void *clientData,
                                         AsyncSocketPollParams *pollParams,
                                         int *outError);

#endif
#endif

/*
 * Connect to address:port and fire callback with new asock
 */
AsyncSocket *AsyncSocket_Connect(const char *hostname,
                                 unsigned int port,
                                 AsyncSocketConnectFn connectFn,
                                 void *clientData,
                                 AsyncSocketConnectFlags flags,
                                 AsyncSocketPollParams *pollParams,
                                 int *error);
AsyncSocket *AsyncSocket_ConnectVMCI(unsigned int cid, unsigned int port,
                                     AsyncSocketConnectFn connectFn,
                                     void *clientData,
                                     AsyncSocketConnectFlags flags,
                                     AsyncSocketPollParams *pollParams,
                                     int *error);
#ifndef _WIN32
AsyncSocket *AsyncSocket_ConnectUnixDomain(const char *path,
                                           AsyncSocketConnectFn connectFn,
                                           void *clientData,
                                           AsyncSocketConnectFlags flags,
                                           AsyncSocketPollParams *pollParams,
                                           int *error);
#else
AsyncSocket *
AsyncSocket_ConnectNamedPipe(const char *pipeName,
                             AsyncSocketConnectFn connectFn,
                             void *clientData,
                             AsyncSocketConnectFlags flags,
                             AsyncSocketPollParams *pollParams,
                             int *outError);

AsyncSocket*
AsyncSocket_CreateNamedPipe(const char *pipeName,
                            AsyncSocketConnectFn connectFn,
                            void *clientData,
                            DWORD openMode,
                            DWORD pipeMode,
                            uint32 numInstances,
                            AsyncSocketPollParams *pollParams,
                            int *error);
#endif

#if !defined VMX86_TOOLS || TARGET_OS_IPHONE
AsyncSocket *
AsyncSocket_ConnectWebSocket(const char *url,
                             struct _SSLVerifyParam *sslVerifyParam,
                             const char *cookies,
                             const char *protocols[],
                             AsyncSocketConnectFn connectFn,
                             void *clientData,
                             AsyncSocketConnectFlags flags,
                             AsyncSocketPollParams *pollParams,
                             int *error);

AsyncSocket *
AsyncSocket_ConnectProxySocket(const char *url,
                               struct _SSLVerifyParam *sslVerifyParam,
                               const char *cookies,
                               const char *protocols[],
                               AsyncSocketConnectFn connectFn,
                               void *clientData,
                               AsyncSocketConnectFlags flags,
                               AsyncSocketPollParams *pollParams,
                               int *error);
#endif

/*
 * Initiate SSL connection on existing asock, with optional cert verification
 */
Bool AsyncSocket_ConnectSSL(AsyncSocket *asock,
                            struct _SSLVerifyParam *verifyParam,
                            void *sslContext);
void AsyncSocket_StartSslConnect(AsyncSocket *asock,
                                 struct _SSLVerifyParam *verifyParam,
                                 void *sslCtx,
                                 AsyncSocketSslConnectFn sslConnectFn,
                                 void *clientData);

Bool AsyncSocket_AcceptSSL(AsyncSocket *asock);
void AsyncSocket_StartSslAccept(AsyncSocket *asock,
                                void *sslCtx,
                                AsyncSocketSslAcceptFn sslAcceptFn,
                                void *clientData);

/*
 * Create a new AsyncSocket from an existing socket
 */
AsyncSocket *AsyncSocket_AttachToFd(int fd, AsyncSocketPollParams *pollParams,
                                    int *error);
AsyncSocket *AsyncSocket_AttachToSSLSock(struct SSLSockStruct *sslSock,
                                         AsyncSocketPollParams *pollParams,
                                         int *error);

/*
 * Enable or disable TCP_NODELAY on this AsyncSocket.
 */
int AsyncSocket_UseNodelay(AsyncSocket *asock, Bool nodelay);

/*
 * Set TCP timeout values on this AsyncSocket.
 */
#ifdef VMX86_SERVER
int AsyncSocket_SetTCPTimeouts(AsyncSocket *asock, int keepIdle,
                               int keepIntvl, int keepCnt);
#endif

/*
 * Waits until at least one packet is received or times out.
 */
int AsyncSocket_DoOneMsg(AsyncSocket *s, Bool read, int timeoutMS);

/*
 * Waits until at least one connect() is accept()ed or times out.
 */
int AsyncSocket_WaitForConnection(AsyncSocket *s, int timeoutMS);

/*
 * Send all pending packets onto the wire or give up after timeoutMS msecs.
 */
int AsyncSocket_Flush(AsyncSocket *asock, int timeoutMS);

/*
 * Specify the exact amount of data to receive and the receive function to call.
 */
int AsyncSocket_Recv(AsyncSocket *asock, void *buf, int len, void *cb, void *cbData);

/*
 * Specify the maximum amount of data to receive and the receive function to call.
 */
int AsyncSocket_RecvPartial(AsyncSocket *asock, void *buf, int len,
                            void *cb, void *cbData);

/*
 * Specify the amount of data to receive and the receive function to call.
 */
int AsyncSocket_RecvPassedFd(AsyncSocket *asock, void *buf, int len,
                             void *cb, void *cbData);

/*
 * Retrieve socket received via RecvPassedFd.
 */
int AsyncSocket_GetReceivedFd(AsyncSocket *asock);


/*
 * Specify the amount of data to send/receive and how long to wait before giving
 * up.
 */
int AsyncSocket_RecvBlocking(AsyncSocket *asock,
                             void *buf, int len, int *received,
                             int timeoutMS);

int AsyncSocket_RecvPartialBlocking(AsyncSocket *asock,
                                    void *buf, int len, int *received,
                                    int timeoutMS);

int AsyncSocket_SendBlocking(AsyncSocket *asock,
                             void *buf, int len, int *sent,
                             int timeoutMS);

/*
 * Specify the amount of data to send and the send function to call
 */
int AsyncSocket_Send(AsyncSocket *asock, void *buf, int len,
                      AsyncSocketSendFn sendFn, void *clientData);

int AsyncSocket_IsSendBufferFull(AsyncSocket *asock);
int AsyncSocket_CancelRecv(AsyncSocket *asock, int *partialRecvd, void **recvBuf,
                           void **recvFn);
int AsyncSocket_CancelRecvEx(AsyncSocket *asock, int *partialRecvd, void **recvBuf,
                             void **recvFn, Bool cancelOnSend);
/*
 * Unregister asynchronous send and recv from poll
 */
void AsyncSocket_CancelCbForClose(AsyncSocket *asock);

/*
 * Set the error handler to invoke on I/O errors (default is to close the
 * socket)
 */
int AsyncSocket_SetErrorFn(AsyncSocket *asock, AsyncSocketErrorFn errorFn,
                           void *clientData);

/*
 * Set socket level recv/send buffer sizes if they are less than given sizes.
 */
Bool AsyncSocket_SetBufferSizes(AsyncSocket *asock,  // IN
                                int sendSz,    // IN
                                int recvSz);   // IN

/*
 * Set optional AsyncSocket_Close() behaviors.
 */
void AsyncSocket_SetCloseOptions(AsyncSocket *asock,
                                 int flushEnabledMaxWaitMsec,
                                 AsyncSocketCloseCb closeCb);

/*
 * Send websocket close frame.
 */
int
AsyncSocket_SendWebSocketCloseFrame(AsyncSocket *asock,
                                    uint16 closeStatus);

/*
 * Close the connection and destroy the asock.
 */
int AsyncSocket_Close(AsyncSocket *asock);

/*
 * Retrieve the URI Supplied for a websocket connection
 */
char *AsyncSocket_GetWebSocketURI(AsyncSocket *asock);

/*
 * Retrieve the Cookie Supplied for a websocket connection
 */
char *AsyncSocket_GetWebSocketCookie(AsyncSocket *asock);

/*
 * Retrieve the close status, if received, for a websocket connection
 */
uint16 AsyncSocket_GetWebSocketCloseStatus(const AsyncSocket *asock);

/*
 * Set low-latency mode for sends:
 */
void AsyncSocket_SetSendLowLatencyMode(AsyncSocket *asock, Bool enable);

/*
 * Get negotiated websocket protocol
 */
const char *AsyncSocket_GetWebSocketProtocol(AsyncSocket *asock);

const char * stristr(const char *s, const char *find);

/*
 * Some logging macros for convenience
 */
#define ASOCKPREFIX "SOCKET "

#define ASOCKWARN(_asock, _warnargs)                                 \
   do {                                                              \
      Warning(ASOCKPREFIX "%d (%d) ",                                \
              AsyncSocket_GetID(_asock), AsyncSocket_GetFd(_asock)); \
      Warning _warnargs;                                             \
   } while (0)

#define ASOCKLG0(_asock, _logargs)                               \
   do {                                                          \
      Log(ASOCKPREFIX "%d (%d) ",                                \
          AsyncSocket_GetID(_asock), AsyncSocket_GetFd(_asock)); \
      Log _logargs;                                              \
   } while (0)

#define ASOCKLOG(_level, _asock, _logargs)                            \
   do {                                                               \
      if (((_level) == 0) || DOLOG_BYNAME(asyncsocket, (_level))) {   \
         Log(ASOCKPREFIX "%d (%d) ",                                  \
             AsyncSocket_GetID((_asock)), AsyncSocket_GetFd(_asock)); \
         Log _logargs;                                                \
      }                                                               \
   } while(0)

#endif // __ASYNC_SOCKET_H__


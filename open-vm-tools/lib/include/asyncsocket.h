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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
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
#define ASOCKERR_NETUNREACH        14
#define ASOCKERR_ADDRUNRESV        15
#define ASOCKERR_BUSY              16

/*
 * Cross-platform codes for AsyncSocket_GetGenericError():
 */
#ifdef _WIN32
#define ASOCK_ENOTCONN          WSAENOTCONN
#define ASOCK_ENOTSOCK          WSAENOTSOCK
#define ASOCK_EADDRINUSE        WSAEADDRINUSE
#define ASOCK_ECONNECTING       WSAEWOULDBLOCK
#define ASOCK_EWOULDBLOCK       WSAEWOULDBLOCK
#define ASOCK_ENETUNREACH       WSAENETUNREACH
#define ASOCK_ECONNRESET        WSAECONNRESET
#define ASOCK_ECONNABORTED      WSAECONNABORTED
#define ASOCK_EPIPE             ERROR_NO_DATA
#else
#define ASOCK_ENOTCONN          ENOTCONN
#define ASOCK_ENOTSOCK          ENOTSOCK
#define ASOCK_EADDRINUSE        EADDRINUSE
#define ASOCK_ECONNECTING       EINPROGRESS
#define ASOCK_EWOULDBLOCK       EWOULDBLOCK
#define ASOCK_ENETUNREACH       ENETUNREACH
#define ASOCK_ECONNRESET        ECONNRESET
#define ASOCK_ECONNABORTED      ECONNABORTED
#define ASOCK_EPIPE             EPIPE
#endif

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


typedef struct AsyncSocketNetworkStats {
   uint32 cwndBytes;             /* maximum outstanding bytes */
   uint32 rttSmoothedAvgMillis;  /* rtt average in milliseconds */
   uint32 rttSmoothedVarMillis;  /* rtt variance in milliseconds */
   uint32 queuedBytes;           /* unsent bytes in send queue */
   uint32 inflightBytes;         /* current outstanding bytes */
   double packetLossPercent;     /* packet loss percentage */
} AsyncSocketNetworkStats;


/*
 * The following covers all facilities involving dynamic socket options w.r.t.
 * various async sockets, excluding the async socket options API on the
 * sockets themselves, which can be found in asyncSocketVTable.h and those
 * files implementing that API.
 *
 * Potential related future work is covered in
 * asyncsocket/README-asyncSocketOptions-future-work.txt.
 *
 * Summary of dynamic socket options:
 *
 * Dynamic socket option = setting settable by the async socket API user
 * including during the lifetime of the socket. An interface spiritually
 * similar to setsockopt()'s seemed appropriate.
 *
 * The option-setting API looks as follows:
 *
 *   int ...SetOption(AsyncSocket *asyncSocket,
 *                    AsyncSocketOpts_Layer layer, // enum type
 *                    AsyncSocketOpts_ID optID, // an integer type
 *                    const void *valuePtr,
 *                    size_t inBufLen)
 *
 * Both native (setsockopt()) and non-native (usually, struct member)
 * options are supported. layer and optID arguments are conceptually similar
 * to setsockopt() level and option_name arguments, respectively.
 *
 * FOR NATIVE (setsockopt()) OPTIONS:
 *    layer = setsockopt() level value.
 *    optID = setsockopt() option_name value.
 *
 * FOR NON-NATIVE (struct member inside socket impl.) OPTIONS:
 *    layer = ..._BASE, ..._TCP, ..._FEC, etc.
 *       (pertains to the various AsyncSocket types);
 *    optID = value from enum type appropriate to the chosen layer.
 *
 * Examples (prefixes omitted for space):
 *
 *    -- NATIVE OPTIONS --
 *    optID          | layer       | <= | ssopt() level | ssopt() option_name
 *    ---------------+-------------+----+---------------+--------------------
 *    == option_name | == level    | <= | SOL_SOCKET    | SO_SNDBUF
 *    == option_name | == level    | <= | IPPROTO_TCP   | TCP_NODELAY
 *
 *    -- NON-NATIVE OPTIONS --
 *    optID                          | layer | <= | AsyncSocket type(s)
 *    -------------------------------+-------+----+--------------------
 *    _SEND_LOW_LATENCY_MODE         | _BASE | <= | any
 *       (enum AsyncSocket_OptID)    |       |    |
 *    _ALLOW_DECREASING_BUFFER_SIZE  | _TCP  | <= | AsyncTCPSocket
 *       (enum AsyncTCPSocket_OptID) |       |    |
 *    _MAX_CWND                      | _FEC  | <= | FECAsyncSocket
 *       (enum FECAsyncSocket_OptID) |       |    |
 *
 * Socket option lists for each non-native layer are just enums. Each socket
 * type should declare its own socket option enum in its own .h file; e.g., see
 * AsyncTCPSocket_OptID in this file. Some option lists apply to all async
 * sockets; these are also here in asyncsocket.h.
 *
 * The only way in which different socket option layers coexist in the same
 * file is the layer enum, AsyncSocketOpts_Layer, in the present file,
 * which enumerates all possible layers.
 *
 * The lack of any other cross-pollution between different non-native option
 * lists' containing files is a deliberate design choice.
 */

/*
 * Integral type used for the optID argument to ->setOption() async socket API.
 *
 * For a non-native option, use an enum value for your socket type.
 * (Example: ASYNC_TCP_SOCKET_OPT_ALLOW_DECREASING_BUFFER_SIZE
 * of type AsyncTCPSocket_OptID, which would apply to TCP sockets only.)
 *
 * For a native (setsockopt()) option, use the setsockopt() integer directly.
 * (Example: TCP_NODELAY.)
 *
 * Let's use a typedef as a small bit of abstraction and to be able to easily
 * change it to size_t, if (for example) we start indexing arrays with this
 * thing.
 */
typedef int AsyncSocketOpts_ID;

/*
 * Enum type used for the layer argument to ->setOption() async socket API.
 * As explained in the summary comment above, this
 * informs the particular ->setOption() implementation how to interpret
 * the accompanying optID integer value, as it may refer to one of several
 * option lists; and possible different socket instances (not as of this
 * writing).
 *
 * If editing, see summary comment above first for background.
 *
 * The values explicitly in this enum are for non-native options.
 * For native options, simply use the level value as for setsockopt().
 *
 * Ordinal values for all these non-native layers must not clash
 * with the native levels; hence the `LEVEL + CONSTANT` trick
 * just below.
 */
typedef enum {

   /*
    * Used when optID applies to a non-native socket option applicable to ANY
    * async socket type.
    */
   ASYNC_SOCKET_OPTS_LAYER_BASE = SOL_SOCKET + 1000,

   /*
    * Next enums must follow the above ordinally, so just:
    *    ASYNC_SOCKET_OPTS_LAYER_<layer name 1>,
    *    ASYNC_SOCKET_OPTS_LAYER_<layer name 2>, ...
    */

   ASYNC_SOCKET_OPTS_LAYER_BLAST_PROXY,

} AsyncSocketOpts_Layer;

/*
 * Enum type used for the OptId argument to ->setOption() async socket API,
 * when optID refers to a non-native option of any AsyncSocket regardless
 * of type.
 */
typedef enum {
   /*
    * Bool indicating whether to put the socket into a mode where we attempt
    * to issue sends directly from within ->send(). Ordinarily
    * (FALSE), we would set up a Poll callback from within ->send(),
    * which introduces some non-zero latency to the send path. In
    * low-latency-send mode (TRUE), that delay is potentially avoided. This
    * does introduce a behavioral change; the send completion
    * callback may be triggered before the call to ->send() returns. As
    * not all clients may be expecting this, we don't enable this mode
    * unless requested by the client.
    *
    * Default: FALSE.
    */
   ASYNC_SOCKET_OPT_SEND_LOW_LATENCY_MODE,
   /*
    * This socket config option provides a way to set DSCP value
    * on the TOS field of IP packet which is a 6 bit value.
    * Permissible values to configure are 0x0 to 0x3F, although
    * there are only subset of these values which are widely used.
    *
    * Default: 0.
    */
   ASYNC_SOCKET_OPT_DSCP
} AsyncSocket_OptID;

/*
 * Note: If you need to add a non-native option that applies to AsyncTCPSockets
 * only, you'd probably introduce an enum here named AsyncTCPSocket_OptID; and
 * at least one layer named ASYNC_SOCKET_OPTS_LAYER_TCP in the enum
 * AsyncSocketOpts_Layer.
 */


/* API functions for all AsyncSockets. */

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
typedef void (*AsyncSocketCloseFn) (AsyncSocket *asock, void *clientData);

/*
 * Callback to handle http upgrade request header
 */
typedef int (*AsyncWebSocketHandleUpgradeRequestFn) (AsyncSocket *asock,
                                                     void *clientData,
                                                     const char *httpRequest,
                                                     char **httpResponse);

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
AsyncSocket *AsyncSocket_ListenWebSocket(const char *addrStr,
                                         unsigned int port,
                                         Bool useSSL,
                                         const char *protocols[],
                                         AsyncSocketConnectFn connectFn,
                                         void *clientData,
                                         AsyncSocketPollParams *pollParams,
                                         void *sslCtx,
                                         int *outError);
AsyncSocket *AsyncSocket_ListenWebSocketEx(const char *addrStr,
                                           unsigned int port,
                                           Bool useSSL,
                                           const char *protocols[],
                                           AsyncSocketConnectFn connectFn,
                                           void *clientData,
                                           AsyncSocketPollParams *pollParams,
                                           void *sslCtx,
                                           AsyncWebSocketHandleUpgradeRequestFn handleUpgradeRequestFn,
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
AsyncSocket *AsyncSocket_ConnectWithFd(const char *hostname,
                                       unsigned int port,
                                       int tcpSocketFd,
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

#define ASOCK_NAMEDPIPE_ALLOW_DEFAULT             (0)
#define ASOCK_NAMEDPIPE_ALLOW_ADMIN_USER_VMWARE   (SDPRIV_GROUP_ADMIN  |   \
                                                   SDPRIV_USER_CURRENT |   \
                                                   SDPRIV_GROUP_VMWARE)
#define ASOCK_NAMEDPIPE_ALLOW_ADMIN_USER          (SDPRIV_GROUP_ADMIN  |   \
                                                   SDPRIV_USER_CURRENT)

AsyncSocket*
AsyncSocket_CreateNamedPipe(const char *pipeName,
                            AsyncSocketConnectFn connectFn,
                            void *clientData,
                            DWORD openMode,
                            DWORD pipeMode,
                            uint32 numInstances,
                            DWORD accessType,
                            AsyncSocketPollParams *pollParams,
                            int *error);
#endif

AsyncSocket *
AsyncSocket_ConnectWebSocket(const char *url,
                             struct _SSLVerifyParam *sslVerifyParam,
                             const char *httpProxy,
                             const char *cookies,
                             const char *protocols[],
                             AsyncSocketConnectFn connectFn,
                             void *clientData,
                             AsyncSocketConnectFlags flags,
                             AsyncSocketPollParams *pollParams,
                             int *error);

/*
 * Initiate SSL connection on existing asock, with optional cert verification
 */
Bool AsyncSocket_ConnectSSL(AsyncSocket *asock,
                            struct _SSLVerifyParam *verifyParam,
                            void *sslContext);
int AsyncSocket_StartSslConnect(AsyncSocket *asock,
                                struct _SSLVerifyParam *verifyParam,
                                void *sslCtx,
                                AsyncSocketSslConnectFn sslConnectFn,
                                void *clientData);

Bool AsyncSocket_AcceptSSL(AsyncSocket *asock, void *sslCtx);
int AsyncSocket_StartSslAccept(AsyncSocket *asock,
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

int AsyncSocket_UseNodelay(AsyncSocket *asyncSocket, Bool nodelay);
int AsyncSocket_SetTCPTimeouts(AsyncSocket *asyncSocket,
                               int keepIdleSec, int keepIntvlSec, int keepCnt);
Bool AsyncSocket_EstablishMinBufferSizes(AsyncSocket *asyncSocket,
                                         int sendSz,
                                         int recvSz);
int AsyncSocket_SetSendLowLatencyMode(AsyncSocket *asyncSocket, Bool enable);
int AsyncSocket_SetOption(AsyncSocket *asyncSocket,
                          AsyncSocketOpts_Layer layer,
                          AsyncSocketOpts_ID optID,
                          const void *valuePtr, socklen_t inBufLen);
int AsyncSocket_GetOption(AsyncSocket *asyncSocket,
                          AsyncSocketOpts_Layer layer,
                          AsyncSocketOpts_ID optID,
                          void *valuePtr, socklen_t *outBufLen);

/*
 * Waits until at least one packet is received or times out.
 */
int AsyncSocket_DoOneMsg(AsyncSocket *s, Bool read, int timeoutMS);

/*
 * Waits until at least one connect() is accept()ed or times out.
 */
int AsyncSocket_WaitForConnection(AsyncSocket *s, int timeoutMS);

/*
 * Waits until a socket is ready with readable data or times out.
 */
int AsyncSocket_WaitForReadMultiple(AsyncSocket **asock, int numSock,
                                    int timeoutMS, int *outIdx);

/*
 * Send all pending packets onto the wire or give up after timeoutMS msecs.
 */
int AsyncSocket_Flush(AsyncSocket *asock, int timeoutMS);

/*
 * Drain recv for a remotely disconnected TCP socket.
 */
int AsyncSocket_TCPDrainRecv(AsyncSocket *base, int timeoutMS);

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
int AsyncSocket_GetNetworkStats(AsyncSocket *asock,
                                AsyncSocketNetworkStats *stats);
int AsyncSocket_CancelRecv(AsyncSocket *asock, int *partialRecvd, void **recvBuf,
                           void **recvFn);
int AsyncSocket_CancelRecvEx(AsyncSocket *asock, int *partialRecvd, void **recvBuf,
                             void **recvFn, Bool cancelOnSend);
/*
 * Unregister asynchronous send and recv from poll
 */
int AsyncSocket_CancelCbForClose(AsyncSocket *asock);

/*
 * Set the error handler to invoke on I/O errors (default is to close the
 * socket)
 */
int AsyncSocket_SetErrorFn(AsyncSocket *asock, AsyncSocketErrorFn errorFn,
                           void *clientData);

/*
 * Set optional AsyncSocket_Close() behaviors.
 */
int AsyncSocket_SetCloseOptions(AsyncSocket *asock,
                                int flushEnabledMaxWaitMsec,
                                AsyncSocketCloseFn closeCb);

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
 * Set the Cookie  for a websocket connection
 */
int AsyncSocket_SetWebSocketCookie(AsyncSocket *asock,         // IN
                                   void *clientData,           // IN
                                   const char *path,           // IN
                                   const char *sessionId);     // IN

/*
 * Retrieve the close status, if received, for a websocket connection
 */
uint16 AsyncSocket_GetWebSocketCloseStatus(AsyncSocket *asock);

/*
 * Get negotiated websocket protocol
 */
const char *AsyncSocket_GetWebSocketProtocol(AsyncSocket *asock);

/*
 * Get error code for websocket failure
 */
int AsyncSocket_GetWebSocketError(AsyncSocket *asock);

const char * stristr(const char *s, const char *find);

/*
 * Helper function to parse websocket URL
 */
Bool AsyncSocket_WebSocketParseURL(const char *url, char **hostname,
                                   unsigned int *port, Bool *useSSL,
                                   char **relativeURL);

/*
 * Find and return the value for the given header key in the supplied buffer
 */
char *AsyncSocket_WebSocketGetHttpHeader(const char *request,
                                         const char *webKey);

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

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // __ASYNC_SOCKET_H__

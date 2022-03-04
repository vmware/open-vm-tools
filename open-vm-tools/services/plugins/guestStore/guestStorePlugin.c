/*********************************************************
 * Copyright (C) 2019-2021 VMware, Inc. All rights reserved.
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

/**
 * @file guestStorePlugin.c
 *
 * GuestStore plugin, allow client to download content from GuestStore.
 */


#define G_LOG_DOMAIN  "guestStore"

#include <glib-object.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "posix.h"
#include "file.h"
#endif

#include "vm_assert.h"
#include "vm_basic_types.h"
#include "vmware/tools/guestStore.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
#include "dataMap.h"
#include "guestStoreConst.h"
#include "guestStoreDefs.h"
#include "rpcout.h"
#include "poll.h"
#ifdef OPEN_VM_TOOLS
#include "vmci_sockets.h"
#else
#include "vsockCommon.h"
#endif
#include "asyncsocket.h"
#include "str.h"
#include "util.h"
#include "guestApp.h"
#include "vmcheck.h"
#ifdef _WIN32
#include "guestStoreWin32.h"
#endif

VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);

#ifndef sockerr
#ifdef _WIN32
#define sockerr()  WSAGetLastError()
#else
#define sockerr()  errno
#endif
#endif  // sockerr

/*
 * User level send/recv buffers
 */

/*
 * Client connection send/recv buffer size
 */
#define CLIENT_CONN_SEND_RECV_BUF_SIZE  GUESTSTORE_REQUEST_BUFFER_SIZE

/*
 * VMX connection send/recv buffer size
 */
#define VMX_CONN_SEND_RECV_BUF_SIZE  GUESTSTORE_RESPONSE_BUFFER_SIZE

/*
 * Maximum concurrent client connections
 */
#define DEFAULT_MAX_CLIENT_CONNECTIONS  8

/*
 * Default timeout value in seconds for receiving from client connections
 */
#define DEFAULT_CLIENT_RECV_TIMEOUT  3  // seconds


/*
 * Client connection details
 */
typedef struct _ClientConnInfo {
   AsyncSocket *asock;

   char *buf;     // Send/recv buffer for HTTP request/response head
   int32 bufLen;  // Send/recv buffer length

   Bool shutDown;  // Close connection in send callback.

   Bool isCurrent;  // True for the current client connection
   char *requestPath;  // Requested GuestStore content path
   GSource *timeoutSource;  // Timeout source for receiving HTTP request
} ClientConnInfo;

/*
 * VMX connection details
 */
typedef struct _VmxConnInfo {
   AsyncSocket *asock;

   char *buf;     // Send/recv buffer for content transfer
   int32 bufLen;  // Send/recv buffer length

   Bool shutDown;  // Close connection in send callback.

   int32 dataMapLen;  // Recv buffer for VMX data map size
   int32 connTimeout;  // Connection inactivity timeout
   int64 bytesRemaining;  // Track remaining content size to transfer
   GSource *timeoutSource;  // Timeout source for connection inactivity
} VmxConnInfo;

typedef struct {
   AsyncSocket *vmxListenSock;     // For vsocket connections from VMX
   AsyncSocket *clientListenSock;  // For connections from clients

   GList *clientConnWaitList;  // Client connections in waiting list

   ClientConnInfo *clientConn;  // The current client connection being served
   VmxConnInfo    *vmxConn;     // The VMX connection providing service

   ToolsAppCtx *ctx;  // vmtoolsd application context

   Bool featureDisabled;  // Track tools.conf [guestStore]disabled change
   Bool adminOnly;  // Track tools.conf [guestStore]adminOnly change

   Bool guestStoreAccessEnabled;  // VMX GuestStore access enable status

   Bool vmxConnectRequested;  // VMX connect request sent status
   GSource *timeoutSource;  // Timeout source for VMX to guest connection
   Bool shutdown;  // vmtoolsd shutdown
} PluginData;

static PluginData pluginData = {0};

#define currentClientConn  pluginData.clientConn
#define theVmxConn         pluginData.vmxConn

#define ReceivedHttpRequestFromCurrentClientConn()  \
   (currentClientConn->requestPath != NULL)

/*
 * Macros to read values from config file
 */
#define GUESTSTORE_CONFIG_GET_BOOL(key, defVal)  \
   VMTools_ConfigGetBoolean(pluginData.ctx->config, "guestStore", key, defVal)

#define GUESTSTORE_CONFIG_GET_INT(key, defVal)  \
   VMTools_ConfigGetInteger(pluginData.ctx->config, "guestStore", key, defVal)


/*
 *-----------------------------------------------------------------------------
 *
 * GetCurrentUtcStr --
 *
 *      Get the current UTC time in a string format suitable for usage
 *      in HTTP response.
 *
 * Results:
 *      Return the current UTC time string, e.g.
 *
 *      Wed, 07 Nov 2018 20:50:11 GMT
 *
 *      The caller should call g_free() to free it.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gchar *
GetCurrentUtcStr(void)
{
   gchar *res = NULL;
   GDateTime *utcTime = g_date_time_new_now_utc();

   if (NULL != utcTime) {
      res = g_date_time_format(utcTime, "%a, %d %b %Y %T GMT");
      g_date_time_unref(utcTime);
   }

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * IsFeatureDisabled --
 *
 *      Check if guest admin/root has disabled GuestStore access.
 *
 * Results:
 *      Return the configured boolean value, default is FALSE.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline Bool
IsFeatureDisabled(void)
{
   return GUESTSTORE_CONFIG_GET_BOOL("disabled", FALSE);
}

#define CheckAndUpdateFeatureDisabled()  \
   (pluginData.featureDisabled = IsFeatureDisabled())


/*
 *-----------------------------------------------------------------------------
 *
 * IsAdminOnly --
 *
 *      Check if only guest admin/root has access to GuestStore.
 *
 * Results:
 *      Return the configured boolean value, default is FALSE.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline Bool
IsAdminOnly(void)
{
   return GUESTSTORE_CONFIG_GET_BOOL("adminOnly", FALSE);
}

#define CheckAndUpdateAdminOnly()  \
   (pluginData.adminOnly = IsAdminOnly())


static void
StartServeNextClientConn(void);

static void
CloseClientConn(ClientConnInfo *clientConn);  // IN

#define CloseCurrentClientConn()           \
   if (currentClientConn != NULL) {        \
      CloseClientConn(currentClientConn);  \
   }

#define CloseCurrentClientConnIfReceivedHttpRequest()  \
   if (currentClientConn != NULL &&                    \
       currentClientConn->requestPath != NULL) {       \
      CloseClientConn(currentClientConn);              \
   }

#define CloseClientConnsInWait()                                   \
   while (pluginData.clientConnWaitList != NULL) {                 \
      ClientConnInfo *clientConn =                                 \
         (ClientConnInfo *)(pluginData.clientConnWaitList->data);  \
      CloseClientConn(clientConn);                                 \
   }

static void
CloseVmxConn(void);

static void
CloseActiveConnections(void);

static void
HandleCurrentClientConnError(void);

static void
HandleVmxConnError(void);

static Bool
RecvHttpRequestFromCurrentClientConn(void *buf,  // OUT
                                     int len);   // IN

static Bool
StartRecvHttpRequestFromCurrentClientConn(void);

static inline void
StopRecvFromCurrentClientConn(void);

static Bool
SendToCurrentClientConn(void *buf,  // IN
                        int len);   // IN

static Bool
SendHttpResponseToCurrentClientConn(const char *headFmt,  // IN
                                    int64 contentLen,     // IN
                                    Bool shutdown);       // IN

#define SendHttpResponseOKToCurrentClientConn(contentSize)  \
   SendHttpResponseToCurrentClientConn(                     \
      HTTP_RES_OK,                                          \
      contentSize,                                          \
      (0 == contentSize ? TRUE : FALSE))

#define SendHttpResponseForbiddenToCurrentClientConn()   \
   SendHttpResponseToCurrentClientConn(                  \
      HTTP_RES_FORBIDDEN,                                \
      0,                                                 \
      TRUE)

#define SendHttpResponseNotFoundToCurrentClientConn()    \
   SendHttpResponseToCurrentClientConn(                  \
      HTTP_RES_NOT_FOUND,                                \
      0,                                                 \
      TRUE)

static Bool
SendConnectRequestToVmx(void);

static Bool
SendDataMapToVmxConn(void);

#define CheckSendShutdownDataMapToVmxConn()            \
   ASSERT(currentClientConn == NULL);                  \
   if (theVmxConn != NULL && !theVmxConn->shutDown) {  \
      SendDataMapToVmxConn();                          \
   }

#define CheckSendRequestDataMapToVmxConn()             \
   ASSERT(currentClientConn != NULL);                  \
   if (ReceivedHttpRequestFromCurrentClientConn() &&   \
       theVmxConn != NULL && !theVmxConn->shutDown) {  \
      SendDataMapToVmxConn();                          \
   }

static Bool
RecvDataMapFromVmxConn(void *buf,  // OUT
                       int len);   // IN

static inline void
StopRecvFromVmxConn(void);

static Bool
ProcessVmxDataMap(const DataMap *map);  // IN

static Bool
RecvContentFromVmxConn(void);

static void
StartCurrentClientConnRecvTimeout(void);

static inline void
StopClientConnRecvTimeout(ClientConnInfo *clientConn);  // IN

static inline void
StopCurrentClientConnRecvTimeout(void);

static Bool
CurrentClientConnRecvTimeoutCb(gpointer clientData);  // IN

static inline void
StartVmxToGuestConnTimeout(void);

static inline void
StopVmxToGuestConnTimeout(void);

static Bool
VmxToGuestConnTimeoutCb(gpointer clientData);  // IN

static inline void
StartConnInactivityTimeout(void);

static inline void
StopConnInactivityTimeout(void);

static Bool
ConnInactivityTimeoutCb(gpointer clientData);  // IN

static void
ClientConnErrorCb(int err,             // IN
                  AsyncSocket *asock,  // IN
                  void *clientData);   // IN

static void
CurrentClientConnSendCb(void *buf,           // IN
                        int len,             // IN
                        AsyncSocket *asock,  // IN
                        void *clientData);   // IN

static void
CurrentClientConnRecvHttpRequestCb(void *buf,           // IN
                                   int len,             // IN
                                   AsyncSocket *asock,  // IN
                                   void *clientData);   // IN

static void
ClientConnectCb(AsyncSocket *asock,  // IN
                void *clientData);   // IN

static void
VmxConnErrorCb(int err,             // IN
               AsyncSocket *asock,  // IN
               void *clientData);   // IN

static void
VmxConnSendDataMapCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData);   // IN

static void
VmxConnRecvDataMapCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData);   // IN

static void
VmxConnRecvContentCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData);   // IN

static void
VmxConnectCb(AsyncSocket *asock,  // IN
             void *clientData);   // IN


/*
 *-----------------------------------------------------------------------------
 *
 * StartServeNextClientConn --
 *
 *      Remove the next client connection from the waiting list, make it
 *      the current client connection and start receiving HTTP request
 *      from it.
 *
 *      If the waiting list is empty, initiate shutdown VMX connection.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
StartServeNextClientConn(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(currentClientConn == NULL);

   if (pluginData.clientConnWaitList != NULL) {
      currentClientConn = (ClientConnInfo *)
         (pluginData.clientConnWaitList->data);
      pluginData.clientConnWaitList = g_list_remove(
         pluginData.clientConnWaitList, currentClientConn);
      currentClientConn->isCurrent = TRUE;

      StartRecvHttpRequestFromCurrentClientConn();
   } else {
      CheckSendShutdownDataMapToVmxConn();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloseClientConn --
 *
 *      Close a client connection and remove its reference.
 *
 *      Note: AsyncSocket does not differentiate read/write errors yet and
 *      does not try to send any data to the other end on close, so pending
 *      send data is dropped when a connection is closed even the socket may
 *      be still good for write.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CloseClientConn(ClientConnInfo *clientConn)  // IN
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(clientConn != NULL);
   ASSERT(clientConn->asock != NULL);

   g_info("Closing client connection %d.\n",
          AsyncSocket_GetFd(clientConn->asock));

   AsyncSocket_Close(clientConn->asock);
   clientConn->asock = NULL;

   if (clientConn->buf != NULL) {
      free(clientConn->buf);
      clientConn->buf = NULL;
   }

   if (clientConn->requestPath != NULL) {
      free(clientConn->requestPath);
      clientConn->requestPath = NULL;
   }

   StopClientConnRecvTimeout(clientConn);

   if (clientConn->isCurrent) {
      ASSERT(currentClientConn == clientConn);
      /*
       * AsyncSocketSendFn (CurrentClientConnSendCb) can be invoked inside
       * AsyncSocket_Close().
       */
      currentClientConn = NULL;
   } else {
      /*
       * This client connection is in the waiting list.
       */
      pluginData.clientConnWaitList =
         g_list_remove(pluginData.clientConnWaitList, clientConn);
   }

   free(clientConn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloseVmxConn --
 *
 *      Close the VMX connection.
 *
 *      Note: AsyncSocket does not differentiate read/write errors yet and
 *      does not try to send any data to the other end on close, so pending
 *      send data is dropped when a connection is closed even the socket may
 *      be still good for write.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CloseVmxConn(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   if (theVmxConn == NULL) {
      return;
   }

   ASSERT(theVmxConn->asock != NULL);

   g_info("Closing VMX connection %d.\n",
          AsyncSocket_GetFd(theVmxConn->asock));

   /*
    * AsyncSocketSendFn (VmxConnSendDataMapCb) can be invoked inside
    * AsyncSocket_Close().
    */
   AsyncSocket_Close(theVmxConn->asock);
   theVmxConn->asock = NULL;

   if (theVmxConn->buf != NULL) {
      free(theVmxConn->buf);
      theVmxConn->buf = NULL;
   }

   StopConnInactivityTimeout();

   free(theVmxConn);
   theVmxConn = NULL;
   pluginData.vmxConnectRequested = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloseActiveConnections --
 *
 *      Close the current client connection and the VMX connection, force to
 *      restart from the next client connection in the waiting list if it
 *      exists.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CloseActiveConnections(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   CloseCurrentClientConn();

   if (theVmxConn != NULL && !theVmxConn->shutDown) {
      /*
       * After CloseCurrentClientConn(), send shutdown data map to VMX.
       */
      SendDataMapToVmxConn();
   } else {
      /*
       * Force to restart.
       */
      CloseVmxConn();
      StartServeNextClientConn();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HandleCurrentClientConnError --
 *
 *      Handle the current client connection error.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HandleCurrentClientConnError(void)
{
   Bool requestReceived;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   requestReceived = ReceivedHttpRequestFromCurrentClientConn();

   CloseCurrentClientConn();

   if (requestReceived) {
      /*
       * The VMX connection that serves the current client connection after
       * it has received HTTP request has to be reset too.
       */
      CheckSendShutdownDataMapToVmxConn();
   } else {
      /*
       * HTTP request not received from the current client connection yet,
       * the VMX connection is still clean.
       */
      StartServeNextClientConn();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HandleVmxConnError --
 *
 *      Handle the VMX connection error.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HandleVmxConnError(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   CloseVmxConn();

   /*
    * The current client connection being served after received HTTP request
    * has to be reset too.
    */
   CloseCurrentClientConnIfReceivedHttpRequest();

   if (pluginData.guestStoreAccessEnabled &&
       currentClientConn == NULL) {
      StartServeNextClientConn();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecvHttpRequestFromCurrentClientConn --
 *
 *      Receive HTTP request from the current client connection.
 *
 * Results:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RecvHttpRequestFromCurrentClientConn(void *buf,  // OUT
                                     int len)    // IN
{
   int res;

   g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   res = AsyncSocket_RecvPartial(currentClientConn->asock, buf, len,
                                 CurrentClientConnRecvHttpRequestCb,
                                 currentClientConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_RecvPartial failed "
                "on current client connection %d: %s\n",
                AsyncSocket_GetFd(currentClientConn->asock),
                AsyncSocket_Err2String(res));
      HandleCurrentClientConnError();
      return FALSE;
   }

   if (currentClientConn->timeoutSource == NULL) {
      StartCurrentClientConnRecvTimeout();
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartRecvHttpRequestFromCurrentClientConn --
 *
 *      Start receiving HTTP request, with timeout, from
 *      the current client connection.
 *
 * Results:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
StartRecvHttpRequestFromCurrentClientConn(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);
   ASSERT(currentClientConn->buf == NULL);

   currentClientConn->bufLen = CLIENT_CONN_SEND_RECV_BUF_SIZE;
   currentClientConn->buf = Util_SafeMalloc(currentClientConn->bufLen);

   return RecvHttpRequestFromCurrentClientConn(currentClientConn->buf,
                                               currentClientConn->bufLen);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopRecvFromCurrentClientConn --
 *
 *      Stop receiving from the current client connection, safe to call in
 *      the same connection recv callback.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopRecvFromCurrentClientConn(void)
{
   int res = AsyncSocket_CancelRecvEx(currentClientConn->asock,
                                      NULL, NULL, NULL, TRUE);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_CancelRecvEx failed "
                "on current client connection %d: %s\n",
                AsyncSocket_GetFd(currentClientConn->asock),
                AsyncSocket_Err2String(res));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendToCurrentClientConn --
 *
 *      Send to the current client connection.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SendToCurrentClientConn(void *buf,  // IN
                        int len)    // IN
{
   int res;

   //g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   res = AsyncSocket_Send(currentClientConn->asock, buf, len,
                          CurrentClientConnSendCb, currentClientConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_Send failed "
                "on current client connection %d: %s\n",
                AsyncSocket_GetFd(currentClientConn->asock),
                AsyncSocket_Err2String(res));
      HandleCurrentClientConnError();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendHttpResponseToCurrentClientConn --
 *
 *      Send HTTP response head to the current client connection.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SendHttpResponseToCurrentClientConn(const char *headFmt,  // IN
                                    int64 contentLen,     // IN
                                    Bool shutdown)        // IN
{
   gchar *utcStr;
   int len;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   utcStr = GetCurrentUtcStr();
   len = Str_Sprintf(currentClientConn->buf, currentClientConn->bufLen,
                     headFmt,
                     utcStr != NULL ? utcStr : "", contentLen);
   g_free(utcStr);

   currentClientConn->shutDown = shutdown;
   return SendToCurrentClientConn(currentClientConn->buf, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendConnectRequestToVmx --
 *
 *      Request VMX to connect to our VSOCK listening port via RPC command.
 *
 *      This function should be called when pluginData.vmxConnectRequested
 *      is FALSE.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      All outstanding client connections are closed if failed.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SendConnectRequestToVmx(void)
{
   Bool retVal;
   int fd;
   struct sockaddr_vm addr;
#ifdef _WIN32
   int       addrLen = (int)sizeof(addr);
#else
   socklen_t addrLen = (socklen_t)sizeof(addr);
#endif
   char msg[32]; // Longest string: "guestStore.connect 4294967295" (29 chars)
   int msgLen;
   char *result;
   size_t resultLen;
   RpcChannelType rpcChannelType;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(!pluginData.vmxConnectRequested);
   ASSERT(theVmxConn == NULL);
   ASSERT(pluginData.vmxListenSock != NULL);

   fd = AsyncSocket_GetFd(pluginData.vmxListenSock);

   /*
    * Get the listening port.
    */
#ifdef _WIN32
   /* coverity[negative_returns] */
   if (0 != getsockname((SOCKET)fd, (struct sockaddr *)&addr, &addrLen))
#else
   /* coverity[negative_returns] */
   if (0 != getsockname(fd, (struct sockaddr *)&addr, &addrLen))
#endif
   {
      g_warning("getsockname failed on VMX listening socket %d: sockerr=%d.\n",
                fd, sockerr());
      retVal = FALSE;
      goto exit;
   }

   msgLen = Str_Sprintf(msg, sizeof msg,
                        "guestStore.connect %u", addr.svm_port);
   result = NULL;
   rpcChannelType = RpcChannel_GetType(pluginData.ctx->rpc);
   g_debug("Current guest RPC channel type: %d.\n", rpcChannelType);

   /*
    * "guestStore.connect" is a privileged guest RPC that should
    * go through a privileged vSock RPC channel.
    */
   if (rpcChannelType == RPCCHANNEL_TYPE_PRIV_VSOCK) {
      retVal = RpcChannel_Send(pluginData.ctx->rpc, msg, msgLen,
                               &result, &resultLen);
   } else {
      /*
       * After the vmsvc RPC channel falls back to backdoor, it could not
       * send through privileged guest RPC any more.
       */
      retVal = RpcChannel_SendOneRawPriv(msg, msgLen,
                                         &result, &resultLen);
   }

   if (retVal) {
      g_info("Connect request sent to VMX (svm_port = %u).\n", addr.svm_port);
   } else {
      g_warning("Failed to send connect request to VMX (svm_port = %u): %s.\n",
                addr.svm_port,
                result != NULL ? result : "");
   }
   vm_free(result);

exit:
   if (!retVal) {
      CloseCurrentClientConn();
      CloseClientConnsInWait();
   } else {
      StartVmxToGuestConnTimeout();
   }

   pluginData.vmxConnectRequested = retVal;
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendDataMapToVmxConn --
 *
 *      Send a data map to the VMX connection.
 *
 *      After received request path from the current client connection, data
 *      map field GUESTSTORE_REQ_FLD_PATH with the request path is sent to
 *      the VMX connection. VMX will send back a response data map with error
 *      code.
 *
 *      When no more client to serve, initiate shutdown VMX connection by
 *      sending data map field GUESTSTORE_REQ_FLD_NONE to the VMX connection
 *      so that VMX side can close its vsocket.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SendDataMapToVmxConn(void)
{
   int fd;
   ErrorCode res;
   DataMap map;
   Bool mapCreated = FALSE;
   int cmdType;
   char *serBuf = NULL;
   uint32 serBufLen;
   int resSock;
   Bool retVal = FALSE;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   fd = AsyncSocket_GetFd(theVmxConn->asock);

   res = DataMap_Create(&map);
   if (res != DMERR_SUCCESS) {
      g_warning("DataMap_Create failed for VMX connection %d: error=%d.\n",
                fd, res);
      goto exit;
   }

   mapCreated = TRUE;

   if (currentClientConn == NULL) {
      /*
       * No client to serve, inform VMX side to close its vsocket proactively,
       * rather than waiting for ASOCKERR_REMOTE_DISCONNECT (4) error callback
       * which may never happen.
       */
      ASSERT(!theVmxConn->shutDown);

      theVmxConn->shutDown = TRUE;
      StopRecvFromVmxConn();
      cmdType = GUESTSTORE_REQ_CMD_CLOSE;
   } else {
      char *str;

      ASSERT(ReceivedHttpRequestFromCurrentClientConn());

      str = Util_SafeStrdup(currentClientConn->requestPath);
      res = DataMap_SetString(&map, GUESTSTORE_REQ_FLD_PATH, str, -1, TRUE);
      if (res != DMERR_SUCCESS) {
         g_warning("DataMap_SetString (field path) failed "
                   "for VMX connection %d: error=%d.\n", fd, res);
         free(str);
         goto exit;
      }

      cmdType = GUESTSTORE_REQ_CMD_GET;
   }

   res = DataMap_SetInt64(&map, GUESTSTORE_REQ_FLD_CMD, cmdType, TRUE);
   if (res != DMERR_SUCCESS) {
      g_warning("DataMap_SetInt64 (field cmd) failed "
                "for VMX connection %d: error=%d.\n", fd, res);
      goto exit;
   }

   res = DataMap_Serialize(&map, &serBuf, &serBufLen);
   if (res != DMERR_SUCCESS) {
      g_warning("DataMap_Serialize failed "
                "for VMX connection %d: error=%d.\n", fd, res);
      goto exit;
   }

   if (serBufLen > theVmxConn->bufLen) {
      g_warning("Data map to VMX connection %d is too large: length=%d.\n",
                fd, serBufLen);
      goto exit;
   }

   memcpy(theVmxConn->buf, serBuf, serBufLen);
   resSock = AsyncSocket_Send(theVmxConn->asock,
                              theVmxConn->buf, serBufLen,
                              VmxConnSendDataMapCb, theVmxConn);
   if (resSock != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_Send failed on VMX connection %d: %s\n",
                fd, AsyncSocket_Err2String(resSock));
      goto exit;
   }

   retVal = TRUE;

exit:
   if (mapCreated) {
      free(serBuf);
      DataMap_Destroy(&map);
   }

   if (!retVal) {
      HandleVmxConnError();
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecvDataMapFromVmxConn --
 *
 *      Start receiving data map from the VMX connection.
 *
 * Results:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RecvDataMapFromVmxConn(void *buf,  // OUT
                       int len)    // IN
{
   int res;

   g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   res = AsyncSocket_Recv(theVmxConn->asock, buf, len,
                          VmxConnRecvDataMapCb, theVmxConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_Recv failed on VMX connection %d: %s\n",
                AsyncSocket_GetFd(theVmxConn->asock),
                AsyncSocket_Err2String(res));
      HandleVmxConnError();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopRecvFromVmxConn --
 *
 *      Stop receiving from the VMX connection, safe to call in the same
 *      connection recv callback.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopRecvFromVmxConn(void)
{
   int res = AsyncSocket_CancelRecvEx(theVmxConn->asock,
                                      NULL, NULL, NULL, TRUE);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_CancelRecvEx failed on VMX connection %d: %s\n",
                AsyncSocket_GetFd(theVmxConn->asock),
                AsyncSocket_Err2String(res));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ProcessVmxDataMap --
 *
 *      Process the data map received from the VMX connection.
 *
 *      The data map should contain field GUESTSTORE_RES_FLD_ERROR_CODE. In
 *      success case, field GUESTSTORE_RES_FLD_CONTENT_SIZE should also exist
 *      with the content size and the data map is followed by content bytes.
 *
 * Results:
 *      TRUE on success, FALSE on error.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ProcessVmxDataMap(const DataMap *map)  // IN
{
   int fd;
   ErrorCode res;
   int64 errorCode;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   fd = AsyncSocket_GetFd(theVmxConn->asock);

   res = DataMap_GetInt64(map, GUESTSTORE_RES_FLD_ERROR_CODE, &errorCode);
   if (res != DMERR_SUCCESS) {
      g_warning("DataMap_GetInt64 (field error code) failed in data map "
                "from VMX connection %d: error=%d.\n", fd, res);
      goto error;
   }

   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   switch ((int32)errorCode) {
      case 0: // ERROR_SUCCESS
         {
            int64 contentSize;
            res = DataMap_GetInt64(map, GUESTSTORE_RES_FLD_CONTENT_SIZE,
                                   &contentSize);
            if (res != DMERR_SUCCESS) {
               g_warning("DataMap_GetInt64 (field content size) failed "
                         "in data map from VMX connection %d: error=%d.\n",
                         fd, res);
               goto error;
            }

            if (contentSize < 0) {
               g_warning("Invalid content size in data map "
                         "from VMX connection %d: contentSize=%" FMT64 "d.\n",
                         fd, contentSize);
               goto error;
            }

            theVmxConn->bytesRemaining = contentSize;
            return SendHttpResponseOKToCurrentClientConn(contentSize);
         }
      case EPERM:
         {
            return SendHttpResponseForbiddenToCurrentClientConn();
         }
      case ENOENT:
         {
            return SendHttpResponseNotFoundToCurrentClientConn();
         }
      default:
         g_warning("Unexpected error code value %" FMT64 "d in data map "
                   "from VMX connection %d.\n", errorCode, fd);
         break;
   }

error:
   HandleVmxConnError();
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecvContentFromVmxConn --
 *
 *      Start receiving content bytes from the VMX connection.
 *
 * Results:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RecvContentFromVmxConn(void)
{
   int res;

   //g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   res = AsyncSocket_RecvPartial(theVmxConn->asock,
                                 theVmxConn->buf,
                                 theVmxConn->bufLen,
                                 VmxConnRecvContentCb,
                                 theVmxConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_RecvPartial failed on VMX connection %d: %s\n",
                AsyncSocket_GetFd(theVmxConn->asock),
                AsyncSocket_Err2String(res));
      HandleVmxConnError();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartCurrentClientConnRecvTimeout --
 *
 *      Start the current client connection recv timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
StartCurrentClientConnRecvTimeout(void)
{
   int clientRecvTimeout;

   ASSERT(currentClientConn->timeoutSource == NULL);

   clientRecvTimeout = GUESTSTORE_CONFIG_GET_INT("clientRecvTimeout",
      DEFAULT_CLIENT_RECV_TIMEOUT);
   if (clientRecvTimeout <= 0 || clientRecvTimeout > (G_MAXINT / 1000)) {
      g_warning("Invalid clientRecvTimeout (%d); Using default (%d).\n",
                clientRecvTimeout, DEFAULT_CLIENT_RECV_TIMEOUT);
      clientRecvTimeout = DEFAULT_CLIENT_RECV_TIMEOUT;
   }

   currentClientConn->timeoutSource = g_timeout_source_new(
      clientRecvTimeout * 1000);
   VMTOOLSAPP_ATTACH_SOURCE(pluginData.ctx,
                            currentClientConn->timeoutSource,
                            CurrentClientConnRecvTimeoutCb,
                            currentClientConn, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopClientConnRecvTimeout --
 *
 *      Stop client connection recv timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopClientConnRecvTimeout(ClientConnInfo *clientConn)  // IN
{
   ASSERT(clientConn != NULL);

   if (clientConn->timeoutSource != NULL) {
      g_source_destroy(clientConn->timeoutSource);
      g_source_unref(clientConn->timeoutSource);
      clientConn->timeoutSource = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopCurrentClientConnRecvTimeout --
 *
 *      Stop the current client connection recv timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopCurrentClientConnRecvTimeout(void)
{
   StopClientConnRecvTimeout(currentClientConn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CurrentClientConnRecvTimeoutCb --
 *
 *      Poll callback function for the current client connection recv timeout.
 *
 * Results:
 *      The current client connection is closed.
 *      The timeout source is removed from poll.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CurrentClientConnRecvTimeoutCb(gpointer clientData)  // IN
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(currentClientConn == clientData);
   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   g_warning("The current client connection %d recv timed out.\n",
             AsyncSocket_GetFd(currentClientConn->asock));

   /*
    * Follow the pattern in ConnInactivityTimeoutCb()
    */
   StopCurrentClientConnRecvTimeout();

   HandleCurrentClientConnError();

   return G_SOURCE_REMOVE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartVmxToGuestConnTimeout --
 *
 *      Start VMX to guest connection timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StartVmxToGuestConnTimeout(void)
{
   ASSERT(pluginData.timeoutSource == NULL);

   pluginData.timeoutSource = g_timeout_source_new(
      GUESTSTORE_VMX_TO_GUEST_CONN_TIMEOUT * 1000);
   VMTOOLSAPP_ATTACH_SOURCE(pluginData.ctx,
                            pluginData.timeoutSource,
                            VmxToGuestConnTimeoutCb,
                            NULL, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopVmxToGuestConnTimeout --
 *
 *      Stop VMX to guest connection timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopVmxToGuestConnTimeout(void)
{
   if (pluginData.timeoutSource != NULL) {
      g_source_destroy(pluginData.timeoutSource);
      g_source_unref(pluginData.timeoutSource);
      pluginData.timeoutSource = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxToGuestConnTimeoutCb --
 *
 *      Poll callback function for VMX to guest connection timeout.
 *
 * Results:
 *      All outstanding client connections are closed.
 *      The timeout source is removed from poll.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VmxToGuestConnTimeoutCb(gpointer clientData)  // IN
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn == NULL);

   g_warning("VMX to guest connection timed out.\n");

   StopVmxToGuestConnTimeout();

   CloseCurrentClientConn();
   CloseClientConnsInWait();

   pluginData.vmxConnectRequested = FALSE;

   return G_SOURCE_REMOVE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartConnInactivityTimeout --
 *
 *      Start connection inactivity timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StartConnInactivityTimeout(void)
{
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->timeoutSource == NULL);
   ASSERT(theVmxConn->connTimeout != 0);

   theVmxConn->timeoutSource = g_timeout_source_new(
      theVmxConn->connTimeout * 1000);
   VMTOOLSAPP_ATTACH_SOURCE(pluginData.ctx,
                            theVmxConn->timeoutSource,
                            ConnInactivityTimeoutCb,
                            theVmxConn, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopConnInactivityTimeout --
 *
 *      Stop connection inactivity timeout.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
StopConnInactivityTimeout(void)
{
   ASSERT(theVmxConn != NULL);

   if (theVmxConn->timeoutSource != NULL) {
      g_source_destroy(theVmxConn->timeoutSource);
      g_source_unref(theVmxConn->timeoutSource);
      theVmxConn->timeoutSource = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConnInactivityTimeoutCb --
 *
 *      Poll callback function for connection inactivity timeout.
 *
 * Results:
 *      The VMX connection and the current client connection are closed.
 *      The timeout source is removed from poll.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ConnInactivityTimeoutCb(gpointer clientData)  // IN
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn == clientData);
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   g_warning("Connection inactivity timed out.\n");

   /*
    * Issue observed:
    * If g_source_destroy() is not called on the inactivity timeout source
    * and the next client connection in the waiting list becomes current
    * and starts its new recv timeout source, g_main_dispatch() does not
    * remove the inactivity timeout source after this callback returns
    * G_SOURCE_REMOVE (FALSE).
    *
    * Solution:
    * Call g_source_destroy() before the new timeout source starts.
    * After this callback returns G_SOURCE_REMOVE (FALSE), g_main_dispatch()
    * detects the inactivity timeout source destroyed and skips same action.
    */
   StopConnInactivityTimeout();

   CloseActiveConnections();

   return G_SOURCE_REMOVE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ClientConnErrorCb --
 *
 *      Client connection error handler for asyncsocket.
 *
 * Results:
 *      The connection is closed.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ClientConnErrorCb(int err,             // IN
                  AsyncSocket *asock,  // IN
                  void *clientData)    // IN
{
   ClientConnInfo *clientConn = (ClientConnInfo *)clientData;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(clientConn->asock != NULL);
   g_info("Client connection %d error callback: %s\n",
          AsyncSocket_GetFd(clientConn->asock),
          AsyncSocket_Err2String(err));

   if (clientConn->isCurrent) {
      ASSERT(currentClientConn == clientConn);
      HandleCurrentClientConnError();
   } else {
      CloseClientConn(clientConn);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CurrentClientConnSendCb --
 *
 *      Callback function after sent to the current client connection.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CurrentClientConnSendCb(void *buf,           // IN
                        int len,             // IN
                        AsyncSocket *asock,  // IN
                        void *clientData)    // IN
{
   //g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(currentClientConn == clientData);
   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   if (AsyncSocket_GetState(currentClientConn->asock) != 
       AsyncSocketConnected) {
      /*
       * This callback may be called after the connection is closed for
       * freeing the send buffer.
       */
      return;
   }

   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->timeoutSource != NULL);

   /*
    * Restart connection inactivity timeout.
    */
   StopConnInactivityTimeout();
   StartConnInactivityTimeout();

   if (currentClientConn->shutDown) {
      g_info("Finished with current client connection %d.\n",
             AsyncSocket_GetFd(currentClientConn->asock));

      CloseCurrentClientConn();
      StartServeNextClientConn();
   } else {
      ASSERT(theVmxConn->bytesRemaining > 0);

      RecvContentFromVmxConn();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CurrentClientConnRecvHttpRequestCb --
 *
 *      Callback function after received from the current client connection.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CurrentClientConnRecvHttpRequestCb(void *buf,           // IN
                                   int len,             // IN
                                   AsyncSocket *asock,  // IN
                                   void *clientData)    // IN
{
   int fd;
   int recvLen;
   char *next_token;
   char *requestMethod;
   char *requestPath;

   g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(currentClientConn == clientData);
   ASSERT(currentClientConn != NULL);
   ASSERT(currentClientConn->asock != NULL);

   fd = AsyncSocket_GetFd(currentClientConn->asock);

   recvLen = (int)((char *)buf - currentClientConn->buf) + len;
   if (recvLen >= currentClientConn->bufLen) {
      g_warning("Recv from current client connection %d "
                "reached buffer limit.\n", fd);
      goto error;
   }

   /*
    * Check for HTTP request end.
    */
   if (recvLen < HTTP_HEADER_END_LEN ||
       strncmp(currentClientConn->buf + recvLen - HTTP_HEADER_END_LEN,
               HTTP_HEADER_END, HTTP_HEADER_END_LEN) != 0) {
      RecvHttpRequestFromCurrentClientConn(currentClientConn->buf +
                                           recvLen,
                                           currentClientConn->bufLen -
                                           recvLen);
      return;
   }

   StopCurrentClientConnRecvTimeout();

   *(currentClientConn->buf + recvLen) = '\0';
   g_debug("HTTP request from current client connection %d:\n%s\n",
           fd, currentClientConn->buf);

   requestMethod = strtok_r(currentClientConn->buf, " ", &next_token);
   if (NULL == requestMethod ||
       strcmp(requestMethod, HTTP_REQ_METHOD_GET) != 0) {
      g_warning("Invalid HTTP request method.\n");
      goto error;
   }

   /*
    * Ignore HTTP query part.
    */
   requestPath = strtok_r(NULL, "? ", &next_token);
   if (NULL == requestPath) {
      g_warning("HTTP request path not found.\n");
      goto error;
   }

   currentClientConn->requestPath = g_uri_unescape_string(requestPath, NULL);
   if (NULL == currentClientConn->requestPath ||
       '/' != *currentClientConn->requestPath ||
       strlen(currentClientConn->requestPath) > GUESTSTORE_CONTENT_PATH_MAX) {
      g_warning("Invalid HTTP request path.\n");
      goto error;
   }

   g_info("HTTP request path from current client connection %d: \"%s\"",
          fd, currentClientConn->requestPath);

   StopRecvFromCurrentClientConn();

   if (!pluginData.vmxConnectRequested) {
      ASSERT(theVmxConn == NULL);
      SendConnectRequestToVmx();
   } else {
      CheckSendRequestDataMapToVmxConn();
   }

   return;

error:
   HandleCurrentClientConnError();
}


/*
 *-----------------------------------------------------------------------------
 *
 * ClientConnectCb --
 *
 *      Poll callback function for a new client connection.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ClientConnectCb(AsyncSocket *asock,  // IN
                void *clientData)    // IN
{
   int fd = AsyncSocket_GetFd(asock);
   int maxConnections;
   ClientConnInfo *clientConn = NULL;
   int res;

   g_debug("Entering %s.\n", __FUNCTION__);
   g_info("Got new client connection %d.\n", fd);

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      g_info("Client connection %d is not in connected state.\n", fd);
      goto error;
   }

   maxConnections = GUESTSTORE_CONFIG_GET_INT("maxConnections",
      DEFAULT_MAX_CLIENT_CONNECTIONS);
   if ((g_list_length(pluginData.clientConnWaitList) +
        ((currentClientConn != NULL) ? 1 : 0)) >= maxConnections) {
      g_info("Client connection %d has exceeded maximum limit "
             "of %d client connections.\n", fd, maxConnections);
      goto error;
   }

#ifdef _WIN32
   CheckAndUpdateAdminOnly();

   /* coverity[negative_returns] */
   if (pluginData.adminOnly && !IsAdminClient(fd)) {
      g_info("Decline non admin/root client connection %d.\n", fd);
      goto error;
   }
#endif

   if (!AsyncSocket_EstablishMinBufferSizes(asock,
           GUESTSTORE_RESPONSE_BUFFER_SIZE, // sendSz
           GUESTSTORE_REQUEST_BUFFER_SIZE)) { // recvSz
      g_warning("AsyncSocket_EstablishMinBufferSizes failed "
                "on client connection %d.\n", fd);
      goto error;
   }

   clientConn = (ClientConnInfo *)Util_SafeCalloc(1, sizeof *clientConn);
   clientConn->asock = asock;

   res = AsyncSocket_SetErrorFn(asock, ClientConnErrorCb, clientConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_SetErrorFn failed on client connection %d: %s\n",
                fd, AsyncSocket_Err2String(res));
      goto error;
   }

   if (currentClientConn == NULL) {
      /*
       * Make the first client connection be the current client connection.
       */
      currentClientConn = clientConn;
      currentClientConn->isCurrent = TRUE;
      StartRecvHttpRequestFromCurrentClientConn();
   } else {
      pluginData.clientConnWaitList = g_list_append(
         pluginData.clientConnWaitList, clientConn);
   }

   return;

error:
   g_info("Closing client connection %d.\n", fd);
   AsyncSocket_Close(asock);
   free(clientConn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnErrorCb --
 *
 *      The VMX connection error handler for asyncsocket.
 *
 * Results:
 *      The connection is closed.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VmxConnErrorCb(int err,             // IN
               AsyncSocket *asock,  // IN
               void *clientData)    // IN
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(theVmxConn == clientData);
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);
   g_info("VMX connection %d error callback: %s\n",
          AsyncSocket_GetFd(theVmxConn->asock),
          AsyncSocket_Err2String(err));

   HandleVmxConnError();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnSendDataMapCb --
 *
 *      Callback function after sent to the VMX connection.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VmxConnSendDataMapCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData)    // IN
{
   int fd;

   g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(theVmxConn == clientData);
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   fd = AsyncSocket_GetFd(theVmxConn->asock);

   if (AsyncSocket_GetState(theVmxConn->asock) != AsyncSocketConnected) {
      /*
       * This callback may be called after the connection is closed for
       * freeing the send buffer.
       */
      return;
   }

   if (theVmxConn->shutDown) {
      g_info("Shut down VMX connection %d.\n", fd);
      CloseVmxConn();

      if (pluginData.guestStoreAccessEnabled) {
         if (currentClientConn == NULL) {
            StartServeNextClientConn();
         } else if (ReceivedHttpRequestFromCurrentClientConn()) {
            SendConnectRequestToVmx();
         }
      }
   } else {
      RecvDataMapFromVmxConn(&theVmxConn->dataMapLen,
                             (int)sizeof(theVmxConn->dataMapLen));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnRecvDataMapCb --
 *
 *      Callback function after received data map from the VMX connection.
 *
 *      VMX responds with a data map, followed by content bytes if no error.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VmxConnRecvDataMapCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData)    // IN
{
   int fd;

   g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(theVmxConn == clientData);
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   fd = AsyncSocket_GetFd(theVmxConn->asock);

   if (buf == &theVmxConn->dataMapLen) {
      int dataMapLen = ntohl(theVmxConn->dataMapLen);

      ASSERT(len == sizeof theVmxConn->dataMapLen);

      if (dataMapLen > (theVmxConn->bufLen - sizeof theVmxConn->dataMapLen)) {
         g_warning("Data map from VMX connection %d "
                   "is too large: length=%d.\n", fd, dataMapLen);
         goto error;
      }

      *((int32 *)(theVmxConn->buf)) = theVmxConn->dataMapLen;
      RecvDataMapFromVmxConn(theVmxConn->buf + sizeof theVmxConn->dataMapLen,
                             dataMapLen);
   } else {
      ErrorCode res;
      DataMap map;

      ASSERT(buf == (theVmxConn->buf + sizeof theVmxConn->dataMapLen));
      ASSERT(len == ntohl(theVmxConn->dataMapLen));

      res = DataMap_Deserialize(theVmxConn->buf,
                                len + (int)sizeof(theVmxConn->dataMapLen),
                                &map);
      if (res != DMERR_SUCCESS) {
         g_warning("DataMap_Deserialize failed for data map "
                   "from VMX connection %d: error=%d.\n", fd, res);
         goto error;
      }

      StopRecvFromVmxConn();
      ProcessVmxDataMap(&map);
      DataMap_Destroy(&map);
   }

   return;

error:
   HandleVmxConnError();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnRecvContentCb --
 *
 *      Callback function after received content bytes from the VMX connection.
 *
 *      VMX responds with a data map, followed by content bytes if no error.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VmxConnRecvContentCb(void *buf,           // IN
                     int len,             // IN
                     AsyncSocket *asock,  // IN
                     void *clientData)    // IN
{
   //g_debug("Entering %s: len=%d.\n", __FUNCTION__, len);

   ASSERT(theVmxConn == clientData);
   ASSERT(theVmxConn != NULL);
   ASSERT(theVmxConn->asock != NULL);

   theVmxConn->bytesRemaining -= len;
   if (theVmxConn->bytesRemaining < 0) {
      g_warning("Recv from VMX connection %d exceeded content size.\n",
                AsyncSocket_GetFd(theVmxConn->asock));
      HandleVmxConnError();
      return;
   }

   StopRecvFromVmxConn();

   if (theVmxConn->bytesRemaining == 0) {
      currentClientConn->shutDown = TRUE;
   }

   SendToCurrentClientConn(buf, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnectCb --
 *
 *      Poll callback function for a new VMX connection.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *-----------------------------------------------------------------------------
 */

static void
VmxConnectCb(AsyncSocket *asock,  // IN
             void *clientData)    // IN
{
   int fd = AsyncSocket_GetFd(asock);
   int res;

   g_debug("Entering %s.\n", __FUNCTION__);
   g_info("Got new VMX connection %d.\n", fd);

   StopVmxToGuestConnTimeout();

   if (!pluginData.vmxConnectRequested) {
      g_warning("Closing the unexpected VMX connection %d.\n", fd);
      AsyncSocket_Close(asock);
      return;
   }

   if (theVmxConn != NULL) {
      g_warning("The VMX connection already exists, closing the extra "
                "VMX connection %d.\n", fd);
      AsyncSocket_Close(asock);
      return;
   }

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      g_info("VMX connection %d is not in connected state.\n", fd);
      goto error;
   }

   if (!AsyncSocket_EstablishMinBufferSizes(asock,
           GUESTSTORE_REQUEST_BUFFER_SIZE, // sendSz
           GUESTSTORE_RESPONSE_BUFFER_SIZE)) { // recvSz
      g_warning("AsyncSocket_EstablishMinBufferSizes failed "
                "on VMX connection %d.\n", fd);
      goto error;
   }

   theVmxConn = (VmxConnInfo *)Util_SafeCalloc(1, sizeof *theVmxConn);

   theVmxConn->asock = asock;

   res = AsyncSocket_SetErrorFn(asock, VmxConnErrorCb, theVmxConn);
   if (res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_SetErrorFn failed "
                "on VMX connection %d: %s\n",
                fd, AsyncSocket_Err2String(res));
      goto error;
   }

   theVmxConn->bufLen = VMX_CONN_SEND_RECV_BUF_SIZE;
   theVmxConn->buf = Util_SafeMalloc(theVmxConn->bufLen);

   theVmxConn->connTimeout = GUESTSTORE_CONFIG_GET_INT("connTimeout",
      GUESTSTORE_DEFAULT_CONN_TIMEOUT);
   if (theVmxConn->connTimeout <= 0 ||
       theVmxConn->connTimeout > (G_MAXINT / 1000)) {
      g_warning("Invalid connTimeout (%d); Using default (%d).\n",
                theVmxConn->connTimeout, GUESTSTORE_DEFAULT_CONN_TIMEOUT);
      theVmxConn->connTimeout = GUESTSTORE_DEFAULT_CONN_TIMEOUT;
   }

   StartConnInactivityTimeout();

   if (currentClientConn == NULL) {
      StartServeNextClientConn();
   } else {
      CheckSendRequestDataMapToVmxConn();
   }

   return;

error:
   g_info("Closing VMX connection %d.\n", fd);
   AsyncSocket_Close(asock);
   if (theVmxConn != NULL) {
      free(theVmxConn);
      theVmxConn = NULL;
   }

   CloseCurrentClientConn();
   CloseClientConnsInWait();
   pluginData.vmxConnectRequested = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateVmxListenSocket --
 *
 *      Create listening vsocket to accept connection from VMX.
 *      The auto-assigned port number will be sent to VMX via guest RPC.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CreateVmxListenSocket(void)
{
   int res = ASOCKERR_SUCCESS;
   AsyncSocket *asock;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(pluginData.vmxListenSock == NULL);

   asock = AsyncSocket_ListenVMCI(VMCISock_GetLocalCID(), VMADDR_PORT_ANY,
                                  VmxConnectCb, NULL, NULL, &res);
   if (NULL == asock || res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_ListenVMCI failed: %s\n",
                AsyncSocket_Err2String(res));
      if (asock != NULL) {
         AsyncSocket_Close(asock);
      }
      return FALSE;
   }

   pluginData.vmxListenSock = asock;

   return TRUE;
}


#ifdef _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * CreateClientListenSocket --
 *
 *      Create listening socket to accept connections from clients.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CreateClientListenSocket(void)
{
   AsyncSocket *asock = NULL;
   uint16 port;
   PortUsage portUseMap[GUESTSTORE_LOOPBACK_PORT_MAX -
                        GUESTSTORE_LOOPBACK_PORT_MIN + 1] = { 0 };

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(pluginData.clientListenSock == NULL);

   /*
    * Use output of GetPortUseMap as a hint, it does not matter
    * if GetPortUseMap fails.
    */
   GetPortUseMap(GUESTSTORE_LOOPBACK_PORT_MIN,
                 GUESTSTORE_LOOPBACK_PORT_MAX,
                 portUseMap);

   for (port = GUESTSTORE_LOOPBACK_PORT_MIN;
        port <= GUESTSTORE_LOOPBACK_PORT_MAX;
        port++) {
      int res = ASOCKERR_SUCCESS;
      PortUsage *portUse = &portUseMap[port - GUESTSTORE_LOOPBACK_PORT_MIN];

      /*
       * Use || instead of && to avoid confusion to see a port used by
       * one service on tcp but another service on tcp6.
       */
      if (portUse->inet4 || portUse->inet6) {
         continue;
      }

      asock = AsyncSocket_ListenLoopback(port, ClientConnectCb, NULL, NULL, &res);
      if (asock != NULL) {
         break;
      }

      if (res == ASOCKERR_BINDADDRINUSE || res == ASOCK_EADDRINUSE) {
         g_info("Port %u is already in use.\n", port);
      } else {
         g_warning("AsyncSocket_ListenLoopback failed on port %u: %s\n",
                   port, AsyncSocket_Err2String(res));
         break;
      }
   }

   if (asock == NULL) {
      return FALSE;
   }

   pluginData.clientListenSock = asock;
   return TRUE;
}

#else

/*
 *-----------------------------------------------------------------------------
 *
 * CreateSocketDir --
 *
 *      Create the UNIX domain socket directory with proper permissions.
 *
 * Results:
 *      Return TRUE if the directory is created or already exists, and the
 *      permissions are set properly. Otherwise FALSE.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CreateSocketDir(const char *sockDir)  // IN
{
   Bool retVal;
   int mode = 0755;  // Same mode as VGAuth service socket dir
   struct stat st;

   ASSERT(sockDir != NULL && *sockDir != '\0');

   retVal = File_EnsureDirectoryEx(sockDir, mode);
   if (!retVal) {
      g_warning("Unable to create folder %s: error=%d.\n",
                sockDir, errno);
      goto exit;
   }

   /*
    * Verify the directory owner and permissions.
    */
   if (Posix_Lstat(sockDir, &st) != 0) {
      g_warning("Unable to retrieve the attributes of %s: error=%d.\n",
                sockDir, errno);
      retVal = FALSE;
      goto exit;
   }

   if (st.st_uid != getuid()) {
      g_warning("%s has the wrong owner.\n", sockDir);
      retVal = FALSE;
      goto exit;
   }

   if ((st.st_mode & 0777) != mode &&
       !File_SetFilePermissions(sockDir, (st.st_mode & 07000) | mode)) {
      g_warning("%s has improper permissions.\n", sockDir);
      retVal = FALSE;
   }

exit:
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AdjustSocketFilePermissions --
 *
 *      Adjust the UNIX domain socket file permissions to control who can
 *      connect.
 *
 * Results:
 *      Return TRUE if the permissions are set properly. Otherwise FALSE.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
AdjustSocketFilePermissions(const char *sockFile,     // IN
                            Bool onlyRootCanConnect)  // IN
{
   Bool retVal = FALSE;
   int mode;
   struct stat st;

   ASSERT(sockFile != NULL && *sockFile != '\0');

   /*
    * Add sticky bit if everyone can connect.
    */
   mode = (onlyRootCanConnect ? 0755 : 01777);

   if (Posix_Lstat(sockFile, &st) != 0) {
      g_warning("Unable to retrieve the attributes of %s: error=%d.\n",
                sockFile, errno);
      goto exit;
   }

   if ((st.st_mode & 01777) != mode &&
       !File_SetFilePermissions(sockFile, (st.st_mode & 07000) | mode)) {
      g_warning("%s has improper permissions.\n", sockFile);
      goto exit;
   }

   retVal = TRUE;

exit:
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateClientListenSocket --
 *
 *      Create listening socket to accept connections from clients.
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CreateClientListenSocket(void)
{
   int res = ASOCKERR_SUCCESS;
   AsyncSocket *asock;

   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(pluginData.clientListenSock == NULL);

   CheckAndUpdateAdminOnly();

   if (!CreateSocketDir(GUESTSTORE_PIPE_DIR)) {
      g_warning("CreateSocketDir failed.\n");
      return FALSE;
   }

   File_Unlink(GUESTSTORE_PIPE_NAME);

   asock = AsyncSocket_ListenSocketUDS(
      GUESTSTORE_PIPE_NAME,
      ClientConnectCb, NULL,
      NULL, &res);

   if (asock == NULL || res != ASOCKERR_SUCCESS) {
      g_warning("AsyncSocket_ListenSocketUDS failed: %s\n",
                AsyncSocket_Err2String(res));

      if (asock != NULL) {
         AsyncSocket_Close(asock);
      }

      return FALSE;
   }

   /*
    * Ideally, this should be done after bind() and before listen() and
    * accept(). Since asyncsocket library shares TCP socket implementation
    * code, there is no such interface to do it. Doing it here is fine,
    * because the initial permission settings allow root to connect only.
    */
   if (!AdjustSocketFilePermissions(GUESTSTORE_PIPE_NAME,
                                    pluginData.adminOnly)) {
      g_warning("AdjustSocketFilePermissions failed.\n");
      AsyncSocket_Close(asock);
      return FALSE;
   }

   pluginData.clientListenSock = asock;
   return TRUE;
}

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * InitPluginData --
 *
 *      Init pluginData structure.
 *      - 'ctx': ToolsAppCtx
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
InitPluginData(ToolsAppCtx *ctx)  // IN
{
   pluginData.ctx = ctx;
   CheckAndUpdateFeatureDisabled();
   CheckAndUpdateAdminOnly();
}


/*
 *-----------------------------------------------------------------------------
 *
 * InitPluginSignals --
 *
 *      Init signals for notification.
 *      - 'ctx': ToolsAppCtx
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
InitPluginSignals(ToolsAppCtx* ctx)  // IN
{
   /* Register the signals we'll use to notify people interested in this event. */
   g_signal_new(TOOLS_CORE_SIG_GUESTSTORE_STATE,
                G_OBJECT_TYPE(ctx->serviceObj),
                0,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__BOOLEAN,
                G_TYPE_NONE,
                1,
                G_TYPE_BOOLEAN);
}


/*
 *----------------------------------------------------------------------------
 *
 * GuestStoreAccessDisable --
 *
 *     Close all sockets/connections and reset plugin internal states.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

static void
GuestStoreAccessDisable(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   if (!pluginData.shutdown) {
      g_signal_emit_by_name(pluginData.ctx->serviceObj,
                            TOOLS_CORE_SIG_GUESTSTORE_STATE,
                            FALSE);
   }

   pluginData.guestStoreAccessEnabled = FALSE;

   if (pluginData.vmxListenSock) {
      AsyncSocket_Close(pluginData.vmxListenSock);
      pluginData.vmxListenSock = NULL;
   }

   if (pluginData.clientListenSock) {
      AsyncSocket_Close(pluginData.clientListenSock);
      pluginData.clientListenSock = NULL;
   }

   CloseCurrentClientConn();
   CloseClientConnsInWait();

   if (theVmxConn != NULL && !theVmxConn->shutDown) {
      /*
       * After CloseCurrentClientConn(), send shutdown data map to VMX.
       */
      SendDataMapToVmxConn();
   } else {
      /*
       * Force to stop.
       */
      CloseVmxConn();
      StopVmxToGuestConnTimeout();
      pluginData.vmxConnectRequested = FALSE;  // To make sure
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * GuestStoreAccessEnable --
 *
 *     Create the sockets and start listening.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

static void
GuestStoreAccessEnable(void)
{
   g_debug("Entering %s.\n", __FUNCTION__);

   ASSERT(!pluginData.guestStoreAccessEnabled);

   if (!CreateVmxListenSocket() || !CreateClientListenSocket()) {
      g_warning("GuestStore access is disabled "
                "due to initialization error.\n");
      GuestStoreAccessDisable();
      return;
   }

   pluginData.guestStoreAccessEnabled = TRUE;
   g_signal_emit_by_name(pluginData.ctx->serviceObj,
                         TOOLS_CORE_SIG_GUESTSTORE_STATE,
                         TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetVmxGuestStoreAccessEnabledState --
 *
 *      Send a GuestRpc command to VMX to retrieve guestStore.accessEnabled
 *      state.
 *
 * Result:
 *      TRUE : access enabled
 *      FALSE: access disabled
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GetVmxGuestStoreAccessEnabledState(void)
{
   Bool retVal;
   const char *msg = "guestStore.accessEnabled";
   char *result = NULL;
   size_t resultLen;

   retVal = RpcChannel_Send(pluginData.ctx->rpc, msg, strlen(msg),
                            &result, &resultLen);
   if (retVal) {
      retVal = (strcmp(result, "true") == 0 ? TRUE : FALSE);
   } else {
      g_warning("Failed to send accessEnabled message to VMX: %s.\n",
                result != NULL ? result : "");
   }

   vm_free(result);

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreShutdown --
 *
 *      Disable GuestStore access before shutdown.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestStoreShutdown(void)
{
   pluginData.shutdown = TRUE;
   g_object_set(pluginData.ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GUESTSTORE,
                NULL, NULL);

   if (pluginData.guestStoreAccessEnabled) {
      GuestStoreAccessDisable();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreConfReload --
 *
 *      Disable/enable GuestStore access after guest side config change.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestStoreConfReload(gpointer src,      // IN
                     ToolsAppCtx *ctx,  // IN
                     gpointer data)     // IN
{
   Bool featureDisabled = IsFeatureDisabled();

   if (pluginData.featureDisabled != featureDisabled) {
      pluginData.featureDisabled = featureDisabled;

      if (pluginData.guestStoreAccessEnabled && featureDisabled) {
         g_info("Disable GuestStore access after guest side "
                "config change.\n");
         GuestStoreAccessDisable();
      } else if (!pluginData.guestStoreAccessEnabled &&
                 !featureDisabled &&
                 GetVmxGuestStoreAccessEnabledState()) {
         g_info("Enable GuestStore access after guest side "
                "config change.\n");
         GuestStoreAccessEnable();
      }
   } else {
      Bool adminOnly = IsAdminOnly();

      if (pluginData.adminOnly != adminOnly) {
         pluginData.adminOnly = adminOnly;

         if (pluginData.guestStoreAccessEnabled) {
            g_info("Reset GuestStore access after guest side "
                   "config change.\n");
            GuestStoreAccessDisable();
            GuestStoreAccessEnable();
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreReset --
 *
 *      VMX connection ASOCKERR_REMOTE_DISCONNECT (4) error callback is not
 *      seen on Windows guests after suspend/resume, address this in tools
 *      reset signal handler.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      Client connections will also be interrupted by tools reset due to
 *      sporadic guest hang.
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestStoreReset(gpointer src,      // IN
                ToolsAppCtx *ctx,  // IN
                gpointer data)     // IN
{
   if (theVmxConn != NULL) {
#ifdef _WIN32
      /*
       * After suspend/resume, VMX side vsocket is closed, VMX connection is
       * broken, but VmxConnErrorCb() is not called on Windows guests.
       * We still send shutdown data map to VMX connection here. We see
       * AsyncSocket_Send() succeeds and either VmxConnSendDataMapCb or
       * VmxConnErrorCb() is called in tests. This minimizes impact on
       * sporadic guest hang case where VMX connection is not broken and
       * we want VMX to close its side vsocket proactively.
       */
      g_info("Perform tools reset by closing active connections.\n");
      CloseActiveConnections();
#endif
   } else if (pluginData.vmxConnectRequested) {
      /*
       * Closing pluginData.vmxListenSock cancels pending VmxConnectCb() call,
       * second call of AsyncSocket_ListenVMCI() results in a new vsocket
       * listening port number.
       */
      g_info("Perform tools reset without VMX connection "
             "but VMX connect request was made.\n");
      GuestStoreAccessDisable(); // Calls StopVmxToGuestConnTimeout()
      if (pluginData.guestStoreAccessEnabled &&
          !CheckAndUpdateFeatureDisabled()) {
         GuestStoreAccessEnable();
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * GuestStoreSetOption --
 *
 *      Handle TOOLSOPTION_ENABLE_GUESTSTORE_ACCESS Set_Option callback.
 *
 * Results:
 *      TRUE on success.
 *
 * Side-effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

static gboolean
GuestStoreSetOption(gpointer src,         // IN
                    ToolsAppCtx *ctx,     // IN
                    const gchar *option,  // IN
                    const gchar *value,   // IN
                    gpointer data)        // IN
{
   gboolean retVal = FALSE;

   if (strcmp(option, TOOLSOPTION_ENABLE_GUESTSTORE_ACCESS) == 0) {
      g_debug("Tools set option %s=%s.\n",
              TOOLSOPTION_ENABLE_GUESTSTORE_ACCESS, value);

      if (strcmp(value, "1") == 0 &&
          !pluginData.guestStoreAccessEnabled) {
         if (CheckAndUpdateFeatureDisabled()) {
            g_info("GuestStore access is disabled on guest side.\n");
         } else {
            GuestStoreAccessEnable();
            retVal = TRUE;
         }
      } else if (strcmp(value, "0") == 0 &&
                 pluginData.guestStoreAccessEnabled) {
         GuestStoreAccessDisable();
         retVal = TRUE;
      }
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsOnLoad --
 *
 *      Return the registration data for the GuestStore plugin.
 *
 * Results:
 *      Return the registration data.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)  // IN
{
   static ToolsPluginData regData = { "guestStore", NULL, NULL, NULL };
   static ToolsPluginSvcGuestStore svcGuestStore = { GuestStoreShutdown };

   ToolsServiceProperty propGuestStore = { TOOLS_PLUGIN_SVC_PROP_GUESTSTORE };

   uint32 vmxVersion = 0;
   uint32 vmxType = VMX_TYPE_UNSET;

   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_CONF_RELOAD, GuestStoreConfReload, NULL },
      { TOOLS_CORE_SIG_RESET, GuestStoreReset, NULL },
      { TOOLS_CORE_SIG_SET_OPTION, GuestStoreSetOption, NULL }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_SIGNALS,
        VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   /*
    * Return NULL to disable the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("Not running in vmsvc daemon: container name='%s'.\n",
             ctx->name);
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("Not running in a VMware VM.\n");
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if VM is not running on ESX host.
    */
   if (!VmCheck_GetVersion(&vmxVersion, &vmxType) ||
       vmxType != VMX_TYPE_SCALABLE_SERVER) {
      g_info("VM is not running on ESX host.\n");
      return NULL;
   }

   InitPluginData(ctx);
   InitPluginSignals(ctx);
   Poll_InitGtk();

   ctx->registerServiceProperty(ctx->serviceObj, &propGuestStore);
   g_object_set(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GUESTSTORE,
                &svcGuestStore, NULL);

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   return &regData;
}

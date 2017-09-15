/*********************************************************
 * Copyright (C) 2012-2016 VMware, Inc. All rights reserved.
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
 * @file grabbitmqProxyPlugin.c
 *
 * Guest RabbitMQ proxy, routing traffic to VMX RabbitMQ proxy.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#define G_LOG_DOMAIN "grabbitmqProxy"

#include "vm_assert.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
#include "rpcout.h"
#include "rabbitmqProxyConst.h"
#include "vm_basic_types.h"
#include "poll.h"
#ifdef OPEN_VM_TOOLS
#include "vmci_sockets.h"
#include "sslDirect.h"
#else
#include "vsockCommon.h"
#include "ssl.h"
#endif
#include "asyncsocket.h"
#include "str.h"
#include "util.h"
#include "guestApp.h"

#include <openssl/ssl.h>

VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);

#ifndef SOCKET_ERROR
#define SOCKET_ERROR        (-1)
#endif

#ifndef sockerr
#define sockerr()           errno
#endif

#define GUEST_RABBITMQ_PROXY_VERSION             "1.0"
#define CONFGROUP_GRABBITMQ_PROXY                "grabbitmqproxy"

#define DEFAULT_MAX_SEND_QUEUE_LEN               (256 * 1024)

/*user level recv buffer */
#define RMQ_CLIENT_CONN_RECV_BUFF_SIZE           (64 * 1024)

/* these are socket level send/recv buffers */
#define DEFAULT_RMQCLIENT_CONN_RECV_BUFF_SIZE    (64 * 1024)
#define DEFAULT_RMQCLIENT_CONN_SEND_BUFF_SIZE    (64 * 1024)
#define DEFAULT_VMX_CONN_RECV_BUFF_SIZE          (64 * 1024)
#define DEFAULT_VMX_CONN_SEND_BUFF_SIZE          (64 * 1024)

#define VC_UUID_SIZE 36

/*  container for each connection details */
typedef struct _ConnInfo {
   Bool isRmqClient;
   AsyncSocket *asock;
   AsyncSocketRecvFn recvCb;
   AsyncSocketSendFn sendCb;
   AsyncSocketErrorFn errorCb;

   gboolean shutDown;

   int32 packetLen;
   char *recvBuf;
   int recvBufLen;

   int sendQueueLen;

   gboolean recvStopped;

   struct _ConnInfo *toConn;  /* the corresponding vmx connection for RabbitMq
                                 client connection, or vice versa. */
} ConnInfo;

typedef struct {
   AsyncSocket *vmxListenSock;   /* for vsocket connection from VMX */
   AsyncSocket *rmqListenSock;   /* for connections from RabbitMQ clients */

   GList *rmqConnList;         /* list of connections from RabbitMQ client.
                                * we do not need a list for vmx connection as
                                * each vmx connection is attached to a RabbitMQ
                                * client connection. */

   ToolsAppCtx *ctx;           /* tools context */
   gboolean messageTunnellingEnabled;    /* Status of Message bus Tunnelling */

   int maxSendQueueLen;
} GuestProxyData;

static GuestProxyData proxyData;

static void
StopRecvFromConn(ConnInfo *conn);   // IN
static void
CloseConn(ConnInfo *conn);  // IN


/*
 *-----------------------------------------------------------------------------
 *
 * GetConfigInt --
 *
 *      Get an integer number from tools config.
 *
 * Result:
 *      Return the configured integer value.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
GetConfigInt(const char *name,      // IN
             int defaultVal)        // IN
{
   GError *gerr = NULL;
   int num = g_key_file_get_integer(proxyData.ctx->config,
                                    CONFGROUP_GRABBITMQ_PROXY,
                                    name,
                                    &gerr);
   if (gerr) {
      g_clear_error(&gerr);
      return defaultVal;
   }

   return num;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetConfigBool --
 *
 *      Get a boolean from tools config.
 *
 * Result:
 *      Return the configured boolean value.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
GetConfigBool(const char *name,           // IN
              gboolean defaultVal)        // IN
{
   GError *gerr = NULL;
   gboolean val = g_key_file_get_boolean(proxyData.ctx->config,
                                         CONFGROUP_GRABBITMQ_PROXY,
                                         name,
                                         &gerr);
   if (gerr) {
      g_clear_error(&gerr);
      return defaultVal;
   }

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetConnName --
 *
 *      return a name for a connection
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GetConnName(ConnInfo *conn)            // IN
{
   return conn->isRmqClient ? "client" : "vmx";
}


/*
 *-----------------------------------------------------------------------------
 *
 * ShutDownConn --
 *
 *      Close connection immediately if its send buffer is empty, otherwise
 *      mark it is being shut down and stop receivng and wait for the send
 *      buffer to be cleared.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ShutDownConn(ConnInfo *conn)            // IN
{
   g_debug("Entering %s\n", __FUNCTION__);

   conn->toConn = NULL;
   if (conn->sendQueueLen > 0) {
      g_info("Shutting down %s connection %d.\n",
             GetConnName(conn), AsyncSocket_GetFd(conn->asock));
      conn->shutDown = TRUE;
      StopRecvFromConn(conn);
   } else {
      CloseConn(conn);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CloseConn --
 *
 *      Close a socket connection and its corresponding vmx or client
 *      connection. Remove the connection from client connection list if
 *      it is a client connection. The corresponding peer connection is not
 *      closead immediately if its send buffer is not empty, we will wait for
 *      the send buffer to be empty by marking a shutdown flag only.
 *
 *      Note: AsyncSocket does not differentiate read/write errors yet and
 *      does not try to send any data to the other end on close, so pending
 *      send data is dropped when a connection is closed even the socket may
 *      be still good for write. We might need to handle partial socket
 *      shutdown later.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
CloseConn(ConnInfo *conn)   // IN
{
   g_debug("Entering %s\n", __FUNCTION__);

   ASSERT(conn->asock != NULL);

   if (conn->toConn != NULL) {
      ShutDownConn(conn->toConn);
      conn->toConn = NULL;
   }
   g_info("Closing %s connection %d\n", GetConnName(conn),
          AsyncSocket_GetFd(conn->asock));

   AsyncSocket_Close(conn->asock);
   conn->asock = NULL;
   free(conn->recvBuf);
   conn->recvBuf = NULL;

   /* remove the connection from corresponding conn list */
   if (conn->isRmqClient) {
      proxyData.rmqConnList = g_list_remove(proxyData.rmqConnList, conn);
   }
   free(conn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * AssignVmxConn --
 *
 *      Assign vmx connection to a RabbitMq client connection.
 *
 * Result:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *-----------------------------------------------------------------------------
 */

static Bool
AssignVmxConn(ConnInfo *conn)    // IN
{
   GList *lp;

   for(lp = proxyData.rmqConnList; lp; lp = g_list_next(lp)) {
      ConnInfo *cli = (ConnInfo *)(lp->data);
      if (cli->toConn == NULL) {
         cli->toConn = conn;
         conn->toConn = cli;
         return TRUE;
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartRecvFromRmqClient --
 *
 *      Register recv callback for RabbitMQ client connection.
 *
 * Result:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
StartRecvFromRmqClient(ConnInfo *conn)   // IN
{
   int res;

   ASSERT(AsyncSocket_GetState(conn->asock) == AsyncSocketConnected);

   if (conn->recvBuf == NULL) {
      conn->recvBufLen = RMQ_CLIENT_CONN_RECV_BUFF_SIZE;
      conn->recvBuf = malloc(conn->recvBufLen);
      if (conn->recvBuf == NULL) {
         g_info("Error in allocating recv buffer for socket %d, "
                "closing connection.\n",
                AsyncSocket_GetFd(conn->asock));
         CloseConn(conn);
         return FALSE;
      }
   }

   res = AsyncSocket_RecvPartial(conn->asock, conn->recvBuf,
                                 conn->recvBufLen,
                                 conn->recvCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in AsyncSocket_RecvPartial for socket %d: %s\n",
             AsyncSocket_GetFd(conn->asock), AsyncSocket_Err2String(res));
      CloseConn(conn);
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StartRecvFromVmx --
 *
 *      Register recv callback for VMX connection.
 *
 * Result:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
StartRecvFromVmx(ConnInfo *conn)   // IN
{
   int res;
   res = AsyncSocket_Recv(conn->asock, &conn->packetLen,
                          sizeof conn->packetLen,
                          conn->recvCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in AsyncSocket_Recv for socket %d: %s\n",
             AsyncSocket_GetFd(conn->asock), AsyncSocket_Err2String(res));
      CloseConn(conn);
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RecvPacketFromVmxConn --
 *
 *      Wrapper function for recving a dataMap packet from VMX connection.
 *
 * Result:
 *      TURE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
RecvPacketFromVmxConn(ConnInfo *conn, int len)
{
   int res;

   res = AsyncSocket_Recv(conn->asock, conn->recvBuf + sizeof conn->packetLen,
         len, conn->recvCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in AsyncSocket_Recv for socket %d: %s\n",
             AsyncSocket_GetFd(conn->asock), AsyncSocket_Err2String(res));
      CloseConn(conn);
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConnSendDoneCb --
 *
 *      Callback function when some data is sent over a connection.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ConnSendDoneCb(void *buf,            // IN
               int len,              // IN
               AsyncSocket *asock,   // IN
               void *clientData)     // IN
{
   ConnInfo *dst = (ConnInfo *)clientData;
   ConnInfo *src = dst->toConn;

   g_debug("Entering %s\n", __FUNCTION__);

   free(buf);

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      /* this callback may be called after the connection is closed to
       * empty the send buffer */
      return;
   }

   dst->sendQueueLen -= len;
   ASSERT(dst->sendQueueLen >= 0);

   if (dst->sendQueueLen == 0 && dst->shutDown) {
      g_info("Closing %s connection %d as sendbuffer is now empty.\n",
             GetConnName(dst), AsyncSocket_GetFd(dst->asock));
      CloseConn(dst);
      return;
   }

   g_debug("%d bytes sent to %s connection %d, sendQueueLen = %d\n",
           len, GetConnName(dst), AsyncSocket_GetFd(dst->asock),
           dst->sendQueueLen);

   if ((!(dst->shutDown)) && src->recvStopped &&
       (dst->sendQueueLen < proxyData.maxSendQueueLen)) {
      g_debug("Restart reading from connection %d.\n",
              AsyncSocket_GetFd(src->asock));

      src->recvStopped = FALSE;
      if (src->isRmqClient) {
         StartRecvFromRmqClient(src);
      } else {
         StartRecvFromVmx(src);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * StopRecvFromConn --
 *
 *      Temporarily stop recving from a given connection.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
StopRecvFromConn(ConnInfo *conn)   // IN
{
   int res;
   g_debug("Temporarily stop reading from socket %d.\n",
            AsyncSocket_GetFd(conn->asock));
   res = AsyncSocket_CancelRecvEx(conn->asock, NULL, NULL, NULL, TRUE);
   ASSERT(res == ASOCKERR_SUCCESS);
   conn->recvStopped = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendToConn --
 *
 *      Call AsyncSocket_Send to queue the buffer for send.
 *      - If there is too much data queued, then recv from
 *        source connection is temporarily stopped.
 *
 * Result:
 *      TRUE on success, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
SendToConn(ConnInfo *dst,          // IN/OUT
           char *buf,              // IN
           int len)                // IN
{
   ConnInfo *src = dst->toConn;
   int res;

   g_debug("Entering %s\n", __FUNCTION__);

   res = AsyncSocket_Send(dst->asock, buf, len, dst->sendCb, dst);

   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in AsyncSocket_Send for socket %d, "
             "closing connection: %s\n",
             AsyncSocket_GetFd(dst->asock), AsyncSocket_Err2String(res));
      free(buf);     /* need to free here */
      CloseConn(dst);
      return FALSE;
   }

   g_debug("Sending %d bytes to socket %d\n", len,
           AsyncSocket_GetFd(dst->asock));

   dst->sendQueueLen += len;
   g_debug("Socket %d sendQueueLen = %d\n",
           AsyncSocket_GetFd(dst->asock), dst->sendQueueLen);

   if ((!src->recvStopped) && (dst->sendQueueLen > proxyData.maxSendQueueLen)) {
      StopRecvFromConn(src);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendVmxConnectRequest --
 *
 *      Notify VMX the listening port via RPC command so VMX can connect to
 *      the guest proxy.
 *      - 'sock': listening socket struct
 *
 * Result:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
SendVmxConnectRequest(void)
{
   AsyncSocket *asock = proxyData.vmxListenSock;
   gboolean ok;
   struct sockaddr_vm addr;
   socklen_t len = sizeof addr;
   int port;
   gchar *msg;

   g_debug("Entering %s\n", __FUNCTION__);

   ASSERT(asock != NULL);

   /* get the listening port */
   if (getsockname(AsyncSocket_GetFd(asock),
                   (struct sockaddr *)&addr, &len) == SOCKET_ERROR) {
      g_warning("Error in socket getsockname: error=%d.\n", sockerr());
      return FALSE;
   }

   port = addr.svm_port;

   msg = g_strdup_printf("xrabbitmqProxy.connect %d", port);
   ok = RpcChannel_Send(proxyData.ctx->rpc, msg, strlen(msg), NULL, NULL);
   if (!ok) {
      g_warning("Failed to send connect request to VMX RabbitMQ Proxy.\n");
   }
   g_free(msg);

   return ok;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SendToVmxRmqProxy --
 *
 *      Package RabbitMQ Client data and send it to VMX RabbitMQ Proxy.
 *
 * Result:
 *      TRUE on sucess, FALSE on error
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
SendToVmxRmqProxy(ConnInfo *cli,     // IN
                  char *buf,         // IN
                  int len)           // IN
{
   DataMap map;
   ErrorCode res;
   char *newBuf;
   char *serBuf;
   int bufLen;
   gboolean mapCreated = FALSE;
   char *ver;

   res = DataMap_Create(&map);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   mapCreated = TRUE;

   res = DataMap_SetInt64(&map, RMQPROXYDM_FLD_COMMAND,
                          COMMAND_DATA, TRUE);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   ver = strdup(GUEST_RABBITMQ_PROXY_VERSION);
   if (ver == NULL) {
      res = DMERR_INSUFFICIENT_MEM;
      goto quit;
   }
   res = DataMap_SetString(&map, RMQPROXYDM_FLD_GUEST_VER_ID, ver, -1, TRUE);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   newBuf = malloc(len);
   if (newBuf == NULL) {
      g_warning("Error in allocating memory.\n");
      goto quit;
   }
   memcpy(newBuf, buf, len);
   res = DataMap_SetString(&map, RMQPROXYDM_FLD_PAYLOAD, newBuf,
                           len, TRUE);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   res = DataMap_Serialize(&map, &serBuf, &bufLen);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   DataMap_Destroy(&map);
   return SendToConn(cli->toConn, serBuf, bufLen);

quit:
   if (mapCreated) {
      DataMap_Destroy(&map);
   }
   g_info("Error in dataMap encoding for socket %d, error=%d, "
          "closing connection.\n",
          AsyncSocket_GetFd(cli->asock), res);
   CloseConn(cli);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RmqClientConnRecvedCb --
 *
 *      Callback function when some data is recved from RabbitMQ client.
 *
 * Result:
 *      TRUE to continue polling, FALSE to discontinue polling.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RmqClientConnRecvedCb(void *buf,            // IN
                      int len,              // IN
                      AsyncSocket *asock,   // IN
                      void *clientData)     // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;

   g_debug("Entering %s\n", __FUNCTION__);

   g_debug("Recved %d bytes from client connection %d\n", len,
           AsyncSocket_GetFd(conn->asock));
   ASSERT(buf == conn->recvBuf);
   if (SendToVmxRmqProxy(conn, conn->recvBuf, len)) {
      StartRecvFromRmqClient(conn);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ProcessVmxDataPacket --
 *
 *      Process the dataMap packet received from VMX.
 *
 * Result:
 *      TRUE on success, FALSE on error.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ProcessVmxDataPacket(ConnInfo *cli,     // IN
                     DataMap *map)      // IN
{
   ErrorCode res;
   int64 cmdType;

   res = DataMap_GetInt64(map, RMQPROXYDM_FLD_COMMAND, &cmdType);
   ASSERT(res == DMERR_SUCCESS);

   switch (cmdType) {
      case COMMAND_DATA:
         {
            char *buf;
            int payloadLen;
            char *payload;

            res = DataMap_GetString(map, RMQPROXYDM_FLD_PAYLOAD,
                                    &payload, &payloadLen);
            ASSERT(res == DMERR_SUCCESS && payloadLen > 0);
            buf = malloc(payloadLen); /* get rid of this ? */

            if (buf) {
               memcpy(buf, payload, payloadLen);
               return SendToConn(cli, buf, payloadLen);
            } else {
               g_warning("Could not allocate buffer for socket %d, "
                         "closing connection.\n",
                         AsyncSocket_GetFd(cli->asock));
               CloseConn(cli);
               return FALSE;
            }
            break;
         }
      case COMMAND_CLOSE:
         {
            g_debug("Closing connection %d as instructed.\n",
                    AsyncSocket_GetFd(cli->asock));
            CloseConn(cli);
            return FALSE;
         }
      case COMMAND_CONNECT:
         break;
      default:
         g_debug("Unknown dataMap packet type from connection %d!\n",
                 AsyncSocket_GetFd(cli->asock));
         CloseConn(cli);
         return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ProcessPacketHeaderLen --
 *
 *      Helper function to handle once a dataMap packet length is known.
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
ProcessPacketHeaderLen(ConnInfo *conn,   // IN
                       int len)          // IN
{
   int pktLen = ntohl(conn->packetLen);

   g_debug("Entering %s\n", __FUNCTION__);

   if (conn->recvBuf == NULL || conn->recvBufLen < pktLen + len) {
      conn->recvBufLen = pktLen + len;
      free(conn->recvBuf);
      conn->recvBuf = malloc(pktLen + len);
      if (conn->recvBuf == NULL) {
         g_info("Could not allocate recv buffer for socket %d, "
                "closing connection.\n", AsyncSocket_GetFd(conn->asock));
         CloseConn(conn);
         return;
      }
   }

   *((int32 *)(conn->recvBuf)) = conn->packetLen;
   RecvPacketFromVmxConn(conn, pktLen);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxConnRecvedCb --
 *
 *      Callback function when data from VMX vsocket connection is recved.
 *
 * Result:
 *      TRUE to continue polling, FALSE to discontinue polling.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VmxConnRecvedCb(void *buf,            // IN
                int len,              // IN
                AsyncSocket *asock,   // IN
                void *clientData)     // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;

   g_debug("Entering %s\n", __FUNCTION__);

   if (buf == &conn->packetLen) {
      ASSERT(len == sizeof conn->packetLen);
      ProcessPacketHeaderLen(conn, len);
   } else {
      DataMap map;
      ErrorCode res;
      int packetLen = len + sizeof conn->packetLen;

      /* decoding the packet */
      res = DataMap_Deserialize(conn->recvBuf, packetLen, &map);
      ASSERT(res == DMERR_SUCCESS);

      if (ProcessVmxDataPacket(conn->toConn, &map)) {
         StartRecvFromVmx(conn); /* continue to recv next packet */
      }

      DataMap_Destroy(&map);
   }

}


/*
 *-----------------------------------------------------------------------------
 *
 * ConnErrorHandlerCb --
 *
 *      Connection error handler for asyncsocket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ConnErrorHandlerCb(int err,             // IN
                   AsyncSocket *asock,  // IN
                   void *clientData)    // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;
   g_debug("Entering %s\n", __FUNCTION__);

   ASSERT(conn->asock != NULL);
   g_info("Error code %d, on %s connection %d\n", err, GetConnName(conn),
          AsyncSocket_GetFd(conn->asock));

   CloseConn(conn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmxListenSockConnectedCb --
 *
 *      Poll callback function for a new VMX connection.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *-----------------------------------------------------------------------------
 */

void
VmxListenSockConnectedCb(AsyncSocket *asock,    // IN
                         void *clientData)      // IN
{
   int res;
   int fd = AsyncSocket_GetFd(asock);
   ConnInfo *conn = NULL;
   int sendBufSize = GetConfigInt("vmxSendBufferSize",
                                  DEFAULT_VMX_CONN_SEND_BUFF_SIZE);
   int recvBufSize = GetConfigInt("vmxRecvBufferSize",
                                  DEFAULT_VMX_CONN_RECV_BUFF_SIZE);

   g_debug("Entering %s\n", __FUNCTION__);

   g_info("Got vmx connection, socket=%d\n", fd);

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      g_info("Socket %d is not connected, closing\n", fd);
      goto exit;
   }

   if (!AsyncSocket_SetBufferSizes(asock, sendBufSize, recvBufSize)) {
      g_info("Cannot set VSOCK buffer sizes, closing socket %d\n", fd);
      goto exit;
   }

   conn = calloc(1, sizeof *conn);
   if (conn == NULL) {
      g_warning("Could not allocate memory, closing socket %d\n", fd);
      goto exit;
   }

   conn->asock = asock;
   conn->recvCb = VmxConnRecvedCb;
   conn->sendCb = ConnSendDoneCb;
   conn->errorCb = ConnErrorHandlerCb;

   res = AsyncSocket_SetErrorFn(asock, conn->errorCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in set error handler for socket %d\n", fd);
      goto exit;
   }

   if (!AssignVmxConn(conn)) {
      g_warning("Could not find RabbitMQ client connection for vmx connection, "
                "closing connection ...\n");
      goto exit;
   }

   if (StartRecvFromVmx(conn)) {
      StartRecvFromRmqClient(conn->toConn);
   }
   return;

exit:
   AsyncSocket_Close(asock);
   free(conn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RmqClientSockHandShakeCb --
 *
 *      Callback function when a RabbitMQ client connection completes the
 *      SSL hand shake.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RmqClientSockHandShakeCb(Bool status,           // IN
                         AsyncSocket *asock,    // IN
                         void *clientData)      // IN
{
   ConnInfo *conn;
   int fd, res;

   g_debug("Entering %s\n", __FUNCTION__);

   if (!status) {
      g_warning("Failed SSL hand shake in socket %d, closing connection.\n",
                AsyncSocket_GetFd(asock));
      goto exit;
   }

   fd = AsyncSocket_GetFd(asock);

   g_info("Established new RabbitMQ client connection %d.\n", fd);

   if (!SendVmxConnectRequest()) {
      g_warning("Closing RabbitMQ client connection %d due to error in "
                "sending connection request!\n", fd);
      goto exit;
   }

   /* add to the client connection list */
   conn = calloc(1, sizeof *conn);
   if (conn == NULL) {
      g_warning("Could not allocate memory, closing socket %d\n", fd);
      goto exit;
   }

   conn->isRmqClient = TRUE;
   conn->asock = asock;
   conn->recvCb = RmqClientConnRecvedCb;
   conn->sendCb = ConnSendDoneCb;
   conn->errorCb = ConnErrorHandlerCb;

   res = AsyncSocket_SetErrorFn(asock, conn->errorCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      g_info("Error in set error handler for socket %d\n", fd);
      free(conn);
      goto exit;
   }

   proxyData.rmqConnList = g_list_append(proxyData.rmqConnList, conn);

   /* we start recv only after the vmx connection is established,
      so we do not need to buffer no-destination content */
   return;

exit:
   AsyncSocket_Close(asock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetProxyDataDir --
 *
 *      Find out the guest proxy data directory.
 *
 * Result:
 *      Return the path of the guest proxy data directory.
 *      NULL if there is an error.
 *      Caller should treat the returned string as static, and not free it.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
static const char *
GetProxyDataDir(void)
{
   static char *proxyDataDir = NULL;
   char *confPath = NULL;

   if (proxyDataDir) {
      goto done;
   }

   confPath = GuestApp_GetConfPath();
   if (confPath) {
      proxyDataDir = g_strdup_printf("%s%sGuestProxyData", confPath, DIRSEPS);
   }

done:
   free(confPath);

   return proxyDataDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetSslCertFile --
 *
 *      Find out the SSL certicate file path
 *
 * Result:
 *      The SSL certificate file path
 *      NULL if there is an error.
 *      Caller should treat the returned string as static, and not free it.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GetSslCertFile(void)
{
   static char *certFile = NULL;
   const char *dataDir;

   if (certFile) {
      goto done;
   }

   dataDir = GetProxyDataDir();
   if (dataDir) {
      certFile = g_strdup_printf("%s%sserver%scert.pem", dataDir, DIRSEPS,
                                 DIRSEPS);
   }

done:
   return certFile;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetSslKeyFile --
 *
 *      Find out the SSL key file path
 *
 * Result:
 *      The SSL key file path
 *      NULL if there is an error.
 *      Caller should treat the returned string as static, and not free it.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GetSslKeyFile(void)
{
   static char *keyFile = NULL;
   const char *dataDir;

   if (keyFile) {
      goto done;
   }

   dataDir = GetProxyDataDir();
   if (dataDir) {
      keyFile = g_strdup_printf("%s%sserver%skey.pem", dataDir, DIRSEPS,
                                DIRSEPS);
   }

done:
   return keyFile;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetSslTrustDir --
 *
 *      Find out the SSL trusted client certificate directory path.
 *
 * Result:
 *      The SSL trusted client certificate directory path.
 *      NULL if there is an error.
 *      Caller should treat the returned string as static, and not free it.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GetSslTrustDir(void)
{
   static char *trustDir = NULL;
   const char *dataDir;

   if (trustDir) {
      goto done;
   }

   dataDir = GetProxyDataDir();
   if (dataDir) {
      trustDir = g_strdup_printf("%s%strusted", dataDir, DIRSEPS);
   }

done:
   return trustDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * TheSslContext --
 *
 *      Create and cache a global SSL_CTX object
 *
 * Result:
 *      The SSL_CTX object created.
 *      NULL if there is an error.
 *      Caller should not free the SSL context returned.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static SSL_CTX *
TheSslContext(void)
{
   static SSL_CTX *sslCtx = NULL;
   SSL_CTX *workingCtx = NULL;
   const char *certFile;
   const char *keyFile;
   const char *trustDir;
   long sslCtxOptions;

   if (sslCtx) {
      goto done;
   }

   workingCtx = SSL_NewContext();
   if (!workingCtx) {
      g_warning("Cannot create the SSL context.\n");
      goto done;
   }

   /*
    * The bora/lib/ssl code not yet ready to disable TLS1, and TLS1_1,
    * we shall remove the code below once it is able to.
    */
   sslCtxOptions = SSL_CTX_get_options(workingCtx);
   /* Allow only TLSv1_2 */

#ifdef SSL_OP_NO_TLSv1
   sslCtxOptions |= SSL_OP_NO_TLSv1;
#endif

#ifdef SSL_OP_NO_TLSv1_1
   sslCtxOptions |= SSL_OP_NO_TLSv1_1;
#endif

   SSL_CTX_set_options(workingCtx, sslCtxOptions);

   certFile = GetSslCertFile();
   if (!certFile) {
      g_warning("Cannot find the certificate file\n");
      goto done;
   }
   if (!SSL_CTX_use_certificate_file(workingCtx, certFile,
                                     SSL_FILETYPE_PEM)) {
      g_warning("Cannot load the certificate file: %s\n", certFile);
      goto done;
   }

   keyFile = GetSslKeyFile();
   if (!keyFile) {
      g_warning("Cannot find the key file\n");
      goto done;
   }
   if (!SSL_CTX_use_PrivateKey_file(workingCtx, keyFile,
                                    SSL_FILETYPE_PEM)) {
      g_warning("Cannot load the key file: %s\n", keyFile);
      goto done;
   }

   trustDir = GetSslTrustDir();
   if (!trustDir) {
      g_warning("Cannot find the trusted client certificate directory\n");
      goto done;
   }
   if (!SSL_CTX_load_verify_locations(workingCtx, NULL, trustDir)) {
      g_warning("Cannot load the trusted cert directory: %s\n", trustDir);
      goto done;
   }

   SSL_CTX_set_verify(workingCtx, SSL_VERIFY_PEER |
                      SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE,
                      NULL);

   sslCtx = workingCtx;
   workingCtx = NULL;

done:
   if (workingCtx) {
      SSL_CTX_free(workingCtx);
   }

   return sslCtx;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RmqListenSockConnectedCb --
 *
 *      Poll callback function when a new RabbitMQ client connection.
 *      listen socket.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
RmqListenSockConnectedCb(AsyncSocket *asock,    // IN
                         void *clientData)      // IN
{
   int fd = AsyncSocket_GetFd(asock);
   int sendBufSize = GetConfigInt("rmqClientSendBuffer",
                                  DEFAULT_RMQCLIENT_CONN_SEND_BUFF_SIZE);
   int recvBufSize = GetConfigInt("rmqClientRecvBuffer",
                                  DEFAULT_RMQCLIENT_CONN_RECV_BUFF_SIZE);


   g_debug("Entering %s\n", __FUNCTION__);
   g_info("Got new RabbitMQ client connection %d.\n", fd);

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      g_info("Socket %d is not connected, closing.\n", fd);
      goto exit;
   }

   if (!AsyncSocket_SetBufferSizes(asock, sendBufSize, recvBufSize)) {
      g_info("Closing socket %d due to error.\n", fd);
      goto exit;
   }

   if (GetConfigBool("ssl", TRUE)) {
      SSL_CTX *sslCtx = TheSslContext();
      if (!sslCtx) {
         g_warning("Closing socket %d due to the invalid ssl context.\n", fd);
         goto exit;
      }

      AsyncSocket_StartSslAccept(asock, sslCtx, RmqClientSockHandShakeCb, NULL);
   } else {
      RmqClientSockHandShakeCb(TRUE, asock, NULL);
   }

   return;

exit:
   AsyncSocket_Close(asock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateVmxListenSocket --
 *
 *      Create listening socket for VMX vsocket connection. The auto assigned
 *      port will be sent to VMX via RPC.
 *
 * Result:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
CreateVmxListenSocket(void)
{
   int res = ASOCKERR_SUCCESS;
   AsyncSocket *asock;

   g_debug("Entering %s\n", __FUNCTION__);

   asock = AsyncSocket_ListenVMCI(VMCISock_GetLocalCID(), VMADDR_PORT_ANY,
                                  VmxListenSockConnectedCb,
                                  NULL, NULL, &res);

   if (asock == NULL || res != ASOCKERR_SUCCESS) {
      g_info("Could not create listening socket for VMX proxy connection: %s\n",
             AsyncSocket_Err2String(res));
      if (asock != NULL) {
         AsyncSocket_Close(asock);
      }
      return FALSE;
   }

   proxyData.vmxListenSock = asock;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateRmqListenSocket --
 *
 *      Create listening socket for RabbitMQ clients.
 *
 * Result:
 *      TRUE on success, FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
CreateRmqListenSocket(void)
{
   int defaultPort = 6672;  /* default RabbitMQ Proxy listening port */
   int port;
   int res = ASOCKERR_SUCCESS;
   AsyncSocket *asock;

   g_debug("Entering %s\n", __FUNCTION__);

   port = GetConfigInt("port", defaultPort);

   if (GetConfigBool("enableNetworkConnections", FALSE)) {
      asock = AsyncSocket_Listen(NULL, port, RmqListenSockConnectedCb,
                                 NULL, NULL, &res);
   } else {
      asock = AsyncSocket_ListenLoopback(port, RmqListenSockConnectedCb,
                                         NULL, NULL, &res);
   }

   if (asock == NULL || res != ASOCKERR_SUCCESS) {
      g_info("Error in creating listening socket for RabbitMQ client: %s\n",
             AsyncSocket_Err2String(res));
      if (asock != NULL) {
         AsyncSocket_Close(asock);
      }
      return FALSE;
   }

   proxyData.rmqListenSock = asock;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * InitProxyData --
 *
 *      Init proxyData structure.
 *      - 'ctx': ToolsAppCtx
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
InitProxyData(ToolsAppCtx *ctx)   // IN
{
   /* init proxyData */
   memset(&proxyData, 0, sizeof proxyData);

   proxyData.ctx = ctx;
   proxyData.messageTunnellingEnabled = FALSE;
   proxyData.maxSendQueueLen = GetConfigInt("maxSendQueueLen",
                                            DEFAULT_MAX_SEND_QUEUE_LEN);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetVmVcUuidFromVmx --
 *
 *      Send a GuestRpc command to VMX to retrieve the VM's VC uuid.
 *
 * Result:
 *      The VM's vc uuid if successful.
 *      NULL otherwise.
 *      Caller needs to free the returned result string.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
GetVmVcUuidFromVmx(void)
{
   char *vcUuid;
   char *reply;
   size_t replyLen;
   gboolean ok;
   gchar *msg = "xrabbitmqProxy.getVmVcUuid";

   /* check VMX to see if proxy is enabled or not */
   ok = RpcChannel_Send(proxyData.ctx->rpc, msg, strlen(msg),
                        &reply, &replyLen);
   if (!ok) {
      g_warning("Guest rpc call to VMX failed, "
                "cannot retrieve vc uuid from vmx.\n");
      return NULL;
   }

   if (replyLen > VC_UUID_SIZE) {
      g_warning("Guest rpc call to VMX failed, "
                "the returned vc uuid too large.\n");
      return NULL;
   }

   vcUuid = Util_SafeMalloc(replyLen + 1);
   Str_Strcpy(vcUuid, reply, replyLen + 1);

   g_info("Guest rpc call to VMX, retrieved vc uuid %s\n",
          vcUuid);

   return vcUuid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetVmVcUuidDir --
 *
 *      Return the directory to publish the VM's vc uuid
 *
 * Result:
 *      A string represents where to publish the VM's vc uuid
 *      NULL otherwise.
 *      Caller should not free the returned string
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GetVmVcUuidDir(void) // IN
{
   static char *vmVcUuidDir = NULL;
   const char *proxyDataDir;

   if (vmVcUuidDir) {
      goto done;
   }

   proxyDataDir = GetProxyDataDir();
   if (proxyDataDir) {
      vmVcUuidDir = g_strdup_printf("%s%sVmVcUuid", proxyDataDir, DIRSEPS);
   }

done:
   return vmVcUuidDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PublishVmVcUuid --
 *
 *      Publish the vc uuid in the guest
 *
 * Result:
 *      TRUE if successful
 *      FALSE otherwise
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
PublishVmVcUuid(const char *vcUuid) // IN
{
   gboolean status = FALSE;
   GError *err = NULL;
   char *filename = NULL;
   const char *dir = GetVmVcUuidDir();

   if (!dir) {
      g_warning("Cannot find out the VM VC UUID path\n");
      goto done;
   }

   if (g_mkdir_with_parents(dir, 0755) < 0) {
      g_warning("Cannot create directory %s\n", dir);
      goto done;
   }

   filename = g_strdup_printf("%s%svm.vc.uuid", dir, DIRSEPS);
   g_assert(filename);

   g_file_set_contents(filename, vcUuid, -1, &err);
   if (err != NULL)  {
      g_warning("Cannot write the vc uuid to file %s, %s\n", filename,
                err->message);
      goto done;
   }

   status = TRUE;

done:

   if (err) {
      g_error_free(err);
   }
   g_free(filename);

   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * GRabbitmqProxyDisableMessageTunnelling --
 *
 *     Clean up sockets and connections.
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
GRabbitmqProxyDisableMessageTunnelling()
{
   g_debug("Entering %s\n", __FUNCTION__);

   if (proxyData.vmxListenSock) {
      AsyncSocket_Close(proxyData.vmxListenSock);
      proxyData.vmxListenSock = NULL;
   }

   if (proxyData.rmqListenSock) {
      AsyncSocket_Close(proxyData.rmqListenSock);
      proxyData.rmqListenSock = NULL;
   }

   while (proxyData.rmqConnList != NULL) {
      ConnInfo *cli = (ConnInfo *)(proxyData.rmqConnList->data);
      CloseConn(cli);
   }

   proxyData.messageTunnellingEnabled = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GRabbitmqProxyEnableMessageTunnelling --
 *
 *     Creates the sockets and starts listening.
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
GRabbitmqProxyEnableMessageTunnelling()
{
   char * vcUuid;

   g_debug("Entering %s\n", __FUNCTION__);

   if (proxyData.messageTunnellingEnabled) {
      return;
   }

   vcUuid = GetVmVcUuidFromVmx();

   if (!vcUuid) {
      g_warning("Failed to get vc uuid, disable the rabbitmq proxy");
      return;
   }

   if (!PublishVmVcUuid(vcUuid))
   {
      g_warning("Failed to publish vc uuid, disable the rabbitmq proxy");
      free(vcUuid);
      return;
   }

   free(vcUuid);

   if (!CreateVmxListenSocket() || !CreateRmqListenSocket()) {
      g_warning("The proxy is disabled due to initialization error.\n");
      GRabbitmqProxyDisableMessageTunnelling();
      return;
   }

   proxyData.messageTunnellingEnabled = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GRabbitmqProxyShutdown --
 *
 *      Clean up internal state on shutdown.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
GRabbitmqProxyShutdown(gpointer src,                 // IN
                       ToolsAppCtx *ctx,             // IN
                       ToolsPluginData *plugin)      // IN
{
   g_debug("Entering %s\n", __FUNCTION__);

   if (proxyData.messageTunnellingEnabled) {
      GRabbitmqProxyDisableMessageTunnelling();
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * GRabbitmqProxySetOption --
 *
 *      Handles a "Set_Option" callback. Handles
 *      TOOLSOPTION_ENABLE_MESSAGE_BUS_TUNNEL by enabling or disabling
 *      the message bus tunnel.
 *
 * Result:
 *      TRUE on success.
 *
 * Side-effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

static gboolean
GRabbitmqProxySetOption(gpointer src,               // IN
                        ToolsAppCtx *ctx,           // IN
                        const gchar *option,        // IN
                        const gchar *value,         // IN
                        ToolsPluginData *plugin)    // IN
{
   gboolean retVal = FALSE;

   if (strcmp(option, TOOLSOPTION_ENABLE_MESSAGE_BUS_TUNNEL) == 0) {
      if (strcmp(value, "1") == 0) {
         GRabbitmqProxyEnableMessageTunnelling();
      } else if (strcmp(value, "0") == 0 &&
                 proxyData.messageTunnellingEnabled) {
         GRabbitmqProxyDisableMessageTunnelling();
      }
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GRabbitmqProxyGetSSLLibPath --
 *
 *      This function returns the properly formatted string that points to
 *      a directory containing the library files.
 *
 * Results:
 *      Absolute path of the directory containing the library files.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
GRabbitmqProxyGetSSLLibPath(const char *arg1,     // IN/UNUSED
                            const char *arg2)     // IN/UNUSED
{
   gchar *sslLibPath = NULL;

#ifdef _WIN32
   sslLibPath = GuestApp_GetInstallPath();
#elif !defined(OPEN_VM_TOOLS)
   sslLibPath = VMTools_GetLibdir();
#endif

   if (NULL != sslLibPath) {
      gchar *endPath;
      g_debug("%s: SSL Library Directory is %s\n", __FUNCTION__, sslLibPath);
      endPath = sslLibPath + strlen(sslLibPath) - 1;
      if ((endPath > sslLibPath) && (DIRSEPC == *endPath)) {
         *endPath = 0;
      }
   }

   return sslLibPath;
} // GRabbitmqProxyGetSSLLibPath


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsOnLoad --
 *
 *      Returns the registration data for the Guest RabbitMQ Proxy.
 *
 * Result:
 *      Returns the registration data.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)           // IN
{
   static ToolsPluginData regData = { "grabbitmqProxy", NULL, NULL, NULL };

   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_SHUTDOWN, GRabbitmqProxyShutdown, &regData },
      { TOOLS_CORE_SIG_SET_OPTION, GRabbitmqProxySetOption, &regData }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_SIGNALS,
        VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   InitProxyData(ctx);
   Poll_InitGtk();
   SSL_Init(GRabbitmqProxyGetSSLLibPath, NULL, NULL);

   if (!TOOLS_IS_MAIN_SERVICE(ctx) && !TOOLS_IS_USER_SERVICE(ctx)) {
      g_info("Unknown container '%s', not loading grabbitmqProxyPlugin.",
             ctx->name);
      return NULL;
   }

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   g_info("The Guest RabbitMQ Proxy is up and running ...\n");
   return &regData;
}

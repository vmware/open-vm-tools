/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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

/*
 * gdpPlugin.c --
 *
 * Publishes guest data to host side gdp daemon.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <unistd.h>
#endif

#include "vm_assert.h"
#include "vm_atomic.h"
#include "vm_basic_types.h"
#define  G_LOG_DOMAIN  "gdp"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/gdp.h"
#include "vmci_defs.h"
#include "vmci_sockets.h"
#include "vmcheck.h"

#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);


#if defined(_WIN32)
#   define GetSockErr()  WSAGetLastError()

#   define SYSERR_EADDRINUSE     WSAEADDRINUSE
#   define SYSERR_EHOSTUNREACH   WSAEHOSTUNREACH
#   define SYSERR_EINTR          WSAEINTR
#   define SYSERR_EMSGSIZE       WSAEMSGSIZE
#   define SYSERR_WOULDBLOCK(e)  (e == WSAEWOULDBLOCK)

    typedef int socklen_t;
#   define CloseSocket closesocket

#   define SOCK_READ   FD_READ
#   define SOCK_WRITE  FD_WRITE
#else
#   define GetSockErr()  errno

#   define SYSERR_EADDRINUSE     EADDRINUSE
#   define SYSERR_EHOSTUNREACH   EHOSTUNREACH
#   define SYSERR_EINTR          EINTR
#   define SYSERR_EMSGSIZE       EMSGSIZE
#   define SYSERR_WOULDBLOCK(e)  (e == EAGAIN || e == EWOULDBLOCK)

    typedef int SOCKET;
#   define SOCKET_ERROR    (-1)
#   define INVALID_SOCKET  ((SOCKET) -1)
#   define CloseSocket     close

#   define SOCK_READ   POLLIN
#   define SOCK_WRITE  POLLOUT
#endif

#define PRIVILEGED_PORT_MAX  1023
#define PRIVILEGED_PORT_MIN  1

#define GDPD_LISTEN_PORT  7777

#define GDP_SEND_TIMEOUT  1000 // ms
#define GDP_RECV_TIMEOUT  3000 // ms


/*
 * GdpError message table.
 */
#define GDP_ERR_ITEM(a, b) b,
static const char * const gdpErrMsgs[] = {
GDP_ERR_LIST
};
#undef GDP_ERR_ITEM


typedef struct PluginData {
   ToolsAppCtx *ctx;       /* The application context */
#if defined(_WIN32)
   Bool wsaStarted;        /* TRUE : WSAStartup succeeded, WSACleanup required
                              FALSE: otherwise */
   WSAEVENT eventSendRecv; /* The send-recv event object:
                              Event object to associate with
                              network send/recv ready event */
   WSAEVENT eventStop;     /* The stop event object:
                              Event object signalled to stop guest data
                              publishing for vmtoolsd shutdown */
#else
   int eventStop;          /* The stop event fd:
                              Event fd signalled to stop guest data
                              publishing for vmtoolsd shutdown */
#endif
   int vmciFd;             /* vSocket address family value fd */
   int vmciFamily;         /* vSocket address family value */
   SOCKET sock;            /* Datagram socket for publishing guest data */
   Atomic_Bool stopped;    /* TRUE : Guest data publishing is stopped
                                     for vmtoolsd shutdown
                              FALSE: otherwise */
} PluginData;

static PluginData pluginData;


static Bool GdpInit(ToolsAppCtx *ctx);
static void GdpDestroy(void);
static void GdpSetStopEvent(void);
static Bool GdpCreateSocket(void);
static void GdpCloseSocket(void);
static GdpError GdpEmptyRecvQueue(void);
static GdpError GdpWaitForEvent(int netEvent, int timeout);
static GdpError GdpSend(const char *buf, int len, int timeout);
static GdpError GdpRecv(char *buf, int *len, int timeout);


/*
 ******************************************************************************
 * GdpInit --
 *
 * Initializes internal plugin data.
 *
 * @param[in]  ctx   The application context
 *
 * @return TRUE on success.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpInit(ToolsAppCtx *ctx) // IN
{
   Bool retVal = FALSE;

   pluginData.ctx = ctx;
#if defined(_WIN32)
   pluginData.wsaStarted = FALSE;
   pluginData.eventSendRecv = WSA_INVALID_EVENT;
   pluginData.eventStop = WSA_INVALID_EVENT;
#else
   pluginData.eventStop = -1;
#endif
   pluginData.vmciFd = -1;
   pluginData.vmciFamily = -1;
   pluginData.sock = INVALID_SOCKET;
   Atomic_WriteBool(&pluginData.stopped, FALSE);

#if defined(_WIN32)
   {
      WSADATA wsaData;
      int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
      if (res != 0) {
         g_critical("%s: WSAStartup failed: error=%d.\n",
                     __FUNCTION__, res);
         return FALSE;
      }

      pluginData.wsaStarted = TRUE;
   }

   pluginData.eventSendRecv = WSACreateEvent();
   if (pluginData.eventSendRecv == WSA_INVALID_EVENT) {
      g_critical("%s: WSACreateEvent for send/recv failed: error=%d.\n",
                 __FUNCTION__, WSAGetLastError());
      goto exit;
   }

   pluginData.eventStop = WSACreateEvent();
   if (pluginData.eventStop == WSA_INVALID_EVENT) {
      g_critical("%s: WSACreateEvent for stop failed: error=%d.\n",
                 __FUNCTION__, WSAGetLastError());
      goto exit;
   }
#else
   pluginData.eventStop = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
   if (pluginData.eventStop == -1) {
      g_critical("%s: eventfd for stop failed: error=%d.\n",
                 __FUNCTION__, errno);
      goto exit;
   }
#endif

   pluginData.vmciFamily = VMCISock_GetAFValueFd(&pluginData.vmciFd);
   if (pluginData.vmciFamily == -1) {
      g_critical("%s: Failed to get vSocket address family value.\n",
                 __FUNCTION__);
      goto exit;
   }

   retVal = TRUE;

exit:
   if (!retVal) {
      GdpDestroy();
   }

   return retVal;
}


/*
 ******************************************************************************
 * GdpDestroy --
 *
 * Destroys internal plugin data.
 *
 ******************************************************************************
 */

static void
GdpDestroy(void)
{
   GdpCloseSocket();

   if (pluginData.vmciFd != -1) {
      VMCISock_ReleaseAFValueFd(pluginData.vmciFd);
      pluginData.vmciFd = -1;
   }

#if defined(_WIN32)
   if (pluginData.eventStop != WSA_INVALID_EVENT) {
      WSACloseEvent(pluginData.eventStop);
      pluginData.eventStop = WSA_INVALID_EVENT;
   }

   if (pluginData.eventSendRecv != WSA_INVALID_EVENT) {
      WSACloseEvent(pluginData.eventSendRecv);
      pluginData.eventSendRecv = WSA_INVALID_EVENT;
   }

   if (pluginData.wsaStarted) {
      WSACleanup();
      pluginData.wsaStarted = FALSE;
   }
#else
   if (pluginData.eventStop != -1) {
      close(pluginData.eventStop);
      pluginData.eventStop = -1;
   }
#endif

   pluginData.ctx = NULL;
}


/*
 ******************************************************************************
 * GdpSetStopEvent --
 *
 * Signals the stop event object/fd.
 *
 ******************************************************************************
 */

static void
GdpSetStopEvent(void)
{
#if defined(_WIN32)
   ASSERT(pluginData.eventStop != WSA_INVALID_EVENT);
   WSASetEvent(pluginData.eventStop);
#else
   eventfd_t val = 1;
   ASSERT(pluginData.eventStop != -1);
   eventfd_write(pluginData.eventStop, val);
#endif
}


/*
 ******************************************************************************
 * GdpCreateSocket --
 *
 * Creates a non-blocking datagram socket for guest data publishing.
 *
 * The socket is bound to a local privileged port with default remote address
 * set to host side gdp daemon endpoint.
 *
 * @return TRUE on success.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpCreateSocket(void)
{
   struct sockaddr_vm localAddr;
   struct sockaddr_vm remoteAddr;
   int    sockErr;
   Bool   retVal = FALSE;
#if defined(_WIN32)
   u_long nbMode = 1; // Non-blocking mode
#  define SOCKET_TYPE_PARAM SOCK_DGRAM
#else
   /*
    * Requires Linux kernel version >= 2.6.27.
    */
#  define SOCKET_TYPE_PARAM SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC
#endif

   ASSERT(pluginData.sock == INVALID_SOCKET);

   pluginData.sock = socket(pluginData.vmciFamily, SOCKET_TYPE_PARAM, 0);
#undef SOCKET_TYPE_PARAM

   if (pluginData.sock == INVALID_SOCKET) {
      g_warning("%s: socket failed: error=%d.\n", __FUNCTION__, GetSockErr());
      return FALSE;
   }

#if defined(_WIN32)
   /*
    * Set socket to nonblocking mode.
    * Note: WSAEventSelect automatically does this if not done.
    */
   if (ioctlsocket(pluginData.sock, FIONBIO, &nbMode) != 0) {
      sockErr = GetSockErr();
      g_warning("%s: ioctlsocket failed: error=%d.\n", __FUNCTION__, sockErr);
      goto exit;
   }
#endif

   memset(&localAddr, 0, sizeof localAddr);
   localAddr.svm_family = pluginData.vmciFamily;
   localAddr.svm_cid = VMCISock_GetLocalCID();
   localAddr.svm_port = PRIVILEGED_PORT_MAX; // No htons

   do {
      if (bind(pluginData.sock, (struct sockaddr *)&localAddr,
               (socklen_t) sizeof localAddr) == 0) {
         sockErr = 0;
         break;
      }

      sockErr = GetSockErr();
      if (sockErr == SYSERR_EADDRINUSE) {
         g_info("%s: Local port %d is in use, retry the next one.\n",
                __FUNCTION__, localAddr.svm_port);
         localAddr.svm_port--;
      } else {
         g_warning("%s: bind failed: error=%d.\n", __FUNCTION__, sockErr);
         goto exit;
      }
   } while (localAddr.svm_port >= PRIVILEGED_PORT_MIN);

   if (sockErr != 0) {
      goto exit;
   }

   memset(&remoteAddr, 0, sizeof remoteAddr);
   remoteAddr.svm_family = pluginData.vmciFamily;
   remoteAddr.svm_cid = VMCI_HOST_CONTEXT_ID;
   remoteAddr.svm_port = GDPD_LISTEN_PORT; // No htons

   /*
    * Set default remote address to send to/recv from datagrams.
    */
   if (connect(pluginData.sock, (struct sockaddr *)&remoteAddr,
               (socklen_t) sizeof remoteAddr) != 0) {
      sockErr = GetSockErr();
      g_warning("%s: connect failed: error=%d.\n", __FUNCTION__, sockErr);
      goto exit;
   }

   g_debug("%s: Socket created and bound to local port %d.\n",
           __FUNCTION__, localAddr.svm_port);

   retVal = TRUE;

exit:
   if (!retVal) {
      CloseSocket(pluginData.sock);
      pluginData.sock = INVALID_SOCKET;
   }

   return retVal;
}


/*
 ******************************************************************************
 * GdpCloseSocket --
 *
 * Closes the guest data publishing socket.
 *
 ******************************************************************************
 */

static void
GdpCloseSocket(void)
{
   if (pluginData.sock != INVALID_SOCKET) {
      g_debug("%s: Closing socket.\n", __FUNCTION__);
      if (CloseSocket(pluginData.sock) != 0) {
         g_warning("%s: CloseSocket failed: fd=%d, error=%d.\n",
                   __FUNCTION__, pluginData.sock, GetSockErr());
      }

      pluginData.sock = INVALID_SOCKET;
   }
}


/*
 ******************************************************************************
 * GdpEmptyRecvQueue --
 *
 * Empties receive queue before publishing new data to host side gdp daemon.
 * This is required in case previous receive from daemon timed out and daemon
 * did reply later.
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpEmptyRecvQueue(void)
{
   GdpError retVal;

   ASSERT(pluginData.sock != INVALID_SOCKET);

   do {
      long res;
      int sockErr;
      char buf[1]; // OK to truncate, case of SYSERR_EMSGSIZE.

      /*
       * Windows: recv returns -1, first with SYSERR_EMSGSIZE,
       *          then SYSERR_WOULDBLOCK.
       * Linux  : recv returns 1 first, then -1 with SYSERR_WOULDBLOCK.
       */
      res = recv(pluginData.sock, buf, (int) sizeof buf, 0);
      if (res >= 0) {
         g_debug("%s: recv returns %d.\n", __FUNCTION__, (int)res);
         continue;
      }

      sockErr = GetSockErr();
      if (sockErr == SYSERR_EINTR) {
         continue;
      } else if (sockErr == SYSERR_EMSGSIZE) {
         g_debug("%s: recv truncated.\n", __FUNCTION__);
         continue;
      } else if (SYSERR_WOULDBLOCK(sockErr)) {
         retVal = GDP_ERROR_SUCCESS; // No more message in the recv queue.
      } else {
         /*
          * Note: recv does not return SYSERR_EHOSTUNREACH.
          */
         g_info("%s: recv failed: error=%d.\n", __FUNCTION__, sockErr);
         retVal = GDP_ERROR_INTERNAL;
      }

      break;

   } while (TRUE);

   return retVal;
}


/*
 ******************************************************************************
 * GdpWaitForEvent --
 *
 * Waits for the stop event object/fd signalled for vmtoolsd shutdown,
 * network send/receive event ready or timeout.
 *
 * @param[in]    netEvent GOS specific network send/receive event flag
 * @param[in]    timeout  Timeout value in milliseconds,
 *                        negative value means an infinite timeout,
 *                        zero means no wait
 *
 * @return GDP_ERROR_SUCCESS on specified network event ready.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpWaitForEvent(int netEvent, int timeout)
{
#if defined(_WIN32)
   int res;
   DWORD localTimeout;
   gint64 startTime;
   GdpError retVal;

   ASSERT(netEvent == FD_READ || netEvent == FD_WRITE);
   ASSERT(pluginData.sock != INVALID_SOCKET);

   /*
    * Reset the send-recv event object.
    */
   WSAResetEvent(pluginData.eventSendRecv);

   /*
    * Associate the send-recv event object with the specified network event
    * in the socket.
    */
   res = WSAEventSelect(pluginData.sock, pluginData.eventSendRecv, netEvent);
   if (res != 0) {
      g_info("%s: WSAEventSelect failed: error=%d.\n",
             __FUNCTION__, WSAGetLastError());
      return GDP_ERROR_INTERNAL;
   }

   localTimeout = (DWORD)(timeout >= 0 ? timeout : WSA_INFINITE);
   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deal with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      WSAEVENT eventObjects[] = {pluginData.eventStop,
                                 pluginData.eventSendRecv};
      DWORD waitRes;

      waitRes = WSAWaitForMultipleEvents((DWORD)ARRAYSIZE(eventObjects),
                                         eventObjects,
                                         FALSE, localTimeout, TRUE);
      if (waitRes == WSA_WAIT_EVENT_0) {
         /*
          * Main thread has set the stop event object to interrupt
          * pool thread for vmtoolsd shutdown.
          */
         retVal = GDP_ERROR_STOP;
         break;
      } else if (waitRes == (WSA_WAIT_EVENT_0 + 1)) {
         WSANETWORKEVENTS networkEvents;
         res = WSAEnumNetworkEvents(pluginData.sock, NULL, &networkEvents);
         if (res != 0) {
            g_info("%s: WSAEnumNetworkEvents failed: error=%d.\n",
                   __FUNCTION__, WSAGetLastError());
            retVal = GDP_ERROR_INTERNAL;
            break;
         }

         /*
          * Not checking networkEvents.iErrorCode[FD_READ_BIT]/
          * networkEvents.iErrorCode[FD_WRITE_BIT] for WSAENETDOWN,
          * since WSAEnumNetworkEvents should have returned WSAENETDOWN
          * if the error condition exists.
          */
         if (networkEvents.lNetworkEvents & netEvent) {
            retVal = GDP_ERROR_SUCCESS;
         } else { // Not expected
            g_info("%s: Unexpected network event from WSAEnumNetworkEvents.\n",
                   __FUNCTION__);
            retVal = GDP_ERROR_INTERNAL;
         }

         break;
      } else if (waitRes == WSA_WAIT_IO_COMPLETION) {
         gint64 curTime;
         gint64 passedTime;

         if (localTimeout == 0 ||
             localTimeout == WSA_INFINITE) {
            continue;
         }

         curTime = g_get_monotonic_time();
         passedTime = (curTime - startTime) / 1000;
         if (passedTime >= localTimeout) {
            retVal = GDP_ERROR_TIMEOUT;
            break;
         }

         startTime = curTime;
         localTimeout -= (DWORD)passedTime;
         continue;
      } else if (waitRes == WSA_WAIT_TIMEOUT) {
         retVal = GDP_ERROR_TIMEOUT;
         break;
      } else { // WSA_WAIT_FAILED
         g_info("%s: WSAWaitForMultipleEvents failed: error=%d.\n",
                __FUNCTION__, WSAGetLastError());
         retVal = GDP_ERROR_INTERNAL;
         break;
      }
   }

   /*
    * Cancel the association.
    */
   WSAEventSelect(pluginData.sock, NULL, 0);

   return retVal;

#else

   gint64 startTime;
   GdpError retVal;

   ASSERT(netEvent == POLLIN || netEvent == POLLOUT);
   ASSERT(pluginData.sock != INVALID_SOCKET);

   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deal with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      struct pollfd fds[2];
      int res;

      fds[0].fd = pluginData.eventStop;
      fds[0].events = POLLIN;
      fds[0].revents = 0;
      fds[1].fd = pluginData.sock;
      fds[1].events = (short)netEvent;
      fds[1].revents = 0;

      res = poll(fds, ARRAYSIZE(fds), timeout);
      if (res > 0) {
         if (fds[0].revents & POLLIN) {
            /*
             * Check comments in _WIN32.
             */
            retVal = GDP_ERROR_STOP;
         } else if (fds[1].revents & netEvent) {
            retVal = GDP_ERROR_SUCCESS;
         } else { // Not expected
            g_info("%s: Unexpected event from poll.\n", __FUNCTION__);
            retVal = GDP_ERROR_INTERNAL;
         }

         break;
      } else if (res == -1) {
         int err = errno;
         if (err == EINTR) {
            gint64 curTime;
            gint64 passedTime;

            if (timeout <= 0) {
               continue;
            }

            curTime = g_get_monotonic_time();
            passedTime = (curTime - startTime) / 1000;
            if (passedTime >= (gint64)timeout) {
               retVal = GDP_ERROR_TIMEOUT;
               break;
            }

            startTime = curTime;
            timeout -= (int)passedTime;
            continue;
         } else {
            g_info("%s: poll failed: error=%d.\n", __FUNCTION__, err);
            retVal = GDP_ERROR_INTERNAL;
            break;
         }
      } else if (res == 0) {
         retVal = GDP_ERROR_TIMEOUT;
         break;
      } else {
         g_info("%s: Unexpected poll return: %d.\n", __FUNCTION__, res);
         retVal = GDP_ERROR_INTERNAL;
         break;
      }
   }

   return retVal;
#endif
}


/*
 ******************************************************************************
 * GdpSend --
 *
 * Sends guest data to host side gdp daemon.
 *
 * @param[in]    buf      Send data buffer pointer
 * @param[in]    len      Send data buffer length
 * @param[in]    timeout  Timeout value in milliseconds,
 *                        negative value means an infinite timeout,
 *                        zero means no wait
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpSend(const char *buf, // IN
        int len,         // IN
        int timeout)     // IN
{
   GdpError retVal;

   ASSERT(buf != NULL && len > 0);
   ASSERT(pluginData.sock != INVALID_SOCKET);

   do {
      long res;
      int sockErr;

      res = send(pluginData.sock, buf, len, 0);
      if (res >= 0) {
         retVal = GDP_ERROR_SUCCESS;
         break;
      }

      sockErr = GetSockErr();
      if (sockErr == SYSERR_EINTR) {
         continue;
      } else if (SYSERR_WOULDBLOCK(sockErr)) {
         /*
          * Datagram send is not buffered, if host daemon is not running,
          * send returns error EHOSTUNREACH. In theory, this case should
          * not happen, we just follow standard async socket programming
          * paradigm here.
          */
         GdpError err;

         g_info("%s: Gdp send would block.\n", __FUNCTION__);

         err = GdpWaitForEvent(SOCK_WRITE, timeout);
         if (err == GDP_ERROR_SUCCESS) {
            continue;
         }

         retVal = err;
      } else if (sockErr == SYSERR_EHOSTUNREACH) {
         g_info("%s: send failed: host daemon unreachable.\n", __FUNCTION__);
         retVal = GDP_ERROR_UNREACH;
      } else if (sockErr == SYSERR_EMSGSIZE) {
         g_info("%s: send failed: message too large.\n", __FUNCTION__);
         retVal = GDP_ERROR_SEND_SIZE;
      } else {
         g_info("%s: send failed: error=%d.\n", __FUNCTION__, sockErr);
         retVal = GDP_ERROR_INTERNAL;
      }

      break;

   } while (TRUE);

   return retVal;
}


/*
 ******************************************************************************
 * GdpRecv --
 *
 * Receives reply from host side gdp daemon.
 *
 * @param[out]    buf      Receive buffer pointer
 * @param[in,out] len      Receive buffer length on input,
 *                         reply data length on output
 * @param[in]     timeout  Timeout value in milliseconds,
 *                         negative value means an infinite timeout,
 *                         zero means no wait
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpRecv(char *buf,   // OUT
        int *len,    // IN/OUT
        int timeout) // IN
{
   GdpError retVal;

   ASSERT(buf != NULL && len != NULL && *len > 0);
   ASSERT(pluginData.sock != INVALID_SOCKET);

   do {
      long res;
      int sockErr;

      res = recv(pluginData.sock, buf, *len, 0);
      if (res >= 0) {
         *len = (int)res;
         retVal = GDP_ERROR_SUCCESS;
         break;
      }

      sockErr = GetSockErr();
      if (sockErr == SYSERR_EINTR) {
         continue;
      } else if (SYSERR_WOULDBLOCK(sockErr)) {
         GdpError err = GdpWaitForEvent(SOCK_READ, timeout);
         if (err == GDP_ERROR_SUCCESS) {
            continue;
         }

         retVal = err;
      } else if (sockErr == SYSERR_EMSGSIZE) {
         g_info("%s: recv failed: buffer size too small.\n", __FUNCTION__);
         retVal = GDP_ERROR_RECV_SIZE;
      } else {
         g_info("%s: recv failed: error=%d.\n", __FUNCTION__, sockErr);
         retVal = GDP_ERROR_INTERNAL;
      }

      break;

   } while (TRUE);

   return retVal;
}


/*
 ******************************************************************************
 * GdpPublish --
 *
 * Publishes guest data to host side gdp daemon.
 *
 * @param[in]              msg       Buffer containing guest data to publish
 * @param[in]              msgLen    Guest data length
 * @param[out,optional]    reply     Buffer to receive reply from gdp daemon
 * @param[in,out,optional] replyLen  NULL when param reply is NULL, otherwise:
 *                                   reply buffer length on input,
 *                                   reply data length on output
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError codes otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpPublish(const char *msg, // IN
           int msgLen,      // IN
           char *reply,     // OUT, OPTIONAL
           int *replyLen)   // IN/OUT, OPTIONAL
{
   static GMutex mutex;

   GdpError err;
   char altRecvBuf[GDP_SEND_RECV_BUF_LEN]; // Enough for maximum size datagram
   int altRecvBufLen = (int) sizeof altRecvBuf;
   char *recvBuf;
   int *recvBufLen;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   ASSERT((reply == NULL && replyLen == NULL) ||
          (reply != NULL && replyLen != NULL && *replyLen > 0));

   g_mutex_lock(&mutex);

   if (Atomic_ReadBool(&pluginData.stopped)) {
      /*
       * Main thread has interrupted pool thread for vmtoolsd shutdown.
       */
      err = GDP_ERROR_STOP;
      goto exit;
   }

   if (pluginData.sock == INVALID_SOCKET && !GdpCreateSocket()) {
      err = GDP_ERROR_INTERNAL;
      goto exit;
   }

   err = GdpEmptyRecvQueue();
   if (err != GDP_ERROR_SUCCESS) {
      goto exit;
   }

   err = GdpSend(msg, msgLen,
                 GDP_SEND_TIMEOUT); // Should not time out in theory
   if (err != GDP_ERROR_SUCCESS) {
      g_info("%s: GdpSend failed: %s.\n", __FUNCTION__, gdpErrMsgs[err]);
      goto exit;
   }

   if (reply != NULL) {
      recvBuf = reply;
      recvBufLen = replyLen;
   } else {
      recvBuf = altRecvBuf;
      recvBufLen = &altRecvBufLen;
   }

   err = GdpRecv(recvBuf, recvBufLen, GDP_RECV_TIMEOUT);
   if (err != GDP_ERROR_SUCCESS) {
      g_info("%s: GdpRecv failed: %s.\n", __FUNCTION__, gdpErrMsgs[err]);
   }

exit:
   if (err == GDP_ERROR_INTERNAL) {
      /*
       * No need to close and recreate socket for these errors:
       *
       * GDP_ERROR_UNREACH
       * GDP_ERROR_TIMEOUT
       * GDP_ERROR_SEND_SIZE
       * GDP_ERROR_RECV_SIZE
       */
      GdpCloseSocket();
   }

   g_mutex_unlock(&mutex);

   g_debug("%s: Return: %s.\n", __FUNCTION__, gdpErrMsgs[err]);

   return err;
}


/*
 ******************************************************************************
 * GdpStop --
 *
 * Stops guest data publishing for vmtoolsd shutdown, called by main thread.
 *
 * @return GDP_ERROR_SUCCESS.
 *
 ******************************************************************************
 */

static GdpError
GdpStop(void)
{
   g_debug("%s: Entering ...\n", __FUNCTION__);
   Atomic_WriteBool(&pluginData.stopped, TRUE);
   GdpSetStopEvent();
   return GDP_ERROR_SUCCESS;
}


/*
 ******************************************************************************
 * GdpShutdown --
 *
 * Cleans up on shutdown.
 *
 * @param[in]  src     The source object, unused
 * @param[in]  ctx     The application context
 * @param[in]  data    Unused
 *
 ******************************************************************************
 */

static void
GdpShutdown(gpointer src,     // IN
            ToolsAppCtx *ctx, // IN
            gpointer data)    // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);
   g_object_set(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, NULL, NULL);
   GdpDestroy();
}


/*
 ******************************************************************************
 * ToolsOnLoad --
 *
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The application context
 *
 * @return The registration data.
 *
 ******************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx) // IN
{
   /*
    * Return NULL to disable the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("%s: Not running in vmsvc daemon: container name='%s'.\n",
             __FUNCTION__, ctx->name);
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("%s: Not running in a VMware VM.\n", __FUNCTION__);
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if VM is not running on ESX host.
    */
   {
      uint32 vmxVersion = 0;
      uint32 vmxType = VMX_TYPE_UNSET;
      if (!VmCheck_GetVersion(&vmxVersion, &vmxType) ||
          vmxType != VMX_TYPE_SCALABLE_SERVER) {
         g_info("%s: VM is not running on ESX host.\n", __FUNCTION__);
         return NULL;
      }
   }

   if (!GdpInit(ctx)) {
      g_info("%s: Failed to init plugin.\n", __FUNCTION__);
      return NULL;
   }

   {
      static ToolsPluginSvcGdp svcGdp = { GdpPublish, GdpStop };
      static ToolsPluginData regData = { "gdp", NULL, NULL, NULL };

      ToolsServiceProperty propGdp = { TOOLS_PLUGIN_SVC_PROP_GDP };

      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_SHUTDOWN, GdpShutdown, NULL },
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      ctx->registerServiceProperty(ctx->serviceObj, &propGdp);
      g_object_set(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, &svcGdp, NULL);

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
      return &regData;
   }
}

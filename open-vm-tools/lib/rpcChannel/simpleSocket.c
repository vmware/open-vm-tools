/*********************************************************
 * Copyright (C) 2013-2017,2019-2021 VMware, Inc. All rights reserved.
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
 * simpleSocket.c --
 *
 *    Simple wrappers for socket.
 *
 */

#include <stdlib.h>
#if defined(__linux__)
#include <arpa/inet.h>
#endif

#include "simpleSocket.h"
#include "vmci_defs.h"
#include "vmci_sockets.h"
#include "dataMap.h"
#include "err.h"
#include "debug.h"

#define LGPFX "SimpleSock: "


static int
SocketGetLastError(void);

/*
 *-----------------------------------------------------------------------------
 *
 * SocketStartup --
 *
 *      Win32 special socket init.
 *
 * Results:
 *      TRUE on success
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
SocketStartup(void)
{
#if defined(_WIN32)
   static Bool initialized = FALSE;
   int err;
   WSADATA wsaData;

   if (initialized) {
      return TRUE;
   }

   err = WSAStartup(MAKEWORD(2, 0), &wsaData);
   if (err) {
      Warning(LGPFX "Error in WSAStartup: %d[%s]\n", err,
              Err_Errno2String(err));
      return FALSE;
   }

   if (2 != LOBYTE(wsaData.wVersion) || 0 != HIBYTE(wsaData.wVersion)) {
      Warning(LGPFX "Unsupported Winsock version %d.%d\n",
              LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
      return FALSE;
   }

   initialized = TRUE;
#endif

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_Close --
 *
 *      wrapper for socket close.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Socket_Close(SOCKET sock)
{
   int res;

#if defined(_WIN32)
   res = closesocket(sock);
#else
   res = close(sock);
#endif

   if (res == SOCKET_ERROR) {
      int err = SocketGetLastError();
      Warning(LGPFX "Error in closing socket %d: %d[%s]\n",
              sock, err, Err_Errno2String(err));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * SocketGetLastError --
 *
 *      Get the last error code.
 *
 * Results:
 *      error code.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
SocketGetLastError(void)
{
#if defined(_WIN32)
   return WSAGetLastError();
#else
   return errno;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_Recv --
 *
 *      Block until given number of bytes of data is received or error occurs.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
Socket_Recv(SOCKET fd,      // IN
            char *buf,      // OUT
            int len)        // IN
{
   int remaining = len;
   int sysErr;

   while (remaining > 0) {
      int rv = recv(fd, buf , remaining, 0);
      if (rv == 0) {
         Debug(LGPFX "Socket %d closed by peer.", fd);
         return FALSE;
      }
      if (rv == SOCKET_ERROR) {
         sysErr = SocketGetLastError();
         if (sysErr == SYSERR_EINTR) {
            continue;
         }
         Warning(LGPFX "Recv error for socket %d: %d[%s]", fd, sysErr,
                 Err_Errno2String(sysErr));
         return FALSE;
      }
      remaining -= rv;
      buf += rv;
   }

   Debug(LGPFX "Recved %d bytes from socket %d\n", len, fd);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_Send --
 *
 *      Block until the given number of bytes of data is sent or error occurs.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
Socket_Send(SOCKET fd,      // IN
            char *buf,      // IN
            int len)        // IN
{
   int left = len;
   int sent = 0;
   int sysErr;

   while (left > 0) {
      int rv = send(fd, buf + sent, left, 0);
      if (rv == SOCKET_ERROR) {
         sysErr = SocketGetLastError();
         if (sysErr == SYSERR_EINTR) {
            continue;
         }
         Warning(LGPFX "Send error for socket %d: %d[%s]", fd, sysErr,
                 Err_Errno2String(sysErr));
         return FALSE;
      }
      left -= rv;
      sent += rv;
   }

   Debug(LGPFX "Sent %d bytes from socket %d\n", len, fd);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * SocketConnectVmciInternal --
 *
 *      Connect to a VSOCK destination in blocking mode
 *
 * Results:
 *      The socket created/connected upon success.
 *      INVALID_SOCKET upon a failure:
 *         apiErr and sysErr are populated with the proper error codes.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */
static SOCKET
SocketConnectVmciInternal(const struct sockaddr_vm *destAddr, // IN
                          unsigned int localPort,             // IN
                          ApiError *apiErr,                   // OUT
                          int *sysErr)                        // OUT
{
   SOCKET fd;
   struct sockaddr_vm localAddr;

   fd = socket(destAddr->svm_family, SOCK_STREAM, 0);
   if (fd == INVALID_SOCKET) {
      *apiErr = SOCKERR_SOCKET;
      *sysErr = SocketGetLastError();
      Warning(LGPFX "failed to create socket, error %d: %s\n",
              *sysErr, Err_Errno2String(*sysErr));
      return INVALID_SOCKET;
   }

   memset(&localAddr, 0, sizeof localAddr);
   localAddr.svm_family = destAddr->svm_family;
   localAddr.svm_cid = VMCISock_GetLocalCID();
   localAddr.svm_port = localPort;

   if (bind(fd, (struct sockaddr *)&localAddr, sizeof localAddr) != 0) {
      *apiErr = SOCKERR_BIND;
      *sysErr = SocketGetLastError();
      Debug(LGPFX "Couldn't bind on source port %d, error %d, %s\n",
            localPort, *sysErr, Err_Errno2String(*sysErr));
      Socket_Close(fd);
      return INVALID_SOCKET;
   }

   Debug(LGPFX "Successfully bound to source port %d\n", localPort);

   if (connect(fd, (struct sockaddr *)destAddr, sizeof *destAddr) != 0) {
      *apiErr = SOCKERR_CONNECT;
      *sysErr = SocketGetLastError();
      Warning(LGPFX "failed to connect (%d => %d), error %d: %s\n",
              localPort, destAddr->svm_port, *sysErr,
              Err_Errno2String(*sysErr));
      Socket_Close(fd);
      return INVALID_SOCKET;
   }

   *apiErr = SOCKERR_SUCCESS;
   *sysErr = 0;
   return fd;
}


/*
 *----------------------------------------------------------------------------
 *
 * Socket_ConnectVMCI --
 *
 *      Connect to a VMCI port in blocking mode.
 *      If isPriv is true, we will try to bind the local port to a port that
 *      is less than 1024.
 *
 * Results:
 *      The socket created/connected upon success.
 *      INVALID_SOCKET upon a failure:
 *         outApiErr and outSysErr are populated with proper error codes.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

SOCKET
Socket_ConnectVMCI(unsigned int cid,                  // IN
                   unsigned int port,                 // IN
                   gboolean isPriv,                   // IN
                   ApiError *outApiErr,               // OUT optional
                   int *outSysErr)                    // OUT optional
{
   struct sockaddr_vm addr;
   unsigned int localPort;
   SOCKET fd;
   int sysErr = 0;
   ApiError apiErr;
   int vsockDev = -1;
   int family = VMCISock_GetAFValueFd(&vsockDev);
   int retryCount = 0;

   if (family == -1) {
      Warning(LGPFX "Couldn't get VMCI socket family info.");
      apiErr = SOCKERR_VMCI_FAMILY;
      fd = INVALID_SOCKET;
      goto done;
   }

   if (!SocketStartup()) {
      apiErr = SOCKERR_STARTUP;
      fd = INVALID_SOCKET;
      goto done;
   }

   memset((char *)&addr, 0, sizeof addr);
   addr.svm_family = family;
   addr.svm_cid = cid;
   addr.svm_port = port;

   Debug(LGPFX "creating new socket, connecting to %u:%u\n", cid, port);

   if (!isPriv) {
      fd = SocketConnectVmciInternal(&addr, VMADDR_PORT_ANY,
                                     &apiErr, &sysErr);
      goto done;
   }

   /* We are required to use a privileged source port. */
   localPort = PRIVILEGED_PORT_MAX;
   while (localPort >= PRIVILEGED_PORT_MIN) {
      fd = SocketConnectVmciInternal(&addr, localPort, &apiErr, &sysErr);
      if (fd != INVALID_SOCKET) {
         goto done;
      }
      if (apiErr == SOCKERR_BIND && sysErr == SYSERR_EADDRINUSE) {
         --localPort;
         continue; /* Try next port */
      }
      if (apiErr == SOCKERR_CONNECT && sysErr == SYSERR_ECONNRESET) {
         /*
          * VMX might be slow releasing a port pair
          * when another client closed the client side end.
          * Simply try next port.
          */
         --localPort;
         continue;
      }
      if (apiErr == SOCKERR_CONNECT && sysErr == SYSERR_EINTR) {
         /*
          * EINTR on connect due to signal.
          * Try again using the same port.
          */
         continue;
      }

      if (apiErr == SOCKERR_CONNECT && sysErr == SYSERR_ENOBUFS) {
         /*
          * ENOBUFS can happen if we're out of vsocks in the kernel.
          * Delay a bit and try again using the same port.
          * Have a retry count in case something has gone horribly wrong.
          */
         if (++retryCount > 5) {
            goto done;
         }
#ifdef _WIN32
         Sleep(1);
#else
         usleep(1000);
#endif
         continue;
      }
      /* Unrecoverable error occurred */
      goto done;
   }

   Debug(LGPFX "Failed to connect using a privileged port.\n");

done:

   VMCISock_ReleaseAFValueFd(vsockDev);

   if (outApiErr) {
      *outApiErr = apiErr;
   }

   if (outSysErr) {
      *outSysErr = sysErr;
   }

   if (fd != INVALID_SOCKET) {
      Debug(LGPFX "socket %d connected\n", fd);
   }
   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_DecodePacket --
 *
 *    Helper function to decode received packet in DataMap encoding format.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
Socket_DecodePacket(const char *recvBuf,       // IN
                    int fullPktLen,            // IN
                    char **payload,            // OUT
                    int32 *payloadLen)         // OUT
{
   ErrorCode res;
   DataMap map;
   char *buf;
   int32 len;

   *payload = NULL;
   *payloadLen = 0;

   /* decoding the packet */
   res = DataMap_Deserialize(recvBuf, fullPktLen, &map);
   if (res != DMERR_SUCCESS) {
      Debug(LGPFX "Error in dataMap decoding, error=%d\n", res);
      return FALSE;
   }

   res = DataMap_GetString(&map, GUESTRPCPKT_FIELD_PAYLOAD, &buf, &len);
   if (res == DMERR_SUCCESS) {
      char *tmpPtr = malloc(len + 1);
      if (tmpPtr == NULL) {
         Debug(LGPFX "Error in allocating memory\n");
         goto error;
      }
      memcpy(tmpPtr, buf, len);
      /* add a trailing 0 for backward compatibility */
      tmpPtr[len] = '\0';

      *payload = tmpPtr;
      *payloadLen = len;
   } else {
      Debug(LGPFX "Error in decoding payload, error=%d\n", res);
      goto error;
   }

   DataMap_Destroy(&map);
   return TRUE;

error:
   DataMap_Destroy(&map);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_PackSendData --
 *
 *    Helper function for building send packet and serialize it.
 *
 * Result:
 *    TRUE on sucess, FALSE otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
Socket_PackSendData(const char *buf,             // IN
                    int len,                     // IN
                    Bool fastClose,              // IN
                    char **serBuf,               // OUT
                    int32 *serBufLen)            // OUT
{
   DataMap map;
   ErrorCode res;
   char *newBuf;
   gboolean mapCreated = FALSE;
   int64 pktType = GUESTRPCPKT_TYPE_DATA;

   res = DataMap_Create(&map);
   if (res != DMERR_SUCCESS) {
      goto error;
   }

   mapCreated = TRUE;
   res = DataMap_SetInt64(&map, GUESTRPCPKT_FIELD_TYPE,
                          pktType, TRUE);
   if (res != DMERR_SUCCESS) {
      goto error;
   }

   newBuf = malloc(len);
   if (newBuf == NULL) {
      Debug(LGPFX "Error in allocating memory.\n");
      goto error;
   }
   memcpy(newBuf, buf, len);
   res = DataMap_SetString(&map, GUESTRPCPKT_FIELD_PAYLOAD, newBuf,
                           len, TRUE);
   if (res != DMERR_SUCCESS) {
      free(newBuf);
      goto error;
   }

   if (fastClose) {
      res = DataMap_SetInt64(&map, GUESTRPCPKT_FIELD_FAST_CLOSE, TRUE, TRUE);
      if (res != DMERR_SUCCESS) {
         goto error;
      }
   }

   res = DataMap_Serialize(&map, serBuf, serBufLen);
   if (res != DMERR_SUCCESS) {
      goto error;
   }

   DataMap_Destroy(&map);
   return TRUE;

error:
   if (mapCreated) {
      DataMap_Destroy(&map);
   }
   Debug(LGPFX "Error in dataMap encoding\n");
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_RecvPacket --
 *
 *    Helper function to recv a dataMap packet over the socket.
 *    The caller has to *free* the payload to avoid memory leak.
 *
 * Result:
 *    TRUE on sucess, FALSE otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
Socket_RecvPacket(SOCKET sock,               // IN
                  char **payload,            // OUT
                  int *payloadLen)           // OUT
{
   gboolean ok;
   uint32 packetLen;
   uint32 partialPktLen;
   int packetLenSize = sizeof packetLen;
   int fullPktLen;
   char *recvBuf;

   ok = Socket_Recv(sock, (char *)&packetLen, packetLenSize);
   if (!ok) {
      Debug(LGPFX "error in recving packet header, err=%d\n",
            SocketGetLastError());
      return FALSE;
   }

   partialPktLen = ntohl(packetLen);
   if (partialPktLen > INT_MAX - packetLenSize) {
      Panic(LGPFX "Invalid packetLen value 0x%08x\n", packetLen);
   }

   fullPktLen = partialPktLen + packetLenSize;
   recvBuf = malloc(fullPktLen);
   if (recvBuf == NULL) {
      Debug(LGPFX "Could not allocate recv buffer.\n");
      return FALSE;
   }

   memcpy(recvBuf, &packetLen, packetLenSize);
   ok = Socket_Recv(sock, recvBuf + packetLenSize,
                     fullPktLen - packetLenSize);
   if (!ok) {
      Debug(LGPFX "error in recving packet, err=%d\n",
            SocketGetLastError());
      free(recvBuf);
      return FALSE;
   }

   ok = Socket_DecodePacket(recvBuf, fullPktLen, payload, payloadLen);
   free(recvBuf);
   return ok;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Socket_SendPacket --
 *
 *    Helper function to send a dataMap packet over the socket.
 *
 * Result:
 *    TRUE on sucess, FALSE otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
Socket_SendPacket(SOCKET sock,               // IN
                  const char *payload,       // IN
                  int payloadLen,            // IN
                  Bool fastClose)            // IN
{
   gboolean ok;
   char *sendBuf;
   int sendBufLen;

   if (!Socket_PackSendData(payload, payloadLen, fastClose,
                            &sendBuf, &sendBufLen)) {
      return FALSE;
   }

   ok = Socket_Send(sock, sendBuf, sendBufLen);
   free(sendBuf);

   return ok;
}

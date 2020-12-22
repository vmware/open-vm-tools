/*********************************************************
 * Copyright (C) 2019-2020 VMware, Inc. All rights reserved.
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
 * @file guestStoreClientLib.c
 *
 * GuestStore client library, connecting to vmtoolsd GuestStore plugin.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include<stdarg.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
/*
 * #define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
 */
#define TLS_INDEX_TYPE  DWORD
#else
#define _GNU_SOURCE  // For struct ucred
#define __USE_GNU    // For struct ucred (glibc 2.17)
#include <sys/socket.h>
#undef __USE_GNU
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
/*
 * typedef unsigned int pthread_key_t;
 * #define PTHREAD_KEYS_MAX	1024
 */
#define TLS_INDEX_TYPE  pthread_key_t
#define TLS_OUT_OF_INDEXES  ((pthread_key_t)-1)
#endif

#include "guestStoreClientLibInt.h"

#include "vm_version.h"
#include "embed_version.h"
#include "gueststoreclientlib_version.h"
VM_EMBED_VERSION(GUESTSTORECLIENTLIB_VERSION_STRING);

#define GSLIBLOG_TAG      "[guestStoreClientLib] "
#define GSLIBLOG_TAG_LEN  (sizeof(GSLIBLOG_TAG) - 1)


/*
 * Library Init/DeInit reference count.
 */
static Atomic_uint32 initLibCount = { 0 };

/*
 * Thread local storage index for CallCtx pointer.
 */
static TLS_INDEX_TYPE callCtxTlsIndex = TLS_OUT_OF_INDEXES;


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreAllocTls --
 *
 *      Allocate a thread local storage index for CallCtx pointer.
 *
 * Results:
 *      GSLIBERR_SUCCESS or GSLIBERR_TLS.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreAllocTls(void)
{
   ASSERT(callCtxTlsIndex == TLS_OUT_OF_INDEXES);

#ifdef _WIN32
   callCtxTlsIndex = TlsAlloc();
#else
   if (pthread_key_create(&callCtxTlsIndex, NULL) != 0) {
      callCtxTlsIndex = TLS_OUT_OF_INDEXES;
   }
#endif

   return (callCtxTlsIndex == TLS_OUT_OF_INDEXES) ? GSLIBERR_TLS :
                                                    GSLIBERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreFreeTls --
 *
 *      Free the allocated thread local storage index for CallCtx pointer.
 *
 * Results:
 *      GSLIBERR_SUCCESS or GSLIBERR_TLS.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreFreeTls(void)
{
   Bool res;

   ASSERT(callCtxTlsIndex != TLS_OUT_OF_INDEXES);

#ifdef _WIN32
   res = TlsFree(callCtxTlsIndex) ? TRUE : FALSE;
#else
   res = pthread_key_delete(callCtxTlsIndex) == 0 ? TRUE : FALSE;
#endif

   callCtxTlsIndex = TLS_OUT_OF_INDEXES;

   return res ? GSLIBERR_SUCCESS : GSLIBERR_TLS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreSetTls --
 *
 *      Set CallCtx pointer to the slot of the thread local storage index.
 *      When ctx is NULL, clear the slot.
 *
 * Results:
 *      GSLIBERR_SUCCESS or GSLIBERR_TLS.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreSetTls(CallCtx *ctx)  // IN
{
   Bool res;

   ASSERT(callCtxTlsIndex != TLS_OUT_OF_INDEXES);

#ifdef _WIN32
   res = TlsSetValue(callCtxTlsIndex, ctx) ? TRUE : FALSE;
#else
   res = pthread_setspecific(callCtxTlsIndex, ctx) == 0 ? TRUE : FALSE;
#endif

   return res ? GSLIBERR_SUCCESS : GSLIBERR_TLS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreGetTls --
 *
 *      Get CallCtx pointer from the slot of the thread local storage index.
 *
 * Results:
 *      CallCtx/NULL pointer
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static CallCtx *
GuestStoreGetTls(void)
{
   ASSERT(callCtxTlsIndex != TLS_OUT_OF_INDEXES);

#ifdef _WIN32
   return (CallCtx *)TlsGetValue(callCtxTlsIndex);
#else
   return (CallCtx *)pthread_getspecific(callCtxTlsIndex);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStore_Init --
 *
 *      GuestStore client library Init entry point function.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestStoreLibError
GuestStore_Init(void)
{
   if (Atomic_ReadInc32(&initLibCount) == 0) {
      GuestStoreLibError retVal = GuestStoreAllocTls();
      if (retVal != GSLIBERR_SUCCESS) {
         Atomic_Dec32(&initLibCount);
      }
      return retVal;
   }

   return GSLIBERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStore_DeInit --
 *
 *      GuestStore client library DeInit entry point function.
 *      Call of GuestStore_DeInit should match succeeded GuestStore_Init call.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestStoreLibError
GuestStore_DeInit(void)
{
   uint32 oldVal;
   uint32 newVal;

   do {
      oldVal = Atomic_Read32(&initLibCount);
      if (oldVal == 0) {
         return GSLIBERR_NOT_INITIALIZED;
      }

      newVal = oldVal - 1;
   } while (Atomic_ReadIfEqualWrite32(&initLibCount,
                                      oldVal, newVal) != oldVal);

   if (oldVal == 1) {
      return GuestStoreFreeTls();
   }

   return GSLIBERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreLogV --
 *
 *      Internal log function.
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
GuestStoreLogV(CallCtx *ctx,                 // IN
               GuestStoreLibLogLevel level,  // IN
               const char *fmt,              // IN
               va_list args)                 // IN
{
   char buf[1024] = GSLIBLOG_TAG;

   ASSERT(ctx != NULL && ctx->logger != NULL);

   Str_Vsnprintf(buf + GSLIBLOG_TAG_LEN, sizeof(buf) - GSLIBLOG_TAG_LEN,
                 fmt, args);
   ctx->logger(level, buf, ctx->clientData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreLog --
 *
 *      Internal log function.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestStoreLog(CallCtx *ctx,                 // IN
              GuestStoreLibLogLevel level,  // IN
              const char *fmt, ...)         // IN
{
   va_list args;

   ASSERT(ctx != NULL && ctx->logger != NULL);

   va_start(args, fmt);
   GuestStoreLogV(ctx, level, fmt, args);
   va_end(args);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreFreeCtxResources --
 *
 *      Free resources allocated in each GuestStore_GetContent call.
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
GuestStoreFreeCtxResources(CallCtx *ctx)  // IN / OUT
{
   int res;

   if (ctx->output != NULL) {
      fclose(ctx->output);
      ctx->output = NULL;

      /*
       * The output file was created after the content size was received.
       */
      ASSERT(ctx->contentSize >= 0);

      /*
       * Delete the output file if not all the content bytes were received.
       */
      if (ctx->contentBytesReceived != ctx->contentSize) {
         res = Posix_Unlink(ctx->outputPath);
         if (res != 0) {
            LOG_ERR(ctx, "Posix_Unlink failed: outputPath='%s', error=%d.",
                    ctx->outputPath, errno);
         }
      }
   }

   if (ctx->sd != INVALID_SOCKET) {
#ifdef _WIN32
      res = closesocket(ctx->sd);
#else
      res = close(ctx->sd);
#endif

      if (res == SOCKET_ERROR) {
         LOG_ERR(ctx, "close failed on socket %d: error=%d.",
                 ctx->sd, SocketGetLastError());
      }

      ctx->sd = INVALID_SOCKET;
   }

   free(ctx->buf);
   ctx->buf = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreCreateOutputFile --
 *
 *      Create an output file stream for writing.
 *
 *      If the given file exists, its content is destroyed.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreCreateOutputFile(CallCtx *ctx)  // IN / OUT
{
   FILE *output = Posix_Fopen(ctx->outputPath, "wb");
   if (output == NULL) {
      LOG_ERR(ctx, "Posix_Fopen failed: outputPath='%s', error=%d.",
              ctx->outputPath, errno);
      return GSLIBERR_CREATE_OUTPUT_FILE;
   }

   ctx->output = output;
   return GSLIBERR_SUCCESS;
}


#ifndef _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreConnect --
 *
 *      Connect to vmtoolsd GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestStoreLibError
GuestStoreConnect(CallCtx *ctx)  // IN / OUT
{
   struct sockaddr_un svcAddr;
   int res;
   int err;
   struct ucred peerCred;
   socklen_t peerCredLen;

   ctx->sd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (ctx->sd == INVALID_SOCKET) {
      LOG_ERR(ctx, "socket failed: error=%d.", SocketGetLastError());
      return GSLIBERR_CONNECT_GENERIC;
   }

   svcAddr.sun_family = AF_UNIX;
   ASSERT(sizeof(GUESTSTORE_PIPE_NAME) < sizeof(svcAddr.sun_path));
   memcpy(svcAddr.sun_path, GUESTSTORE_PIPE_NAME,
          sizeof(GUESTSTORE_PIPE_NAME));

   do {
      res = connect(ctx->sd, (struct sockaddr*)&svcAddr,
                    (socklen_t)sizeof(svcAddr));
   } while (res == SOCKET_ERROR &&
            (err = SocketGetLastError()) == SYSERR_EINTR);

   if (res == SOCKET_ERROR) {
      LOG_ERR(ctx, "connect failed on socket %d: error=%d.",
              ctx->sd, err);

      if (err == SYSERR_ECONNREFUSED) {
         return GSLIBERR_CONNECT_SERVICE_NOT_RUNNING;
      } else if (err == SYSERR_EACCESS) {
         return GSLIBERR_CONNECT_PERMISSION_DENIED;
      } else {
         return GSLIBERR_CONNECT_GENERIC;
      }
   }

   /*
    * On Linux, the SO_PEERCRED socket option will give us the PID,
    * effective UID, and GID of the peer (the server in this case).
    */
   peerCredLen = (socklen_t)sizeof(peerCred);
   res = getsockopt(ctx->sd, SOL_SOCKET, SO_PEERCRED, &peerCred, &peerCredLen);
   if (res == SOCKET_ERROR) {
      LOG_ERR(ctx, "getsockopt SO_PEERCRED failed: error=%d.",
              SocketGetLastError());
      return GSLIBERR_CONNECT_GENERIC;
   } else if (peerCred.uid != 0) {
      LOG_ERR(ctx, "Peer is not supper user.");
      return GSLIBERR_CONNECT_SECURITY_VIOLATION;
   }

   return GSLIBERR_SUCCESS;
}

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreRecvBytes --
 *
 *      Partially receive bytes from vmtoolsd GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreRecvBytes(CallCtx *ctx,        // IN
                    char *buf,           // OUT
                    int bytesToRecv,     // IN
                    int *bytesReceived)  // OUT
{
   GuestStoreLibError retVal;

   do {
      int res;

      res = recv(ctx->sd,  // Synchronous recv
                 buf,
                 bytesToRecv,
                 0);
      if (res > 0) {
         *bytesReceived = res;
         retVal = GSLIBERR_SUCCESS;
         break;
      } else if (res == 0) {
         LOG_ERR(ctx, "peer closed on socket %d.", ctx->sd);
         retVal = GSLIBERR_CONNECT_PEER_RESET;
         break;
      } else {  // SOCKET_ERROR
         int err = SocketGetLastError();
         if (err == SYSERR_EINTR) {
            continue;
         }

         LOG_ERR(ctx, "recv failed on socket %d: error=%d.",
                 ctx->sd, err);
         retVal = GSLIBERR_RECV;
         break;
      }
   } while (TRUE);

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreSendBytes --
 *
 *      Send the specified amount of bytes to vmtoolsd GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreSendBytes(CallCtx *ctx,     // IN
                    char *buf,        // IN
                    int bytesToSend)  // IN
{
   GuestStoreLibError retVal = GSLIBERR_SUCCESS;
   int bytesSent = 0;

   while (bytesSent < bytesToSend) {
      int res;
      res = send(ctx->sd,  // Synchronous send
                 buf + bytesSent,
                 bytesToSend - bytesSent,
                 0);
      if (res == SOCKET_ERROR) {
         int err = SocketGetLastError();
         if (err == SYSERR_EINTR) {
            continue;
         }

         LOG_ERR(ctx, "send failed on socket %d: error=%d.",
                 ctx->sd, err);
         retVal = GSLIBERR_SEND;
         break;
      }

      bytesSent += res;
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreSendHTTPRequest --
 *
 *      Send HTTP request for content download to vmtoolsd GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreSendHTTPRequest(const char *contentPath,  // IN
                          CallCtx *ctx)             // IN
{
   int len = 0;
   int pathLen;
   int index;

   /*
    * HTTP GET request: GET <path> HTTP/1.1\r\n\r\n
    * excluding <path>, it is 17 bytes.
    * Reserve 1024 bytes for it.
    *
    * Length of contentPath is restricted to GUESTSTORE_CONTENT_PATH_MAX (1024)
    * maximum length after URL escaping is 3 * GUESTSTORE_CONTENT_PATH_MAX.
    *
    * ctx->bufSize is GUESTSTORE_RESPONSE_BUFFER_SIZE (64 * 1024)
    */
   ASSERT((1024 + 3 * GUESTSTORE_CONTENT_PATH_MAX) < ctx->bufSize);

   #define COPY_STR_TO_BUF(str)                      \
      memcpy(ctx->buf + len, str, sizeof(str) - 1);  \
      len += (int)(sizeof(str) - 1)

   COPY_STR_TO_BUF(HTTP_REQ_METHOD_GET);
   COPY_STR_TO_BUF(" ");

   /*
    * ' ', '?' and '%' are the 3 protocol characters GuestStore plugin parses.
    */
   pathLen = (int)strlen(contentPath);
   for (index = 0; index < pathLen; index++) {
      char c = contentPath[index];
      if (c == ' ') {
         COPY_STR_TO_BUF("%20");
      } else if (c == '%') {
         COPY_STR_TO_BUF("%25");
      } else if (c == '?') {
         COPY_STR_TO_BUF("%3F");
      } else {
         *(ctx->buf + len++) = c;
      }
   }

   COPY_STR_TO_BUF(" ");
   COPY_STR_TO_BUF(HTTP_VER);
   COPY_STR_TO_BUF(HTTP_HEADER_END);

   return GuestStoreSendBytes(ctx, ctx->buf, len);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreRecvHTTPResponseHeader --
 *
 *      Receive HTTP response header from vmtoolsd GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreRecvHTTPResponseHeader(CallCtx *ctx)  // IN
{
   int totalBytesReceived = 0;
   int recvBufSize = ctx->bufSize - 1;  // Reserve the last byte for '\0'
   char *httpHeaderEnd;
   char *next_token;
   char *httpVer;
   char *httpStatus;
   int status;
   char *contentLengthHeader;
   char *content;
   int httpResHeaderLen;
   GuestStoreLibError retVal;

   do {
      int bytesReceived = 0;

      retVal = GuestStoreRecvBytes(ctx, ctx->buf + totalBytesReceived,
                                   recvBufSize - totalBytesReceived,
                                   &bytesReceived);
      if (retVal != GSLIBERR_SUCCESS) {
         return retVal;
      }

      totalBytesReceived += bytesReceived;
      ctx->buf[totalBytesReceived] = '\0';
      httpHeaderEnd = strstr(ctx->buf, HTTP_HEADER_END);
      if (httpHeaderEnd != NULL) {
         *httpHeaderEnd = '\0';
         break;
      }

      if (totalBytesReceived == recvBufSize) {
         LOG_ERR(ctx, "Protocol header end mark not found.");
         return GSLIBERR_SERVER;
      }
   } while (TRUE);

   httpVer = strtok_r(ctx->buf, " ", &next_token);
   if (NULL == httpVer || strcmp(httpVer, HTTP_VER) != 0) {
      LOG_ERR(ctx, "Protocol version not correct.");
      return GSLIBERR_SERVER;
   }

   httpStatus = strtok_r(NULL, " ", &next_token);
   if (NULL == httpStatus) {
      LOG_ERR(ctx, "Protocol status code not found.");
      return GSLIBERR_SERVER;
   }

   status = atoi(httpStatus);

   if (status == HTTP_STATUS_CODE_FORBIDDEN) {
      LOG_ERR(ctx, "Content forbidden.");
      return GSLIBERR_CONTENT_FORBIDDEN;
   }

   if (status == HTTP_STATUS_CODE_NOT_FOUND) {
      LOG_ERR(ctx, "Content not found.");
      return GSLIBERR_CONTENT_NOT_FOUND;
   }

   if (status != HTTP_STATUS_CODE_OK) {
      LOG_ERR(ctx, "Invalid protocol status '%s'.", httpStatus);
      return GSLIBERR_SERVER;
   }

   contentLengthHeader = strstr(httpStatus + strlen(httpStatus) + 1,
                                CONTENT_LENGTH_HEADER);
   if (NULL == contentLengthHeader) {
      LOG_ERR(ctx, "Protocol content length not found.");
      return GSLIBERR_SERVER;
   }

   contentLengthHeader += CONTENT_LENGTH_HEADER_LEN;
   while (*contentLengthHeader >= '0' && *contentLengthHeader <= '9') {
      ctx->contentSize = ctx->contentSize * 10 + (*contentLengthHeader - '0');
      contentLengthHeader++;
   }

   if (ctx->contentSize < 0) {
      LOG_ERR(ctx, "Invalid protocol content length.");
      return GSLIBERR_SERVER;
   }

   /*
    * We've got content to save, create the output file now.
    */
   retVal = GuestStoreCreateOutputFile(ctx);
   if (retVal != GSLIBERR_SUCCESS) {
      return retVal;
   }

   /*
    * Save content bytes that follow HTTP response header.
    */
   content = httpHeaderEnd + HTTP_HEADER_END_LEN;
   httpResHeaderLen = (int)(content - ctx->buf);
   if (httpResHeaderLen < totalBytesReceived) {
      int contentLen = totalBytesReceived - httpResHeaderLen;

      ctx->contentBytesReceived += contentLen;
      if (ctx->contentBytesReceived > ctx->contentSize) {
         LOG_ERR(ctx, "Bytes received exceeded content size.");
         return GSLIBERR_SERVER;
      }

      if (fwrite(content, sizeof(char), contentLen, ctx->output) != contentLen) {
         LOG_ERR(ctx, "fwrite failed: error=%d.", errno);
         return GSLIBERR_WRITE_OUTPUT_FILE;
      }

      if (!REPORT_PROGRESS(ctx)) {
         LOG_ERR(ctx, "Request cancelled.");
         return GSLIBERR_CANCELLED;
      }
   }

   return GSLIBERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreRecvHTTPResponseBody --
 *
 *      Receive HTTP response body, i.e., content bytes, from vmtoolsd
 *      GuestStore plugin.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreLibError
GuestStoreRecvHTTPResponseBody(CallCtx *ctx)  // IN
{
   GuestStoreLibError retVal = GSLIBERR_SUCCESS;

   while (ctx->contentBytesReceived < ctx->contentSize) {
      int bytesReceived = 0;

      retVal = GuestStoreRecvBytes(ctx, ctx->buf, ctx->bufSize, &bytesReceived);
      if (retVal != GSLIBERR_SUCCESS) {
         break;
      }

      ctx->contentBytesReceived += bytesReceived;
      if (ctx->contentBytesReceived > ctx->contentSize) {
         LOG_ERR(ctx, "Bytes received exceeded content size.");
         retVal = GSLIBERR_SERVER;
         break;
      }

      if (fwrite(ctx->buf, sizeof(char), bytesReceived, ctx->output) != bytesReceived) {
         LOG_ERR(ctx, "fwrite failed: error=%d.", errno);
         retVal = GSLIBERR_WRITE_OUTPUT_FILE;
         break;
      }

      if (!REPORT_PROGRESS(ctx)) {
         LOG_ERR(ctx, "Request cancelled.");
         retVal = GSLIBERR_CANCELLED;
         break;
      }
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStore_GetContent --
 *
 *      GuestStore client library GetContent entry point function.
 *
 * Results:
 *      GSLIBERR_SUCCESS or an error code of GSLIBERR_*.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestStoreLibError
GuestStore_GetContent(
   const char *contentPath,                     // IN
   const char *outputPath,                      // IN
   GuestStore_Logger logger,                    // IN, OPTIONAL
   GuestStore_Panic panic,                      // IN, OPTIONAL
   GuestStore_GetContentCallback getContentCb,  // IN, OPTIONAL
   void *clientData)                            // IN, OPTIONAL
{
   GuestStoreLibError retVal;
   CallCtx ctx = { 0 };
#ifdef _WIN32
   WSADATA wsaData;
   int res;
#endif

   /*
    * Set ctx before first LOG_ERR.
    */
   ctx.contentPath = contentPath ? contentPath : "";
   ctx.outputPath = outputPath ? outputPath : "";
   ctx.logger = logger;
   ctx.panic = panic;
   ctx.getContentCb = getContentCb;
   ctx.clientData = clientData;
   ctx.sd = INVALID_SOCKET;

   if (contentPath == NULL || *contentPath != '/' ||
      strlen(contentPath) > GUESTSTORE_CONTENT_PATH_MAX) {
      LOG_ERR(&ctx, "Invalid content path.");
      return GSLIBERR_INVALID_PARAMETER;
   }

   if (outputPath == NULL || *outputPath == '\0') {
      LOG_ERR(&ctx, "Invalid output file path.");
      return GSLIBERR_INVALID_PARAMETER;
   }

   if (Atomic_Read32(&initLibCount) == 0 ||
       callCtxTlsIndex == TLS_OUT_OF_INDEXES) {
      LOG_ERR(&ctx, "Library is not properly initialized.");
      return GSLIBERR_NOT_INITIALIZED;
   }

   retVal = GuestStoreSetTls(&ctx);
   if (retVal != GSLIBERR_SUCCESS) {
      LOG_ERR(&ctx, "GuestStoreSetTls failed.");
      return retVal;
   }

#ifdef _WIN32
   res = WSAStartup(MAKEWORD(2, 2), &wsaData);
   if (res != 0) {
      retVal = GSLIBERR_CONNECT_GENERIC;
      LOG_ERR(&ctx, "WSAStartup failed: error=%d.", res);
      goto exit;
   }
#endif

   retVal = GuestStoreConnect(&ctx);
   if (retVal != GSLIBERR_SUCCESS) {
      goto exit;
   }

   ctx.bufSize = GUESTSTORE_RESPONSE_BUFFER_SIZE;
   ctx.buf = (char *)Util_SafeMalloc(ctx.bufSize);

   retVal = GuestStoreSendHTTPRequest(contentPath, &ctx);
   if (retVal != GSLIBERR_SUCCESS) {
      goto exit;
   }

   retVal = GuestStoreRecvHTTPResponseHeader(&ctx);
   if (retVal != GSLIBERR_SUCCESS) {
      goto exit;
   }

   retVal = GuestStoreRecvHTTPResponseBody(&ctx);

exit:

   GuestStoreFreeCtxResources(&ctx);  // Should be before WSACleanup()

#ifdef _WIN32
   if (res == 0) {
      if (retVal != GSLIBERR_SUCCESS) {
         /*
          * WSASetLastError needs a successful WSAStartup call,
          * WSAGetLastError does not.
          *
          * Note: WSACleanup may change WSA last error again.
          */
         WSASetLastError(ctx.winWSAErrNum);
      }

      WSACleanup();
   }
#endif

   GuestStoreSetTls(NULL);  // Ignore its return value

   /*
    * Restore the last error in the end.
    */
   if (retVal != GSLIBERR_SUCCESS) {
      Err_SetErrno(ctx.errNum);
#ifdef _WIN32
      errno = ctx.winErrNum;
#endif
   }

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Debug --
 *
 *      Stub for Debug.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Debug(const char *fmt, ...)
{
   WITH_ERRNO(__errNum__,
      {
         CallCtx *ctx = GuestStoreGetTls();

         if (ctx != NULL && ctx->logger != NULL) {
            va_list args;

            va_start(args, fmt);
            GuestStoreLogV(ctx, GSLIBLOGLEVEL_DEBUG, fmt, args);
            va_end(args);
         }
      }
   );
}


/*
 *-----------------------------------------------------------------------------
 *
 * Log --
 *
 *      Stub for Log.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Log(const char *fmt, ...)
{
   WITH_ERRNO(__errNum__,
      {
         CallCtx *ctx = GuestStoreGetTls();

         if (ctx != NULL && ctx->logger != NULL) {
            va_list args;

            va_start(args, fmt);
            GuestStoreLogV(ctx, GSLIBLOGLEVEL_INFO, fmt, args);
            va_end(args);
         }
      }
   );
}


/*
 *-----------------------------------------------------------------------------
 *
 * Warning --
 *
 *      Stub for Warning.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
Warning(const char *fmt, ...)
{
   WITH_ERRNO(__errNum__,
      {
         CallCtx *ctx = GuestStoreGetTls();

         if (ctx != NULL && ctx->logger != NULL) {
            va_list args;

            va_start(args, fmt);
            GuestStoreLogV(ctx, GSLIBLOGLEVEL_WARNING, fmt, args);
            va_end(args);
         }
      }
   );
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic --
 *
 *      Stub for Panic.
 *
 * Results:
 *      Does not return.
 *
 * Side-effects:
 *      Process exits.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...)
{
   va_list args;
   CallCtx *ctx;

   va_start(args, fmt);

   ctx = GuestStoreGetTls();
   if (ctx != NULL && ctx->panic != NULL) {
      char buf[1024] = GSLIBLOG_TAG;

      Str_Vsnprintf(buf + GSLIBLOG_TAG_LEN, sizeof(buf) - GSLIBLOG_TAG_LEN,
                    fmt, args);
      ctx->panic(buf, ctx->clientData);  // No return
   } else {
      fprintf(stderr, "Panic: " GSLIBLOG_TAG);
      vfprintf(stderr, fmt, args);
   }

   va_end(args);

   exit(-1);
}

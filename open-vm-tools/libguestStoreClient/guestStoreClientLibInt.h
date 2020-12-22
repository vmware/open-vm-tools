/*********************************************************
 * Copyright (c) 2019-2020 VMware, Inc. All rights reserved.
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
 *  guestStoreClientLibInt.h  --
 *    Private definitions for guestStoreClientLib.
 */

#ifndef __GUESTSTORECLIENTLIBINT_H__
#define __GUESTSTORECLIENTLIBINT_H__

#include "vm_assert.h"
#include "vm_atomic.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "err.h"
#include "str.h"
#include "util.h"
#include "posix.h"

#include "guestStoreConst.h"
#include "guestStoreDefs.h"
#include "vmware/tools/guestStoreClientLib.h"

#ifdef _WIN32

#define SYSERR_EADDRINUSE    WSAEADDRINUSE
#define SYSERR_EACCESS       WSAEACCES
#define SYSERR_EINTR         WSAEINTR
#define SYSERR_ECONNRESET    WSAECONNRESET
#define SYSERR_ECONNREFUSED  WSAECONNREFUSED

#define SHUTDOWN_RECV  SD_RECEIVE
#define SHUTDOWN_SEND  SD_SEND
#define SHUTDOWN_BOTH  SD_BOTH

typedef int socklen_t;

#else

#define SYSERR_EADDRINUSE    EADDRINUSE
#define SYSERR_EACCESS       EACCES
#define SYSERR_EINTR         EINTR
#define SYSERR_ECONNRESET    ECONNRESET
#define SYSERR_ECONNREFUSED  ECONNREFUSED

#define SHUTDOWN_RECV  SHUT_RD
#define SHUTDOWN_SEND  SHUT_WR
#define SHUTDOWN_BOTH  SHUT_RDWR

typedef int SOCKET;

#define SOCKET_ERROR    (-1)
#define INVALID_SOCKET  ((SOCKET) -1)

#endif


/*
 * Context of each GuestStore_GetContent call.
 */
typedef struct _CallCtx {
   const char *contentPath;  // Requested content path
   const char *outputPath;  // Output file path
   GuestStore_Logger logger;  // Caller provided logger function
   GuestStore_Panic panic;  // Caller provided panic function
   GuestStore_GetContentCallback getContentCb;  // Progress callback
   void *clientData;  // Parameter for caller provided functions
   FILE *output;  // Output file stream
   SOCKET sd;  // Socket descriptor connecting to vmtoolsd GuestStore plugin
   int64 contentSize;  // Total content bytes
   int64 contentBytesReceived;  // Received content bytes
   int bufSize;  // Content download buffer size
   char *buf;  // Content download buffer
   Err_Number errNum;  // Preserve the last error
#ifdef _WIN32
   int winErrNum;
   int winWSAErrNum;
#endif
} CallCtx;

/*
 * Preserve the first last error that fails API GuestStore_GetContent() and
 * restore it when the API returns in case API exit resource freeing calls
 * change the last error.
 *
 * WSASetLastError needs a successful WSAStartup call,
 * WSAGetLastError does not.
 */
#ifdef _WIN32
#define LOG_ERR(ctx, format, ...)                 \
   if ((ctx)->errNum == 0 &&                      \
       (ctx)->winErrNum == 0 &&                   \
       (ctx)->winWSAErrNum == 0) {                \
      (ctx)->errNum = Err_Errno();                \
      (ctx)->winErrNum = errno;                   \
      (ctx)->winWSAErrNum = WSAGetLastError();    \
   }                                              \
   if ((ctx)->logger != NULL) {                   \
       GuestStoreLog((ctx), GSLIBLOGLEVEL_ERROR,  \
                     format, ##__VA_ARGS__);      \
   }
#else
#define LOG_ERR(ctx, format, ...)                 \
   if ((ctx)->errNum == 0) {                      \
      (ctx)->errNum = Err_Errno();                \
   }                                              \
   if ((ctx)->logger != NULL) {                   \
       GuestStoreLog((ctx), GSLIBLOGLEVEL_ERROR,  \
                     format, ##__VA_ARGS__);      \
   }
#endif

#define LOG_WARN(ctx, format, ...)                  \
   if ((ctx)->logger != NULL) {                     \
       GuestStoreLog((ctx), GSLIBLOGLEVEL_WARNING,  \
                     format, ##__VA_ARGS__);        \
   }

#define LOG_INFO(ctx, format, ...)               \
   if ((ctx)->logger != NULL) {                  \
       GuestStoreLog((ctx), GSLIBLOGLEVEL_INFO,  \
                     format, ##__VA_ARGS__);     \
   }

#define LOG_DEBUG(ctx, format, ...)               \
   if ((ctx)->logger != NULL) {                   \
       GuestStoreLog((ctx), GSLIBLOGLEVEL_DEBUG,  \
                     format, ##__VA_ARGS__);      \
   }

#define REPORT_PROGRESS(ctx)                                         \
   (((ctx)->getContentCb != NULL) ? (ctx)->getContentCb(             \
                                       (ctx)->contentSize,           \
                                       (ctx)->contentBytesReceived,  \
                                       (ctx)->clientData)            \
                                    : TRUE)


/*
 *-----------------------------------------------------------------------------
 *
 * SocketGetLastError --
 *
 *      Get the last failed sock function error code.
 *
 * Results:
 *      error code.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline int
SocketGetLastError(void)
{
#ifdef _WIN32
   return WSAGetLastError();
#else
   return errno;
#endif
}


#ifdef __cplusplus
extern "C" {
#endif

void
GuestStoreLog(CallCtx *ctx,                 // IN
              GuestStoreLibLogLevel level,  // IN
              const char *fmt, ...);        // IN

GuestStoreLibError
GuestStoreConnect(CallCtx *ctx);  // IN / OUT

#ifdef __cplusplus
}
#endif

#endif /* __GUESTSTORECLIENTLIBINT_H__ */

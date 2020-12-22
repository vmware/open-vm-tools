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
 *  guestStoreClientLib.h  --
 *    Definitions for VMware Tools guestStore client library.
 */

#ifndef __GUESTSTORECLIENTLIB_H__
#define __GUESTSTORECLIENTLIB_H__

#include "vm_basic_types.h"

#define GUESTSTORE_LIB_ERR_LIST                                              \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_SUCCESS = 0,                             \
                           gsliberr.success,                                 \
                           "Success")                                        \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_GENERIC,                                 \
                           gsliberr.generic,                                 \
                           "Generic error")                                  \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_TLS,                                     \
                           gsliberr.tls,                                     \
                           "TLS error")                                      \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_NOT_INITIALIZED,                         \
                           gsliberr.not.initialized,                         \
                           "Not initialized")                                \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_INVALID_PARAMETER,                       \
                           gsliberr.invalid.parameter,                       \
                           "Invalid parameter")                              \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_NOT_ENOUGH_MEMORY,                       \
                           gsliberr.not.enough.memory,                       \
                           "Not enough memory")                              \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CREATE_OUTPUT_FILE,                      \
                           gsliberr.create.output.file,                      \
                           "Create output file error")                       \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_WRITE_OUTPUT_FILE,                       \
                           gsliberr.write.output.file,                       \
                           "Write output file error")                        \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONNECT_GENERIC,                         \
                           gsliberr.connect.generic,                         \
                           "Connect generic error")                          \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONNECT_SERVICE_NOT_RUNNING,             \
                           gsliberr.connect.service.not.running,             \
                           "Connect service not running")                    \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONNECT_PERMISSION_DENIED,               \
                           gsliberr.connect.permission.denied,               \
                           "Connect permission denied")                      \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONNECT_SECURITY_VIOLATION,              \
                           gsliberr.connect.security.violation,              \
                           "Connect security violation")                     \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONNECT_PEER_RESET,                      \
                           gsliberr.connect.peer.reset,                      \
                           "Connect peer reset")                             \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_SEND,                                    \
                           gsliberr.send,                                    \
                           "Send error")                                    \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_RECV,                                    \
                           gsliberr.recv,                                    \
                           "Receive error")                                  \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONTENT_FORBIDDEN,                       \
                           gsliberr.content.forbidden,                       \
                           "Content forbidden")                              \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CONTENT_NOT_FOUND,                       \
                           gsliberr.content.not.found,                       \
                           "Content not found")                              \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_SERVER,                                  \
                           gsliberr.server,                                  \
                           "Server error")                                   \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CANCELLED,                               \
                           gsliberr.cancelled,                               \
                           "Cancelled")                                      \
   GUESTSTORE_LIB_ERR_ITEM(GSLIBERR_CHECKSUM,                                \
                           gsliberr.checksum,                                \
                           "Checksum error")

/*
 * Error codes
 */
#define GUESTSTORE_LIB_ERR_ITEM(a, b, c) a,
typedef enum {
GUESTSTORE_LIB_ERR_LIST
GUESTSTORE_LIB_ERR_MAX
} GuestStoreLibError;
#undef GUESTSTORE_LIB_ERR_ITEM

/*
 * Log levels
 */
typedef enum {
   GSLIBLOGLEVEL_ERROR = 1,
   GSLIBLOGLEVEL_WARNING,
   GSLIBLOGLEVEL_INFO,
   GSLIBLOGLEVEL_DEBUG,
} GuestStoreLibLogLevel;


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Caller provided function to receive log messages from GuestStore client
 * library. Caller can log the messages to its own logging facilities.
 */
typedef void (*GuestStore_Logger) (GuestStoreLibLogLevel level,
                                   const char *message,
                                   void *clientData);

/*
 * Caller provided Panic function in non-recoverable error situations.
 * This function shall exit the library host process.
 */
typedef void (*GuestStore_Panic) (const char *message,
                                  void *clientData);

/*
 * Caller provided callback to get total content size in bytes and so far
 * received bytes. Return FALSE to cancel content download.
 */
typedef Bool (*GuestStore_GetContentCallback) (int64 contentSize,
                                               int64 contentBytesReceived,
                                               void *clientData);

/*
 * GuestStore client library Init entry point function.
 */
GuestStoreLibError
GuestStore_Init(void);

/*
 * GuestStore client library GetContent entry point function.
 */
GuestStoreLibError
GuestStore_GetContent(
   const char *contentPath,                     // IN
   const char *outputPath,                      // IN
   GuestStore_Logger logger,                    // IN, OPTIONAL
   GuestStore_Panic panic,                      // IN, OPTIONAL
   GuestStore_GetContentCallback getContentCb,  // IN, OPTIONAL
   void *clientData);                           // IN, OPTIONAL

/*
 * GuestStore client library DeInit entry point function.
 * Call of GuestStore_DeInit should match succeeded GuestStore_Init call.
 */
GuestStoreLibError
GuestStore_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __GUESTSTORECLIENTLIB_H__ */

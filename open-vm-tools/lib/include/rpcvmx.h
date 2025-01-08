/*********************************************************
 * Copyright (c) 2004-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * rpcvmx.h --
 *
 *      Simple utility library (usable by guest drivers as well as userlevel
 *      Tools code) that provides some useful VMX interaction capability, e.g.
 *      logging to the VM's VMX log, querying config variables, etc.
 *
 *      Thread safety:
 *          1. This library in general is *NOT* threadsafe.  Users of
 *             RpcVMX_Log/RpcVMX_LogV and RpcVMX_LogSetPrefix should protect
 *             this usage externally with their own locking, and any other
 *             requirements for thread safety should be carefully audited.
 *
 *          2. If thread safety around logging is required at a finer-grained
 *             level than a single external lock, callers should allocate
 *             buffers externally and then initialize them with
 *             RpcVMX_InitLogBackingBuffer. These buffers should be externally
 *             protected in some way such that they are only used by one thread
 *             at a time, and passed into RpcVMX_LogVWithBuffer at logging
 *             time.
 */

#ifndef _RPCVMX_H_
#define _RPCVMX_H_

#include <stdarg.h>

#include "vm_basic_types.h"
#include "rpcvmxext.h"

#define RPCVMX_DEFAULT_LOG_BUFSIZE  (2048 + sizeof "log")

typedef struct RpcVMXLogBuffer {
   char *       logBuf;
   unsigned int logBufSizeBytes;
   unsigned int logOffset;
} RpcVMXLogBuffer;

/*
 * Set a prefix to prepend to any future log statements.
 */
void RpcVMX_LogSetPrefix(const char *prefix);

/*
 * Get the currently set prefix (returns empty string if no prefix set)
 */
const char *RpcVMX_LogGetPrefix(const char *prefix);

/*
 * Initialize the given log buffer struct with the given caller-allocated
 * backing buffer and prefix string.
 */
Bool RpcVMX_InitLogBackingBuffer(RpcVMXLogBuffer *bufferOut,
                                 char *logBuf,
                                 unsigned int logBufSizeBytes,
                                 const char *prefix);

/*
 * Same as RpcVMX_Log but takes a va_list instead of inline arguments.
 */
void RpcVMX_LogV(const char *fmt,
                 va_list args);

/*
 * Same as RpcVMX_LogV but uses the caller-specified buffer to back the log.
 */
void RpcVMX_LogVWithBuffer(RpcVMXLogBuffer *rpcBuffer,
                           const char *fmt,
                           va_list args);

/*
 * Get the value of "guestinfo.$key" in the host VMX dictionary and return it.
 * Returns the default if the key is not set.
 */
char *RpcVMX_ConfigGetString(const char *defval,
                             const char *key);

/*
 * Same as _ConfigGetString, but convert the value to a 32-bit quantity.
 * XXX Returns 0, *NOT* the default, if the key was set but the value could
 *     not be converted to an int32.
 */
int32 RpcVMX_ConfigGetLong(int32 defval,
                           const char *key);

/*
 * Same as _ConfigGetString, but convert the value to a Bool. Returns the
 * default value if the key was set but could not be converted.
 */
Bool RpcVMX_ConfigGetBool(Bool defval,
                          const char *key);

#endif /* _VMXRPC_H_ */


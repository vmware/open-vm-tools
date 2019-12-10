/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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
 *      NB: This library is *NOT* threadsafe, so if you want to avoid
 *          corrupting your log statements or other screwups, add your own
 *          locking around calls to RpcVMX_Log.
 */

#ifndef _RPCVMX_H_
#define _RPCVMX_H_

#include <stdarg.h>

#include "vm_basic_types.h"
#include "rpcvmxext.h"

#define RPCVMX_MAX_LOG_LEN          (2048) /* 2kb max - make it dynamic? */

/*
 * Set a prefix to prepend to any future log statements.
 */
void RpcVMX_LogSetPrefix(const char *prefix);

/*
 * Get the currently set prefix (returns empty string if no prefix set)
 */
const char *RpcVMX_LogGetPrefix(const char *prefix);

/*
 * Save as RpcVMX_Log but takes a va_list instead of inline arguments.
 */
void RpcVMX_LogV(const char *fmt, va_list args);

/*
 * Get the value of "guestinfo.$key" in the host VMX dictionary and return it.
 * Returns the default if the key is not set.
 */
char *RpcVMX_ConfigGetString(const char *defval, const char *key);

/*
 * Same as _ConfigGetString, but convert the value to a 32-bit quantity.
 * XXX Returns 0, *NOT* the default, if the key was set but the value could
 *     not be converted to an int32.
 */
int32 RpcVMX_ConfigGetLong(int32 defval, const char *key);

/*
 * Same as _ConfigGetString, but convert the value to a Bool. Returns the
 * default value if the key was set but could not be converted.
 */
Bool RpcVMX_ConfigGetBool(Bool defval, const char *key);

#endif /* _VMXRPC_H_ */


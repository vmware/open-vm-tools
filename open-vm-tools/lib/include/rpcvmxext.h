/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * rpcvmxext.h --
 *
 *      Extension of the utility library (usable by guest drivers as well as userlevel
 *      Tools code) that provides some useful VMX interaction capability, e.g.
 *      logging to the VM's VMX log, querying config variables, etc.
 *
 *      NB: This library is *NOT* threadsafe, so if you want to avoid
 *          corrupting your log statements or other screwups, add your own
 *          locking around calls to RpcVMX_Log.
 */

#ifndef _RPCVMXEXT_H_
#define _RPCVMXEXT_H_

#include "vm_basic_types.h"

/*
 * Format the provided string with the provided arguments, and post it to the
 * VMX logfile via RPC.
 */
void RpcVMX_Log(const char *fmt, ...) PRINTF_DECL(1, 2);

/*
 * Report driver name and driver version to vmx to store the key-value in
 * GuestVars, and write a log in vmware.log using RpcVMX_Log.
 */
void RpcVMX_ReportDriverVersion(const char *drivername, const char *versionString);

#endif /* _RPCVMXEXT_H_ */


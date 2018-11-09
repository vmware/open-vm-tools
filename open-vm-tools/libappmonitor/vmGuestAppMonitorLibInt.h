/*********************************************************
 * Copyright (C) 2009-2016,2018 VMware, Inc. All rights reserved.
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

#ifndef _VM_GUEST_APP_MONITOR_LIB_INT_H_
#define _VM_GUEST_APP_MONITOR_LIB_INT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vmware.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Backdoor command
 */

#define VMGUESTAPPMONITOR_BD_CMD_ENABLE "GuestAppMonitor.Cmd.Enable"
#define VMGUESTAPPMONITOR_BD_CMD_DISABLE "GuestAppMonitor.Cmd.Disable"
#define VMGUESTAPPMONITOR_BD_CMD_IS_ENABLED "GuestAppMonitor.Cmd.IsEnabled"
#define VMGUESTAPPMONITOR_BD_CMD_MARK_ACTIVE "GuestAppMonitor.Cmd.MarkActive"
#define VMGUESTAPPMONITOR_BD_CMD_GET_APP_STATUS \
   "GuestAppMonitor.Cmd.GetAppStatus"
#define VMGUESTAPPMONITOR_BD_CMD_POST_APP_STATE \
   "GuestAppMonitor.Cmd.PostAppState"


#define VMGUESTAPPMONITOR_BD_RC_OK "0"
#define VMGUESTAPPMONITOR_BD_RC_INTERNAL_ERROR "1"
#define VMGUESTAPPMONITOR_BD_RC_NOT_ENABLED "2"
#define VMGUESTAPPMONITOR_BD_RC_ARGUMENT_EXPECTED "3"
#define VMGUESTAPPMONITOR_BD_RC_NULL_ARGUMENT "4"
#define VMGUESTAPPMONITOR_BD_RC_BAD_ARGUMENT "5"

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _VM_GUEST_APP_MONITOR_LIB_INT_H_ */

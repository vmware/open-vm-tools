/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * vmsupport.h --
 *
 *   Defines constants related to VM support data collection.
 *
 */

#ifndef _VMSUPPORT_H_
#define _VMSUPPORT_H_

/*
 * The status of vm-support tool running in the guest, exported in VMDB at:
 * vm/#/vmx/guestTools/vmsupport/gStatus
 * This enum must be kept in sync with vm-support script implementation.
 * The ordering in enum is important.
 */

typedef enum {
/* vm-support script is not running */
   VMSUPPORT_NOT_RUNNING = 0,
/* vm-support script is beginning */
   VMSUPPORT_BEGINNING = 1,
/* vm-support script running in progress */
   VMSUPPORT_RUNNING = 2,
/* vm-support script is ending */
   VMSUPPORT_ENDING = 3,
/* vm-support script failed */
   VMSUPPORT_ERROR = 10,
/* vm-support collection not supported */
   VMSUPPORT_UNKNOWN = 100
} VMSupportStatus;

/*
 * The command for vm-support tool launched in the guest, set in VMDB at:
 * vm/#/vmx/guestTools/vmsupport/hgCmd
 */

typedef enum {
  VMSUPPORT_CMD_RESET = 0,
  VMSUPPORT_CMD_SET = 1
} VMSupportCmd;

#define RPC_VMSUPPORT_START   "vmsupport.start"
#define RPC_VMSUPPORT_STATUS  "tools.vmsupport.status"

#endif // _VMSUPPORT_H_

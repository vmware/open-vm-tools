/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * statelogger_backdoor_def.h --
 *
 *      Backdoor command definitions for record/replay.
 */

#ifndef _STATELOGGER_BACKDOOR_DEF_H_
#define _STATELOGGER_BACKDOOR_DEF_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#define STATELOGGER_BKDR_VM_REPLAYABLE             0
#define STATELOGGER_BKDR_START_LOGGING             1
#define STATELOGGER_BKDR_STOP_LOGGING              2
#define STATELOGGER_BKDR_LAST_SNAPSHOT_UID         3
#define STATELOGGER_BKDR_GET_BRANCH_COUNT          4
#define STATELOGGER_BKDR_START_REPLAYING         100

#endif /* _STATELOGGER_BACKDOOR_DEF_H_ */

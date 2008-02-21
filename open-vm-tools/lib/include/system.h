/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * system.h --
 *
 *    System-specific routines used by the tools daemon.
 *
 */


#ifndef __SYSTEM_H__
#   define __SYSTEM_H__

#include "vm_basic_types.h"


uint64 System_Uptime(void);
Bool System_GetCurrentTime(int64 *secs, int64 *usecs);
Bool System_AddToCurrentTime(int64 deltaSecs, int64 deltaUsecs);
Bool System_IsACPI(void);
void System_Shutdown(Bool reboot);
Bool System_IsUserAdmin(void);

char *System_GetEnv(Bool global, char *valueName);
int System_SetEnv(Bool global, char *valueName, char *value);

#ifdef _WIN32
typedef enum {OS_WIN95 = 1, 
              OS_WIN98 = 2, 
              OS_WINME = 3, 
              OS_WINNT = 4, 
              OS_WIN2K = 5, 
              OS_WINXP = 6, 
              OS_WIN2K3 = 7,
              OS_VISTA  = 8,
              OS_UNKNOWN = 9} OS_TYPE;

typedef enum {OS_DETAIL_WIN95           = 1,
              OS_DETAIL_WIN98           = 2,
              OS_DETAIL_WINME           = 3,
              OS_DETAIL_WINNT           = 4,
              OS_DETAIL_WIN2K           = 5,
              OS_DETAIL_WIN2K_PRO       = 6,
              OS_DETAIL_WIN2K_SERV      = 7,
              OS_DETAIL_WIN2K_ADV_SERV  = 8,
              OS_DETAIL_WINXP           = 9,
              OS_DETAIL_WINXP_HOME      = 10,
              OS_DETAIL_WINXP_PRO       = 11,
              OS_DETAIL_WIN2K3          = 12,
              OS_DETAIL_WIN2K3_WEB      = 13,
              OS_DETAIL_WIN2K3_ST       = 14,
              OS_DETAIL_WIN2K3_EN       = 15,
              OS_DETAIL_WIN2K3_BUS      = 16,
              OS_DETAIL_VISTA           = 17,
              OS_DETAIL_UNKNOWN         = 18} OS_DETAIL_TYPE;

BOOL System_SetProcessPrivilege(char *lpszPrivilege, Bool bEnablePrivilege);
OS_TYPE System_GetOSType(void);
OS_DETAIL_TYPE System_GetOSDetailType(void);
int32 System_GetSPVersion(void);
#endif


#endif /* __SYSTEM_H__ */

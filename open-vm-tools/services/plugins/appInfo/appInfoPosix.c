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

/*
 * appInfoPosix.c --
 *
 *      Functions to capture the information about running applications inside
 *      a Linux guest.
 *
 */

#ifndef __linux__
#   error This file should not be compiled.
#endif

#include "appInfoInt.h"
#include "util.h"


/*
 *----------------------------------------------------------------------
 * AppInfo_GetAppInfo --
 *
 * Retrieves the application information for a specified process.
 *
 * @param[in] procInfo   Information about a process.
 *
 * @retval The application information. The caller must free the memory
 *         allocated for the application information.
 *         NULL if any error happens.
 *
 *----------------------------------------------------------------------
 */

AppInfo *
AppInfo_GetAppInfo(ProcMgrProcInfo *procInfo)        // IN
{
   AppInfo *appInfo = NULL;

   if (procInfo == NULL) {
      return appInfo;
   }

   if (procInfo->procCmdName == NULL) {
      return appInfo;
   }

   appInfo = Util_SafeMalloc(sizeof (*appInfo) * 1);
   appInfo->procId = procInfo->procId;
   appInfo->appName = Util_SafeStrdup(procInfo->procCmdName);
   appInfo->version = Util_SafeStrdup("");

   return appInfo;
}

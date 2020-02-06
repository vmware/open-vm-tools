/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appInfoInt.h"
#include "procMgr.h"
#include "vmware.h"
#include "str.h"
#include "util.h"


/*
 *----------------------------------------------------------------------
 * AppInfoGetAppInfo --
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
AppInfoGetAppInfo(ProcMgrProcInfo *procInfo)   // IN
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


/*
 *----------------------------------------------------------------------
 * AppInfo_GetAppList --
 *
 * Generates the application information list.
 *
 * @retval Pointer to the newly allocated application list. The caller must
 *         free the memory using AppInfoDestroyAppList function.
 *         NULL if any error occurs.
 *
 *----------------------------------------------------------------------
 */

GSList *
AppInfo_GetAppList(void) {
   GSList *appList = NULL;
   int i;
   ProcMgrProcInfoArray *procList = NULL;
   size_t procCount;

#if defined(_WIN32)
   procList = ProcMgr_ListProcessesEx();
#else
   procList = ProcMgr_ListProcesses();
#endif

   if (procList == NULL) {
      g_warning("Failed to get the list of processes.\n");
      return appList;
   }

   procCount = ProcMgrProcInfoArray_Count(procList);
   for (i = 0; i < procCount; i++) {
      AppInfo *appInfo;
      ProcMgrProcInfo *procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);
      appInfo = AppInfoGetAppInfo(procInfo);
      if (NULL != appInfo) {
         appList = g_slist_prepend(appList, appInfo);
      }
   }

   ProcMgr_FreeProcList(procList);

   return appList;
}

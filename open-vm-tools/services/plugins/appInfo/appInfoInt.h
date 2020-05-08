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

#ifndef _APPINFOINT_H_
#define _APPINFOINT_H_

/**
 * @file appInfoInt.h
 *
 * Header file with few functions that are internal to appInfo plugin.
 */

#define G_LOG_DOMAIN "appInfo"
#include "vm_basic_types.h"
#include <glib.h>
#include "vmware/tools/plugin.h"
#include "procMgr.h"

#if defined(_WIN32)

#include <windows.h>
typedef DWORD AppInfo_Pid;

#else /* POSIX */

#include <sys/types.h>
typedef pid_t AppInfo_Pid;

#endif

/*
 * Application information structure.
 * This holds basic information we return per process
 * when listing process information inside the guest.
 */

typedef struct AppInfo {
   AppInfo_Pid procId;
   char *appName;             // UTF-8
   char *version;
#if defined(_WIN32)
   size_t memoryUsed;
#endif
} AppInfo;

GSList *AppInfo_GetAppList(GKeyFile *config);
GSList *AppInfo_SortAppList(GSList *appList);

void AppInfo_DestroyAppList(GSList *appList);

#if defined(WIN32)
AppInfo *AppInfo_GetAppInfo(ProcMgrProcInfo *procInfo, Bool useWMI);
#else
AppInfo *AppInfo_GetAppInfo(ProcMgrProcInfo *procInfo);
#endif

#endif /* _APPINFOINT_H_ */

/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * WindowsUtil.h --
 *
 *    misc Windows utilities
 */

#ifndef WINDOWSUTIL_H_
#define WINDOWSUTIL_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_atomic.h"
#include "unicodeTypes.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WIN32

/* Defines */
#define VMX_SHUTDOWN_ORDER    0x100    // Application reserved last shutdown range.
#define UI_SHUTDOWN_ORDER     0x300    // Application reserved first shutdown range.
#define TOOLS_SHUTDOWN_ORDER  0x100    // Application reserved last shutdown range

#include <windows.h>

#if !defined(VM_WIN_UWP)
#include "win32Util.h"
#endif


/* Function declarations */

char * W32Util_GetInstallPath(void);
char * W32Util_GetInstallPath64(void);

/*
 * The string returned is allocated on the heap and must be freed by the
 * calling routine.
 */

char *W32Util_GetAppDataFilePath(const char *fileName);
char *W32Util_GetLocalAppDataFilePath(const char *fileName);

char *W32Util_GetCommonAppDataFilePath(const char *fileName);
char *W32Util_GetVmwareCommonAppDataFilePath(const char *fileName);

char *W32Util_GetMyDocumentPath(void);
char *W32Util_GetMyVideoPath(BOOL myDocumentsOnFail);

char *W32Util_GetDefaultVMPath(const char *pref);
char *W32Util_GetInstalledFilePath(const char *fileName);
char *W32Util_GetInstalledFilePath64(const char *fileName);

CRITICAL_SECTION *W32Util_GetSingletonCriticalSection(Atomic_Ptr *csMemory);

#endif // _WIN32

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // WIN32UTIL_H_

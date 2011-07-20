/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * guestApp.c --
 *
 *    Utility functions common to all guest applications
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmware.h"
#include "vm_version.h"
#include "vm_tools_version.h"
#include "guestApp.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "conf.h"
#include "rpcout.h"
#include "debug.h"
#include "strutil.h"
#include "str.h"
#include "msg.h"
#include "file.h"
#include "posix.h"
#include "vmware/guestrpc/tclodefs.h"

#ifdef _MSC_VER
#include <windows.h>
#include <shlobj.h>
#include "productState.h"
#include "winregistry.h"
#include "win32util.h"
#endif

/*
 * For Netware/Linux/BSD/Solaris, the install path
 * is the hardcoded value below. For Windows, it is
 * determined dynamically in GuestApp_GetInstallPath(),
 * so the empty string here is just for completeness.
 */

#if defined _WIN32
#   define GUESTAPP_TOOLS_INSTALL_PATH ""
#elif defined __APPLE__
#   define GUESTAPP_TOOLS_INSTALL_PATH "/Library/Application Support/VMware Tools"
#else
#   define GUESTAPP_TOOLS_INSTALL_PATH "/etc/vmware-tools"
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetDefaultScript --
 *
 *    Returns the default power script for the given configuration option.
 *
 * Results:
 *    Script name on success, NULL of the option is not recognized.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

const char *
GuestApp_GetDefaultScript(const char *confName) // IN
{
   const char *value = NULL;
   if (strcmp(confName, CONFNAME_SUSPENDSCRIPT) == 0) {
      value = CONFVAL_SUSPENDSCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_RESUMESCRIPT) == 0) {
      value = CONFVAL_RESUMESCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_POWEROFFSCRIPT) == 0) {
      value = CONFVAL_POWEROFFSCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_POWERONSCRIPT) == 0) {
      value = CONFVAL_POWERONSCRIPT_DEFAULT;
   }
   return value;
}

#ifdef _WIN32

/*
 *------------------------------------------------------------------------------
 *
 * GuestApp_GetInstallPathW --
 *
 *    Returns the tools installation path as a UTF-16 encoded string, or NULL on
 *    error. The caller must deallocate the returned string using free.
 *
 * Results:
 *    See above.
 *
 * Side effects:
 *    None.
 *------------------------------------------------------------------------------
 */

LPWSTR
GuestApp_GetInstallPathW(void)
{
   static LPCWSTR TOOLS_KEY_NAME = L"Software\\VMware, Inc.\\VMware Tools";
   static LPCWSTR INSTALLPATH_VALUE_NAME = L"InstallPath";

   HKEY   key    = NULL;
   LONG   rc     = ERROR_SUCCESS;
   DWORD  cbData = 0;
   DWORD  temp   = 0;
   PWCHAR data   = NULL;

   rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, TOOLS_KEY_NAME, 0, KEY_READ, &key);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't open key \"%S\".\n", __FUNCTION__, TOOLS_KEY_NAME);
      Debug("%s: RegOpenKeyExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   rc = RegQueryValueExW(key, INSTALLPATH_VALUE_NAME, 0, NULL, NULL, &cbData);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't get length of value \"%S\".\n", __FUNCTION__,
            INSTALLPATH_VALUE_NAME);
      Debug("%s: RegQueryValueExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   /*
    * The data in the registry may not be null terminated. So allocate enough
    * space for one extra WCHAR and use that space to write our own NULL.
    */
   data = (LPWSTR) malloc(cbData + sizeof(WCHAR));
   if (NULL == data) {
      Debug("%s: Couldn't allocate %d bytes.\n", __FUNCTION__, cbData);
      goto exit;
   }

   temp = cbData;
   rc = RegQueryValueExW(key, INSTALLPATH_VALUE_NAME, 0, NULL, (LPBYTE) data,
                         &temp);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't get data for value \"%S\".\n", __FUNCTION__,
            INSTALLPATH_VALUE_NAME);
      Debug("%s: RegQueryValueExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   data[cbData / sizeof(WCHAR)] = L'\0';

exit:
   if (NULL != key) {
      RegCloseKey(key);
   }

   return data;
}

#endif

/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetInstallPath --
 *
 *      Get the tools installation path. The caller is responsible for
 *      freeing the memory allocated for the path.
 *
 * Results:
 *      The path in UTF-8 if successful.
 *      NULL otherwise.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetInstallPath(void)
{
   char *pathUtf8 = NULL;

#if defined(_WIN32)
   size_t pathLen = 0;

   if (WinReg_GetSZ(HKEY_LOCAL_MACHINE,
                    CONF_VMWARE_TOOLS_REGKEY,
                    "InstallPath",
                    &pathUtf8) != ERROR_SUCCESS) {
      Warning("%s: Unable to retrieve install path: %s\n",
               __FUNCTION__, Msg_ErrString());
      return NULL;
   }

   /* Strip off the trailing backslash, if present */

   pathLen = strlen(pathUtf8);
   if (pathLen > 0) {
      if (pathUtf8[pathLen - 1] == '\\') {
         pathUtf8[pathLen - 1] = '\0';
      }
   }

#else
   pathUtf8 = Str_Asprintf(NULL, "%s", GUESTAPP_TOOLS_INSTALL_PATH);
#endif

   return pathUtf8;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetConfPath --
 *
 *      Get the path to the Tools configuration file.
 *
 *      The return conf path is a dynamically allocated UTF-8 encoded
 *      string that should be freed by the caller.
 *
 *      However, the function will also return NULL if we fail to create
 *      a "VMware/VMware Tools" directory. This can occur if we're not running
 *      as Administrator, which VMwareUser doesn't. But I believe that
 *      VMwareService will always come up before VMwareUser, so by the time
 *      a non-root user process calls this function, the directory exists.
 *
 * Results:
 *      The path in UTF-8, or NULL on failure.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetConfPath(void)
{
#if defined(_WIN32)
   char *path = W32Util_GetVmwareCommonAppDataFilePath(NULL);

   if (path != NULL) {
      char *tmp = Str_SafeAsprintf(NULL, "%s%c%s", path, DIRSEPC,
                                   ProductState_GetName());
      free(path);
      path = tmp;

      if (!File_EnsureDirectory(path)) {
         free(path);
         path = NULL;
      }
   }

   return path;
#else
    /* Just call into GuestApp_GetInstallPath. */
   return GuestApp_GetInstallPath();
#endif
}


/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * hostname.c --
 *
 *   Get the host name.
 */

#if defined(_WIN32)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <winsock.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "vmware.h"
#include "str.h"
#include "log.h"
#include "hostinfo.h"

#if defined(_WIN32)	// Windows
/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Return the fully qualified host name of the host.
 *
 *      Note: GetComputerNameExA is broken on multibyte encodings that
 *      use more than one byte per characters. The function calls
 *      GetComputerNameExW, then calls WideCharToMultiByte assuming, it
 *      seems, that the number of characters returned from 
 *      GetComputerNameW is the required buffer size for the multibyte
 *      conversion. So instead of calling GetComputerNameExA we will call
 *      the wide version and do the conversion ourselves.
 *
 *
 * Results:
 *      The host name on success; must be freed
 *      NULL if unable to determine the name
 *
 * Side effects:
 *      A host name resolution can occur
 *
 *----------------------------------------------------------------------
 */

const char *
Hostinfo_HostName(void)
{
   HMODULE        dllHandle;
   struct hostent *myHostEnt;
   struct hostent *(WINAPI *GetHostByNameFn)(char *hostName);
   int            (WINAPI *GetHostNameFn)(char *hostName, int size);
   BOOL           (WINAPI *GetComputerNameExWFn)(COMPUTER_NAME_FORMAT nameType,
                                                 LPWSTR lpBuffer,
                                                 LPDWORD lpnSize);
   char           hostName[1024] = { '\0' };
   wchar_t        wHostName[1024] = { L'\0' };
   DWORD          size = sizeof wHostName;

   dllHandle = GetModuleHandleA("kernel32");

   if (!dllHandle) {
      Warning("%s GetModuleHandle on kernel32 failed\n", __FUNCTION__);

      return NULL;
   }

   GetComputerNameExWFn = (void *) GetProcAddress(dllHandle, 
                                                  "GetComputerNameExW");

   if ((NULL != GetComputerNameExWFn) && 
       ((*GetComputerNameExWFn)(ComputerNamePhysicalDnsFullyQualified,
                                wHostName, &size) != 0)) {
      int bytesConverted;
      /*
       * The call to WideCharToMultiByte with the following parameters
       * might cause the system to drop/change some of the wide chars
       * in wHostName if an equivalent cannot be found in the locale.
       * However, this is the way GetComputerNameExA calls this 
       * function, and we are trying to mimic it.
       */
      bytesConverted = WideCharToMultiByte(CP_ACP, 0, wHostName, size + 1, 
                                           hostName, sizeof hostName, 
                                           NULL, NULL);
      if (bytesConverted > 0) {
         char *dupStr = NULL;
         dupStr = strdup(hostName);
         ASSERT_MEM_ALLOC(dupStr);
         return dupStr;
      } else {
         DWORD err = GetLastError();
         Warning("%s %s failed: %d\n", __FUNCTION__, "WideCharToMultiByte",
                 err); 
      }
   } else if (NULL != GetComputerNameExWFn) {
      DWORD err = GetLastError();
      Warning("%s %s failed: %d\n", __FUNCTION__, "GetComputerNameExW", err);
   }

   dllHandle = LoadLibraryA("ws2_32");

   if (!dllHandle) {
      Warning("%s Failed to load ws2_32, will try wsock32.\n", __FUNCTION__);

      dllHandle = LoadLibraryA("wsock32");

      if (!dllHandle) {
         Warning("%s Failed to wsock32.\n", __FUNCTION__);
         return NULL;
      }
   }

   GetHostNameFn = (void *) GetProcAddress(dllHandle, "gethostname");

   if (!GetHostNameFn) {
      Warning("%s Failed to find gethostname.\n", __FUNCTION__);
      FreeLibrary(dllHandle);
      return NULL;
   }

   if ((*GetHostNameFn)(hostName, sizeof hostName) == SOCKET_ERROR) {
      Warning("%s gethostname failed.\n", __FUNCTION__);
      FreeLibrary(dllHandle);
      return NULL;
   }

   GetHostByNameFn = (void *) GetProcAddress(dllHandle, "gethostbyname");

   if (!GetHostByNameFn) {
      Warning("%s Failed to find gethostbyname.\n", __FUNCTION__);
      FreeLibrary(dllHandle);
      return strdup(hostName);
   }

   myHostEnt = (*GetHostByNameFn)(hostName);

   FreeLibrary(dllHandle);

   if (myHostEnt == (struct hostent *) NULL) {
      Warning("%s gethostbyname failed.\n", __FUNCTION__);
   } else {
      Str_Strcpy(hostName, myHostEnt->h_name, sizeof hostName);
   }

   return strdup(hostName);
}
#elif defined(__APPLE__)	// MacOS X
#define SYS_NMLN _SYS_NAMELEN
#include <mach-o/dyld.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <unistd.h>
#include <sys/utsname.h>

/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Return the fully qualified host name of the host.
 *
 * Results:
 *      The host name on success; must be freed
 *      NULL on failure
 *
 * Side effects:
 *      A host name resolution can occur.
 *
 *-----------------------------------------------------------------------------
 */

const char *
Hostinfo_HostName(void)
{
   struct utsname un;

   char           *result = (char *) NULL;

   if ((uname(&un) == 0) && (*un.nodename != '\0')) {
      /* 'un.nodename' is already fully qualified. */
      result = strdup(un.nodename);
   }

   return result;
}
#elif defined(linux)		// Linux
#include <unistd.h>
#include <sys/utsname.h>
#include <netdb.h>

/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Return the fully qualified host name of the host.
 *
 * Results:
 *      The host name on success; must be freed
 *      NULL on failure
 *
 * Side effects:
 *      A host name resolution can occur.
 *
 *-----------------------------------------------------------------------------
 */

const char *
Hostinfo_HostName(void)
{
   struct utsname un;

   char           *result = (char *) NULL;

   if ((uname(&un) == 0) && (*un.nodename != '\0')) {
      struct hostent he;
      int            error;
      char           buffer[1024];

      struct hostent *phe = &he;

      /*
       * Fully qualify 'un.nodename'. If the name cannot be fully
       * qualified, use whatever unqualified name is available or bug
       * 139607 will occur.
       */

      result = un.nodename;

      if ((gethostbyname_r(result, &he, buffer, sizeof buffer,
					&phe, &error) == 0) && phe) {
         result = phe->h_name;
      }

      result = strdup(result);
   }

   return result;
}
#else			// Not any specifically coded OS
/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Return the fully qualified host name of the host.
 *
 * Results:
 *      The host name on success; must be freed
 *      NULL on failure
 *
 * Side effects:
 *       None
 *
 * Note:
 *	This is a dummy catcher for uncoded OSen.
 *
 *-----------------------------------------------------------------------------
 */

const char *
Hostinfo_HostName(void)
{
   char string[128];

   Str_Sprintf(string, sizeof string, "%s unimplemented for OS",
               __FUNCTION__);

   return strdup(string);
}
#endif

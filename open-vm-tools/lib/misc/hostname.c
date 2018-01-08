/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * hostname.c --
 *
 *   Get the host name.
 */

#if defined(_WIN32)

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
#include "windowsu.h"
#endif
#include "unicode.h"

#if defined(_WIN32)	// Windows
/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Return the fully qualified host name of the host.
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

char *
Hostinfo_HostName(void)
{
   char *result;
   HMODULE dllHandle;
   struct hostent *myHostEnt;
   struct hostent *(WINAPI *GetHostByNameFn)(char *hostName);
   int            (WINAPI *GetHostNameFn)(char *hostName, int size);

   char hostName[1024] = { '\0' };

   result = Win32U_GetComputerNameEx(ComputerNamePhysicalDnsFullyQualified);

   if (result != NULL) {
      return result;
   }

   Warning("%s GetComputerNameEx failed: %d\n", __FUNCTION__, GetLastError());

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

      return Unicode_Alloc(hostName, STRING_ENCODING_DEFAULT);
   }

   myHostEnt = (*GetHostByNameFn)(hostName);

   FreeLibrary(dllHandle);

   if (myHostEnt == (struct hostent *) NULL) {
      Warning("%s gethostbyname failed.\n", __FUNCTION__);
   } else {
      Str_Strcpy(hostName, myHostEnt->h_name, sizeof hostName);
   }

   return Unicode_Alloc(hostName, STRING_ENCODING_DEFAULT);
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

char *
Hostinfo_HostName(void)
{
   struct utsname un;

   char *result = NULL;

   if ((uname(&un) == 0) && (*un.nodename != '\0')) {
      /* 'un.nodename' is already fully qualified. */
      if (Unicode_IsStringValidUTF8(un.nodename)) {  // ASCII is OK
         result = Unicode_Duplicate(un.nodename);
      }
   }

   return result;
}
#elif defined(__linux__)
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

char *
Hostinfo_HostName(void)
{
   struct utsname un;

   char *result = NULL;

   if ((uname(&un) == 0) && (*un.nodename != '\0')) {
      char *p;
      int error;
      struct hostent he;
      char buffer[1024];

      struct hostent *phe = &he;

      /*
       * Fully qualify 'un.nodename'. If the name cannot be fully
       * qualified, use whatever unqualified name is available or bug
       * 139607 will occur.
       */

      p = un.nodename;

      if ((gethostbyname_r(p, &he, buffer, sizeof buffer, &phe,
                           &error) == 0) && (phe != NULL)) {
         p = phe->h_name;
      }

      if (Unicode_IsStringValidUTF8(p)) {  // ASCII is OK
         result = Unicode_Duplicate(p);
      }
   }

   return result;
}
#else			// Not any specifically coded OS
/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_HostName --
 *
 *      Stub for uncoded OSen
 *
 * Results:
 *      NULL
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_HostName(void)
{
   return NULL;
}
#endif

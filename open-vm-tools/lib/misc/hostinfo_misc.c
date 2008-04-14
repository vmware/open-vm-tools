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

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#include <winbase.h>
#else
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#endif

#include "vmware.h"
#include "hostinfo.h"
#include "safetime.h"
#include "str.h"

static Bool hostinfoOSVersionInitialized;

#if defined(_WIN32)
static int hostinfoOSVersion[4];
static DWORD hostinfoOSPlatform;
#else
#if defined(__APPLE__)
#define SYS_NMLN _SYS_NAMELEN
#endif
static int hostinfoOSVersion[3];
static char hostinfoOSVersionString[SYS_NMLN];
#endif


#if defined(_WIN32)
/*
 *----------------------------------------------------------------------
 *
 * HostinfoOSVersionInit --
 *
 *      Compute the OS version information
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      hostinfoOS* variables are filled in.
 *
 *----------------------------------------------------------------------
 */

void
HostinfoOSVersionInit(void)
{
   OSVERSIONINFO info;
   OSVERSIONINFOEX infoEx;

   if (hostinfoOSVersionInitialized) {
      return;
   }

   info.dwOSVersionInfoSize = sizeof (info);
   if (!GetVersionEx(&info)) {
      Warning("Unable to get OS version.\n");
      NOT_IMPLEMENTED();
   }
   ASSERT(ARRAYSIZE(hostinfoOSVersion) >= 4);
   hostinfoOSVersion[0] = info.dwMajorVersion;
   hostinfoOSVersion[1] = info.dwMinorVersion;
   hostinfoOSVersion[2] = info.dwBuildNumber & 0xffff;
   /*
    * Get the service pack number. We don't care much about NT4 hosts
    * so we can use OSVERSIONINFOEX without checking for Windows NT 4.0 SP6 
    * or later versions.
    */
   infoEx.dwOSVersionInfoSize = sizeof infoEx;
   if (GetVersionEx((OSVERSIONINFO*)&infoEx)) {
      hostinfoOSVersion[3] = infoEx.wServicePackMajor;
   }
   hostinfoOSPlatform = info.dwPlatformId;

   hostinfoOSVersionInitialized = TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSIsWinNT --
 *
 *      This is Windows NT or descendant.
 *
 * Results:
 *      TRUE if Windows NT or descendant.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Hostinfo_OSIsWinNT(void)
{
   HostinfoOSVersionInit();
   return hostinfoOSPlatform == VER_PLATFORM_WIN32_NT;
}
#else


/*
 *----------------------------------------------------------------------
 *
 * HostinfoOSVersionInit --
 *
 *      Compute the OS version information
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      hostinfoOS* variables are filled in.
 *
 *----------------------------------------------------------------------
 */

static void
HostinfoOSVersionInit(void)
{
   struct utsname u;
   char extra[SYS_NMLN] = "";

   if (hostinfoOSVersionInitialized) {
      return;
   }

   if (uname(&u) < 0) {
      Warning("%s unable to get host OS version (uname): %s\n",
	      __FUNCTION__, strerror(errno));
      NOT_IMPLEMENTED();
   }

   Str_Strcpy(hostinfoOSVersionString, u.release, SYS_NMLN);

   ASSERT(ARRAYSIZE(hostinfoOSVersion) >= 3);
   if (sscanf(u.release, "%d.%d.%d%s",
	      &hostinfoOSVersion[0], &hostinfoOSVersion[1],
	      &hostinfoOSVersion[2], extra) < 1) {
      Warning("%s unable to parse host OS version string: %s\n",
              __FUNCTION__, u.release);
      NOT_IMPLEMENTED();
   }

   hostinfoOSVersionInitialized = TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSVersionString --
 *
 *	Returns the host version information as returned in the
 *      release field of uname(2)
 *
 * Results:
 *	const char * - pointer to static buffer containing the release
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

const char *
Hostinfo_OSVersionString(void)
{
   HostinfoOSVersionInit();
   return hostinfoOSVersionString;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSVersion --
 *
 *      Host OS release info.
 *
 * Results:
 *      The i-th component of a dotted release string.
 *	0 if i is greater than the number of components we support.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Hostinfo_OSVersion(int i)
{
   HostinfoOSVersionInit();
   return i < ARRAYSIZE(hostinfoOSVersion) ? hostinfoOSVersion[i] : 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_GetTimeOfDay --
 *
 *      Return the current time of day according to the host.  We want
 *      UTC time (seconds since Jan 1, 1970).
 *
 * Results:
 *      Time of day in microseconds.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void 
Hostinfo_GetTimeOfDay(VmTimeType *time)
{
#if defined(_WIN32)
   struct _timeb t;

   _ftime(&t);

   *time = (t.time * 1000000) + t.millitm * 1000;
#else  // assume POSIX
   struct timeval tv;

   gettimeofday(&tv, NULL);

   *time = ((int64)tv.tv_sec * 1000000) + tv.tv_usec;
#endif
}

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
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <fcntl.h>
#if defined(sun)
#include <sys/systeminfo.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
#endif

#include "vmware.h"
#include "hostinfo.h"
#include "safetime.h"
#include "str.h"

#define SYSINFO_STRING_32       "i386"
#define SYSINFO_STRING_64       "amd64"
#define MAX_ARCH_NAME_LEN       sizeof SYSINFO_STRING_32 > sizeof SYSINFO_STRING_64 ? \
                                   sizeof SYSINFO_STRING_32 : \
                                   sizeof SYSINFO_STRING_64

static Bool hostinfoOSVersionInitialized;

#if defined(__APPLE__)
#define SYS_NMLN _SYS_NAMELEN
#endif
static int hostinfoOSVersion[3];
static char hostinfoOSVersionString[SYS_NMLN];


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
   struct timeval tv;

   gettimeofday(&tv, NULL);

   *time = ((int64)tv.tv_sec * 1000000) + tv.tv_usec;
}


/*
 *----------------------------------------------------------------------------
 *
 * Hostinfo_GetSystemBitness --
 *
 *      Determines the operating system's bitness.
 *
 * Return value:
 *      32 or 64 on success, negative value on failure. Check errno for more
 *      details of error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
Hostinfo_GetSystemBitness(void)
{
#if defined(linux)
   struct utsname u;

   if (uname(&u) < 0) {
      return -1;
   }
   if (strstr(u.machine, "x86_64")) {
      return 64;
   } else {
      return 32;
   }
#elif defined(N_PLAT_NLM)
   return 32;
#else
   char buf[MAX_ARCH_NAME_LEN] = { 0 };

#if defined(__FreeBSD__) || defined(__APPLE__)
   int mib[2];
   size_t len;

   len = sizeof buf;
   mib[0] = CTL_HW;
   mib[1] = HW_MACHINE;

   if (sysctl(mib, ARRAYSIZE(mib), buf, &len, NULL, 0) < 0) {
      return -1;
   }
#elif defined(sun)
#if !defined(SOL10)
   /*
    * XXX: This is bad.  We define SI_ARCHITECTURE_K to what it is on Solaris
    * 10 so that we can use a single guestd build for Solaris 9 and 10.  In the
    * future we should have the Solaris 9 build just return 32 -- since it did
    * not support 64-bit x86 -- and let the Solaris 10 headers define
    * SI_ARCHITECTURE_K, then have the installer symlink to the correct binary.
    * For now, though, we'll share a single build for both versions.
    */
#  define SI_ARCHITECTURE_K  518
# endif

   if (sysinfo(SI_ARCHITECTURE_K, buf, sizeof buf) < 0) {
      return -1;
   }

   if (strcmp(buf, SYSINFO_STRING_32) == 0) {
      return 32;
   } else if (strcmp(buf, SYSINFO_STRING_64) == 0) {
      return 64;
   }
#endif

   return -1;
#endif
}

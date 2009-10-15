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
 * hostinfoPosix.c --
 *
 *   Interface to host-specific information functions for Posix hosts
 *   
 *   I created this file for this only reason: the functions it contains should
 *   be callable by _any_ VMware userland program --hpreg
 *   
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <pwd.h>
#include <sys/resource.h>
#if defined(__APPLE__)
#define SYS_NMLN _SYS_NAMELEN
#include <assert.h>
#include <CoreServices/CoreServices.h>
#include <mach-o/dyld.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#if !defined(RLIMIT_AS)
#  if defined(RLIMIT_VMEM)
#     define RLIMIT_AS RLIMIT_VMEM
#  else
#     define RLIMIT_AS RLIMIT_RSS
#  endif
#endif
#else
#if !defined(USING_AUTOCONF) || defined(HAVE_SYS_VFS_H)
#include <sys/vfs.h>
#endif
#if !defined(sun) && (!defined(USING_AUTOCONF) || (defined(HAVE_SYS_IO_H) && defined(HAVE_SYS_SYSINFO_H)))
#include <sys/io.h>
#include <sys/sysinfo.h>
#ifndef HAVE_SYSINFO
#define HAVE_SYSINFO 1
#endif
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <paths.h>
#endif

#if !defined(_PATH_DEVNULL)
#define _PATH_DEVNULL "/dev/null"
#endif

#include "vmware.h"
#include "hostType.h"
#include "hostinfo.h"
#include "vm_version.h"
#include "str.h"
#include "msg.h"
#include "log.h"
#include "posix.h"
#include "file.h"
#include "backdoor_def.h"
#include "util.h"
#include "vmstdio.h"
#include "su.h"
#include "vm_atomic.h"
#include "x86cpuid.h"
#include "syncMutex.h"
#include "unicode.h"

#ifdef VMX86_SERVER
#include "uwvmkAPI.h"
#include "uwvmk.h"
#include "vmkSyscall.h"
#endif

#define LGPFX "HOSTINFO:"


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetModulePath --
 *
 *	Retrieve the full path to the executable. Not supported under VMvisor.
 *
 *      Note: If your process is running with elevated privileges
 *      (setuid/setgid), treat the path returned by this function as
 *      untrusted (for example, do not pass it to exec or open).
 *
 *      This function returns a path that is under the control of the
 *      user.  An attacker could manipulate the path returned by this
 *      function to elevate privileges.
 *
 * Results:
 *      On success: The allocated, NUL-terminated file path.
 *         Note: This path can be a symbolic or hard link; it's just one
 *         possible path to access the executable.
 *
 *      On failure: NULL.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Hostinfo_GetModulePath(uint32 priv)  // IN:
{
   Unicode path;

#if defined(__APPLE__)
   uint32_t pathSize = FILE_MAXPATH;
#else
   uid_t uid = -1;
#endif

   if ((priv != HGMP_PRIVILEGE) && (priv != HGMP_NO_PRIVILEGE)) {
      Warning("%s: invalid privilege parameter\n", __FUNCTION__);

      return NULL;
   }

#if defined(__APPLE__)
   path = Util_SafeMalloc(pathSize);
   if (_NSGetExecutablePath(path, &pathSize)) {
      Warning(LGPFX" %s: _NSGetExecutablePath failed.\n", __FUNCTION__);
      free(path);

      return NULL;
   }

#else
#if defined(VMX86_SERVER)
   if (HostType_OSIsVMK()) {
      return NULL;
   }
#endif

   // "/proc/self/exe" only exists on Linux 2.2+.
   ASSERT(Hostinfo_OSVersion(0) >= 2 && Hostinfo_OSVersion(1) >= 2);

   if (priv == HGMP_PRIVILEGE) {
      uid = Id_BeginSuperUser();
   }

   path = Posix_ReadLink("/proc/self/exe");

   if (priv == HGMP_PRIVILEGE) {
      Id_EndSuperUser(uid);
   }

   if (path == NULL) {
      Warning(LGPFX" %s: readlink failed: %s\n", __FUNCTION__,
              Err_ErrString());
   }
#endif

   return path;
}

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
#define MAX_LINE_LEN 128

/*
 * Global data
 */

// nothing


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_SystemTimerUS --
 *
 *      This is the routine to use when performing timing measurements. It
 *      is valid (finish-time - start-time) only within a single process.
 *      Don't send a time obtained this way to another process and expect
 *      a relative time measurement to be correct.
 *
 *      This timer is documented to never go backwards.
 *
 * Results:
 *      Relative time in microseconds or zero if a failure.
 *
 *      Please note that the actual resolution of this "clock" is undefined -
 *      it varies between OSen and OS versions.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

VmTimeType
Hostinfo_SystemTimerUS(void)
{
#if defined(__APPLE__)
   /*
    * On Mac OS a commpage timer is used. Such timers are ensured to never
    * go backwards so don't use the mutex technique - it's inefficient.
    */

   return Hostinfo_RawSystemTimerUS();
#else
   SyncMutex *lck;
   VmTimeType curTime;
   VmTimeType newTime;

   static Atomic_Ptr lckStorage;
   static VmTimeType lastTimeBase;
   static VmTimeType lastTimeRead;
   static VmTimeType lastTimeReset;

   /* Get and take lock. */
   lck = SyncMutex_CreateSingleton(&lckStorage);
   SyncMutex_Lock(lck);

   curTime = Hostinfo_RawSystemTimerUS();

   if (curTime == 0) {
      newTime = 0;
      goto exit;
   }

   /*
    * Don't let time be negative or go backward.  We do this by
    * tracking a base and moving foward from there.
    */

   newTime = lastTimeBase + (curTime - lastTimeReset);

   if (newTime < lastTimeRead) {
      lastTimeReset = curTime;
      lastTimeBase = lastTimeRead + 1;
      newTime = lastTimeBase + (curTime - lastTimeReset);
   }

   lastTimeRead = newTime;

exit:
   /* Release lock. */
   SyncMutex_Unlock(lck);

   return newTime;
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_SystemUpTime --
 *
 *      Return system uptime in microseconds.
 *
 *      Please note that the actual resolution of this "clock" is undefined -
 *      it varies between OSen and OS versions. Use Hostinfo_SystemTimerUS
 *      whenever possible.
 *
 * Results:
 *      System uptime in microseconds or zero in case of a failure.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

VmTimeType
Hostinfo_SystemUpTime(void)
{
#if defined(__APPLE__)
   return Hostinfo_RawSystemTimerUS();
#elif defined(VMX86_SERVER)
   uint64 uptime;
   VMK_ReturnStatus status;

   if (VmkSyscall_Init(FALSE, NULL, 0)) {
      status = CosVmnix_GetUptimeUS(&uptime);
      if (status == VMK_OK) {
         return uptime;
      }
   }

   return 0;
#elif defined(__linux__)
   int res;
   double uptime;
   int fd;
   char buf[256];

   static Atomic_Int fdStorage = { -1 };
   static Atomic_uint32 logFailedPread = { 1 };

   fd = Atomic_ReadInt(&fdStorage);

   /* Do we need to open the file the first time through? */
   if (UNLIKELY(fd == -1)) {
      fd = open("/proc/uptime", O_RDONLY);

      if (fd == -1) {
         Warning(LGPFX" Failed to open /proc/uptime: %s\n", Msg_ErrString());
         return 0;
      }

      /* Try to swap ours in. If we lose the race, close our fd */
      if (Atomic_ReadIfEqualWriteInt(&fdStorage, -1, fd) != -1) {
         close(fd);
      }

      /* Get the winning fd - either ours or theirs, doesn't matter anymore */
      fd = Atomic_ReadInt(&fdStorage);
   }

   ASSERT(fd != -1);

   res = pread(fd, buf, sizeof buf - 1, 0);
   if (res == -1) {
      /*
       * In case some kernel broke pread (like 2.6.28-rc1), have a fall-back
       * instead of spewing the log.  This should be rare.  Using a lock
       * around lseek and read does not work here as it will deadlock with
       * allocTrack/fileTrack enabled.
       */

      if (Atomic_ReadIfEqualWrite(&logFailedPread, 1, 0) == 1) {
         Warning(LGPFX" Failed to pread /proc/uptime: %s\n", Msg_ErrString());
      }
      fd = open("/proc/uptime", O_RDONLY);
      if (fd == -1) {
         Warning(LGPFX" Failed to retry open /proc/uptime: %s\n",
                 Msg_ErrString());
         return 0;
      }
      res = read(fd, buf, sizeof buf - 1);
      close(fd);
      if (res == -1) {
         Warning(LGPFX" Failed to read /proc/uptime: %s\n", Msg_ErrString());
         return 0;
      }
   }
   ASSERT(res < sizeof buf);
   buf[res] = '\0';

   if (sscanf(buf, "%lf", &uptime) != 1) {
      Warning(LGPFX" Failed to parse /proc/uptime\n");
      return 0;
   }

   return uptime * 1000 * 1000;
#else
NOT_IMPLEMENTED();
#endif
}


#if !defined(__APPLE__)
/*
 *----------------------------------------------------------------------
 *
 * HostinfoFindEntry --
 *
 *      Search a buffer for a pair `STRING <blanks> DIGITS'
 *	and return the number DIGITS, or 0 when fail.
 *
 * Results:
 *      TRUE on  success, FALSE on failure
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static Bool 
HostinfoFindEntry(char *buffer,         // IN: Buffer
                  char *string,         // IN: String sought
                  unsigned int *value)  // OUT: Value
{
   char *p = strstr(buffer, string);
   unsigned int val;

   if (p == NULL) {
      return FALSE;
   }

   p += strlen(string);

   while (*p == ' ' || *p == '\t') {
      p++;
   }
   if (*p < '0' || *p > '9') {
      return FALSE;
   }

   val = strtoul(p, NULL, 10);
   if (errno == ERANGE || errno == EINVAL) {
      return FALSE;
   }

   *value = val;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HostinfoGetMemInfo --
 *
 *      Get some attribute from /proc/meminfo
 *      Return value is in KB.
 *
 * Results:
 *      TRUE on success, FALSE on failure
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
HostinfoGetMemInfo(char *name,           // IN:
                   unsigned int *value)  // OUT:
{
   size_t len;
   char   buffer[4096];

   int fd = Posix_Open("/proc/meminfo", O_RDONLY);

   if (fd == -1) {
      Warning(LGPFX" %s: Unable to open /proc/meminfo\n", __FUNCTION__);
      return FALSE;
   }

   len = read(fd, buffer, sizeof buffer - 1);
   close(fd);

   if (len == -1) {
      return FALSE;
   }

   buffer[len] = '\0';

   return HostinfoFindEntry(buffer, name, value);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoSysinfo --
 *
 *      Retrieve system information on a Linux system.
 *    
 * Results:
 *      TRUE on success: '*totalRam', '*freeRam', '*totalSwap' and '*freeSwap'
 *                       are set if not NULL
 *      FALSE on failure
 *
 * Side effects:
 *      None.
 *
 *      This seems to be a very expensive call: like 5ms on 1GHz P3 running
 *      RH6.1 Linux 2.2.12-20.  Yes, that's 5 milliseconds.  So caller should
 *      take care.  -- edward
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HostinfoSysinfo(uint64 *totalRam,  // OUT: Total RAM in bytes
                uint64 *freeRam,   // OUT: Free RAM in bytes
                uint64 *totalSwap, // OUT: Total swap in bytes
                uint64 *freeSwap)  // OUT: Free swap in bytes
{
#ifdef HAVE_SYSINFO
   // Found in linux/include/kernel.h for a 2.5.6 kernel --hpreg
   struct vmware_sysinfo {
	   long uptime;			/* Seconds since boot */
	   unsigned long loads[3];	/* 1, 5, and 15 minute load averages */
	   unsigned long totalram;	/* Total usable main memory size */
	   unsigned long freeram;	/* Available memory size */
	   unsigned long sharedram;	/* Amount of shared memory */
	   unsigned long bufferram;	/* Memory used by buffers */
	   unsigned long totalswap;	/* Total swap space size */
	   unsigned long freeswap;	/* swap space still available */
	   unsigned short procs;	/* Number of current processes */
	   unsigned short pad;		/* explicit padding for m68k */
	   unsigned long totalhigh;	/* Total high memory size */
	   unsigned long freehigh;	/* Available high memory size */
	   unsigned int mem_unit;	/* Memory unit size in bytes */
	   // Padding: libc5 uses this..
	   char _f[20 - 2 * sizeof(long) - sizeof(int)];
   };
   struct vmware_sysinfo si;

   if (sysinfo((struct sysinfo *)&si) < 0) {
      return FALSE;
   }
   
   if (si.mem_unit == 0) {
      /*
       * Kernel versions < 2.3.23. Those kernels used a smaller sysinfo
       * structure, whose last meaningful field is 'procs' --hpreg
       */

      si.mem_unit = 1;
   }

   if (totalRam) {
      *totalRam = (uint64)si.totalram * si.mem_unit;
   }
   if (freeRam) {
      *freeRam = (uint64)si.freeram * si.mem_unit;
   }
   if (totalSwap) {
      *totalSwap = (uint64)si.totalswap * si.mem_unit;
   }
   if (freeSwap) {
      *freeSwap = (uint64)si.freeswap * si.mem_unit;
   }

   return TRUE;
#else // ifdef HAVE_SYSINFO
   NOT_IMPLEMENTED();
#endif // ifdef HAVE_SYSINFO
}
#endif // ifndef __APPLE__


#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoGetLinuxMemoryInfoInPages --
 *
 *      Obtain the minimum memory to be maintained, total memory available,
 *      and free memory available on the host (Linux or COS) in pages.
 *
 * Results:
 *      TRUE on success: '*minSize', '*maxSize' and '*currentSize' are set
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HostinfoGetLinuxMemoryInfoInPages(unsigned int *minSize,      // OUT:
                                  unsigned int *maxSize,      // OUT:
                                  unsigned int *currentSize)  // OUT:
{
   uint64 total; 
   uint64 free;
   unsigned int cached = 0;
   
   /*
    * Note that the free memory provided by linux does not include buffer and
    * cache memory. Linux tries to use the free memory to cache file. Most of
    * those memory can be freed immediately when free memory is low,
    * so for our purposes it should be counted as part of the free memory .
    * There is no good way to collect the useable free memory in 2.2 and 2.4
    * kernel.
    *
    * Here is our solution: The free memory we report includes cached memory.
    * Mmapped memory is reported as cached. The guest RAM memory, which is
    * mmaped to a ram file, therefore make up part of the cached memory. We
    * exclude the size of the guest RAM from the amount of free memory that we
    * report here. Since we don't know about the RAM size of other VMs, we
    * leave that to be done in serverd/MUI.
    */

   if (HostinfoSysinfo(&total, &free, NULL, NULL) == FALSE) {
      return FALSE;
   }

   /*
    * Convert to pages and round up total memory to the nearest multiple of 8
    * or 32 MB, since the "total" amount of memory reported by Linux is the
    * total physical memory - amount used by the kernel.
    */

   if (total < (uint64)128 * 1024 * 1024) {
      total = ROUNDUP(total, (uint64)8 * 1024 * 1024);
   } else {
      total = ROUNDUP(total, (uint64)32 * 1024 * 1024);
   }

   *minSize = 128; // XXX - Figure out this value
   *maxSize = total / PAGE_SIZE;

   HostinfoGetMemInfo("Cached:", &cached);
   if (currentSize) {
      *currentSize = free / PAGE_SIZE + cached / (PAGE_SIZE / 1024);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoGetSwapInfoInPages --
 *
 *      Obtain the total swap and free swap on the host (Linux or COS) in
 *      pages.
 *
 * Results:
 *      TRUE on success: '*totalSwap' and '*freeSwap' are set if not NULL
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetSwapInfoInPages(unsigned int *totalSwap,  // OUT:
                            unsigned int *freeSwap)   // OUT:
{
   uint64 total; 
   uint64 free;

   if (HostinfoSysinfo(NULL, NULL, &total, &free) == FALSE) {
      return FALSE;
   }

   if (totalSwap != NULL) {
      *totalSwap = total / PAGE_SIZE;
   }

   if (freeSwap != NULL) {
      *freeSwap = free / PAGE_SIZE;
   }

   return TRUE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetMemoryInfoInPages --
 *
 *      Obtain the minimum memory to be maintained, total memory available,
 *      and free memory available on the host in pages.
 *
 * Results:
 *      TRUE on success: '*minSize', '*maxSize' and '*currentSize' are set
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetMemoryInfoInPages(unsigned int *minSize,      // OUT:
                              unsigned int *maxSize,      // OUT:
                              unsigned int *currentSize)  // OUT:
{
#if defined(__APPLE__)
   mach_msg_type_number_t count;
   vm_statistics_data_t stat;
   kern_return_t error;
   uint64_t memsize;
   size_t memsizeSize = sizeof memsize;

   /*
    * Largely inspired by
    * darwinsource-10.4.5/top-15/libtop.c::libtop_p_vm_sample().
    */

   count = HOST_VM_INFO_COUNT;
   error = host_statistics(mach_host_self(), HOST_VM_INFO,
                           (host_info_t) &stat, &count);

   if (error != KERN_SUCCESS || count != HOST_VM_INFO_COUNT) {
      Warning("%s: Unable to retrieve host vm stats.\n", __FUNCTION__);
      return FALSE;
   }

   // XXX Figure out this value.
   *minSize = 128;

   /*
    * XXX Hopefully this includes cached memory as well. We should check.
    * No. It returns only completely used pages.
    */

   *currentSize = stat.free_count;

   /*
    * Adding up the stat values does not sum to 100% of physical memory.
    * The correct value is available from sysctl so we do that instead.
    */

   if (sysctlbyname("hw.memsize", &memsize, &memsizeSize, NULL, 0) == -1) {
      Warning("%s: Unable to retrieve host vm hw.memsize.\n", __FUNCTION__);
      return FALSE;
   }

   *maxSize = memsize / PAGE_SIZE;
   return TRUE;
#elif defined(VMX86_SERVER)
   uint64 total; 
   uint64 free;
   VMK_ReturnStatus status;

   if (VmkSyscall_Init(FALSE, NULL, 0)) {
      status = CosVmnix_GetMemSize(&total, &free);
      if (status == VMK_OK) {
         *minSize = 128;
         *maxSize = total / PAGE_SIZE;
         *currentSize = free / PAGE_SIZE;

         return TRUE;
      }
   }

   return FALSE;
#else
   return HostinfoGetLinuxMemoryInfoInPages(minSize, maxSize, currentSize);
#endif
}


#ifdef VMX86_SERVER
/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetCOSMemoryInfoInPages --
 *
 *      Obtain the minimum memory to be maintained, total memory available, and
 *      free memory available on the COS in pages.
 *
 * Results:
 *      TRUE on success: '*minSize', '*maxSize' and '*currentSize' are set
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetCOSMemoryInfoInPages(unsigned int *minSize,      // OUT:
                                 unsigned int *maxSize,      // OUT:
                                 unsigned int *currentSize)  // OUT:
{
   if (HostType_OSIsPureVMK()) {
      return FALSE;
   } else {
      return HostinfoGetLinuxMemoryInfoInPages(minSize, maxSize, currentSize);
   }
}
#endif


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
      Warning(LGPFX" %s: readlink failed: %s\n",
              __FUNCTION__, Err_ErrString());
   }
#endif

   return path;
}

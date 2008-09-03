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

#include <stdlib.h>
#include <stdio.h>
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
 * Local functions
 */

#if !defined(__APPLE__) && !defined(__FreeBSD__)
static char *HostinfoGetCpuInfo(int nCpu, char *name);
#if !defined(VMX86_SERVER)
static Bool HostinfoGetMemInfo(char *name, unsigned int *value);
#endif // ifndef VMX86_SERVER
#endif // ifndef __APPLE__


/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoGetLoadAverage --
 *
 *      Returns system average load.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HostinfoGetLoadAverage(float *avg0,  // IN/OUT
                       float *avg1,  // IN/OUT
                       float *avg2)  // IN/OUT
{
   /* getloadavg(3) was introduced with glibc 2.2 */
#if defined(GLIBC_VERSION_22) || defined(__APPLE__)
   double avg[3];
   int res;

   res = getloadavg(avg, 3);
   if (res < 3) {
      NOT_TESTED_ONCE();
      return FALSE;
   }

   if (avg0) {
      *avg0 = (float) avg[0];
   }
   if (avg1) {
      *avg1 = (float) avg[1];
   }
   if (avg2) {
      *avg2 = (float) avg[2];
   }
   return TRUE;
#else
   /* 
    * Not implemented. This function is currently only used in the vmx, so
    * getloadavg is always available to us. If the linux tools ever need this,
    * we can go back to having a look at the output of /proc/loadavg, but let's
    * no do that now as long as it's not necessary.
    */
   NOT_IMPLEMENTED();
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetLoadAverage --
 *
 *      Returns system average load * 100.
 *
 * Results:
 *      TRUE/FALSE
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetLoadAverage(uint32 *avg)      // IN/OUT
{
   float avg0;

   if (!HostinfoGetLoadAverage(&avg0, NULL, NULL)) {
      return FALSE;
   }
   *avg = (uint32) 100 * avg0;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_LogLoadAverage --
 *
 *      Logs system average load.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

void
Hostinfo_LogLoadAverage(void)
{
   float avg0, avg1, avg2;

   if (!HostinfoGetLoadAverage(&avg0, &avg1, &avg2)) {
      return;
   }
   Log("LOADAVG: %.2f %.2f %.2f\n", avg0, avg1, avg2);
}

#if defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoMacAbsTimeNS --
 *
 *      Return the Mac OS absolute time.
 *
 * Results:
 *      The absolute time in nanoseconds is returned. This time is documented
 *      to NEVER go backwards.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

static inline VmTimeType
HostinfoMacAbsTimeNS(void)
{
   VmTimeType raw;
   mach_timebase_info_data_t *ptr;
   static Atomic_Ptr atomic; /* Implicitly initialized to NULL. --mbellon */

   /* Insure that the time base values are correct. */
   ptr = (mach_timebase_info_data_t *) Atomic_ReadPtr(&atomic);

   if (ptr == NULL) {
      char *p;

      p = Util_SafeMalloc(sizeof(mach_timebase_info_data_t));

      mach_timebase_info((mach_timebase_info_data_t *) p);

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         free(p);
      }

      ptr = (mach_timebase_info_data_t *) Atomic_ReadPtr(&atomic);
   }

   raw = mach_absolute_time();

   if ((ptr->numer == 1) && (ptr->denom == 1)) {
      /* The scaling values are unity, save some time/arithmetic */
      return raw;
   } else {
      /* The scaling values are not unity. Prevent overflow when scaling */
      return ((double) raw) * (((double) ptr->numer) / ((double) ptr->denom));
   }
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoRawSystemTimerUS --
 *
 *      Obtain the raw system timer value.
 *
 * Results:
 *      Relative time in microseconds or zero if a failure.
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

VmTimeType
Hostinfo_RawSystemTimerUS(void)
{
#if defined(__APPLE__)
   return HostinfoMacAbsTimeNS() / 1000ULL;
#else
#if defined(VMX86_SERVER)
   if (HostType_OSIsPureVMK()) {
      uint64 uptime;
      VMK_ReturnStatus status;

      status = VMKernel_GetUptimeUS(&uptime);
      if (status != VMK_OK) {
         Log("%s: failure!\n", __FUNCTION__);
         return 0;  // A timer read failure - this is really bad!
      }

      return uptime;
   } else {
#endif /* ifdef VMX86_SERVER */
      struct timeval tval;

      /* Read the time from the operating system */
      if (gettimeofday(&tval, NULL) != 0) {
         Log("%s: failure!\n", __FUNCTION__);
         return 0;  // A timer read failure - this is really bad!
      }
      /* Convert into microseconds */
      return (((VmTimeType)tval.tv_sec) * 1000000 + tval.tv_usec);
#if defined(VMX86_SERVER)
   }
#endif /* ifdef VMX86_SERVER */
#endif /* ifdef __APPLE__ */
}


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
   return HostinfoMacAbsTimeNS() / 1000ULL;
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
      Warning(LGPFX" Failed to pread /proc/uptime: %s\n", Msg_ErrString());
      return 0;
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


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_NameGet --
 *
 *      Return the fully qualified host name of the host.
 *      Thread-safe. --hpreg
 *
 * Results:
 *      The (memorized) name on success
 *      NULL on failure
 *
 * Side effects:
 *      A host name resolution can occur.
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Hostinfo_NameGet(void)
{
   Unicode result;

   static Atomic_Ptr state; /* Implicitly initialized to NULL. --hpreg */

   result = Atomic_ReadPtr(&state);

   if (UNLIKELY(result == NULL)) {
      Unicode before;

      result = Hostinfo_HostName();

      before = Atomic_ReadIfEqualWritePtr(&state, NULL, result);

      if (before) {
         Unicode_Free(result);

         result = before;
      }
   }

   return result;
}


#ifdef VMX86_SERVER
/*
 *----------------------------------------------------------------------
 *
 * HostinfoReadProc --
 *
 *      Depending on what string is passed to it, this function parses the
 *      /proc/vmware/sched/ncpus node and returns the requested value.
 *
 * Results:
 *      A postive value on success, -1 (0xFFFFFFFF) on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint32
HostinfoReadProc(const char *str)
{
   /* XXX this should use sysinfo!! (bug 59849)
    */
   FILE *f;
   char *line;
   uint32 count;

   ASSERT(!strcmp("logical", str) || !strcmp("cores", str) || !strcmp("packages", str)); 

   ASSERT(!HostType_OSIsVMK()); // Don't use /proc/vmware

   f = Posix_Fopen("/proc/vmware/sched/ncpus", "r");
   if (f != NULL) {
      while (StdIO_ReadNextLine(f, &line, 0, NULL) == StdIO_Success) {
         if (strstr(line, str)) {
            if (sscanf(line, "%d ", &count) == 1) {
               free(line);
               break;
            }
         }
         free(line);
      }
      fclose(f);

      if (count > 0) {
         return count;
      }
   }

   return -1;
}

 
/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_HTDisabled --
 *
 *      Figure out if hyperthreading is enabled
 *
 * Results:
 *      TRUE if hyperthreading is disabled, FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Hostinfo_HTDisabled(void)
{
   static uint32 logical = 0, cores = 0;

   if (HostType_OSIsVMK()) {
      VMK_ReturnStatus status = VMKernel_HTEnabledCPU();
      if (status != VMK_OK) {
         return TRUE;
      } else {
         return FALSE;
      }
   }

   if (logical == 0 && cores == 0) {
      logical = HostinfoReadProc("logical");
      cores = HostinfoReadProc("cores");
      if (logical <= 0 || cores <= 0) {
         logical = cores = 0;
      }
   }

   return logical == cores;
}
#endif /*ifdef VMX86_SERVER*/


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_NumCPUs --
 *
 *      Get the number of logical CPUs on the host.  If the CPUs are
 *      hyperthread-capable, this number may be larger than the number of
 *      physical CPUs.  For example, if the host has four hyperthreaded
 *      physical CPUs with 2 logical CPUs apiece, this function returns 8.
 *
 *      This function returns the number of CPUs that the host presents to
 *      applications, which is what we want in the vast majority of cases.  We
 *      would only ever care about the number of physical CPUs for licensing
 *      purposes.
 *
 * Results:
 *      On success, the number of CPUs (> 0) the host tells us we have.
 *      On failure, 0xFFFFFFFF (-1).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

uint32
Hostinfo_NumCPUs(void)
{
#if defined(__APPLE__)
   uint32 out;
   size_t outSize = sizeof out;

   /*
    * Quoting sys/sysctl.h:
    * "
    * These are the support HW selectors for sysctlbyname.  Parameters that are
    * byte counts or frequencies are 64 bit numbers. All other parameters are
    * 32 bit numbers.
    * ...
    * hw.activecpu - The number of processors currently available for executing
    *                threads. Use this number to determine the number threads
    *                to create in SMP aware applications. This number can
    *                change when power management modes are changed.
    * "
    *
    * Apparently the only way to retrieve this info is by name, and I have
    * verified the info changes when you dynamically switch a CPU
    * offline/online. --hpreg
    */

   if (sysctlbyname("hw.activecpu", &out, &outSize, NULL, 0) == -1) {
      return -1;
   }

   return out;
#elif defined(__FreeBSD__)
   uint32 out;
   size_t outSize = sizeof out;

#if __FreeBSD__version >= 500019
   if (sysctlbyname("kern.smp.cpus", &out, &outSize, NULL, 0) == -1) {
      return -1;
   }
#else
   if (sysctlbyname("machdep.smp_cpus", &out, &outSize, NULL, 0) == -1) {
      if (errno == ENOENT) {
         out = 1;
      } else {
         return -1;
      }
   }
#endif

   return out;
#else
   static int count = 0;

   if (count <= 0) {
#ifdef VMX86_SERVER
      if (HostType_OSIsVMK()) {
         VMK_ReturnStatus status = VMKernel_GetNumCPUsUsed(&count);
         if (status != VMK_OK) {
            count = 0;
            return -1;
         }
      } else {
         count = HostinfoReadProc("logical");
         if (count <= 0) {
            count = 0;
            return -1;
         }
      }
#else /* ifdef VMX86_SERVER */
      FILE *f;
      char *line;

      f = Posix_Fopen("/proc/cpuinfo", "r");
      if (f == NULL) { 
	 Msg_Post(MSG_ERROR,
		  MSGID(hostlinux.opencpuinfo)
		  "Could not open /proc/cpuinfo.\n");
	 return -1;
      }
      
      while (StdIO_ReadNextLine(f, &line, 0, NULL) == StdIO_Success) {
	 if (strncmp(line, "processor", strlen("processor")) == 0) {
	    count++;
	 }
	 free(line);
      }
      
      fclose(f);
      
      if (count == 0) {
	 Msg_Post(MSG_ERROR,
		  MSGID(hostlinux.readcpuinfo)
		  "Could not determine the number of processors from "
		  "/proc/cpuinfo.\n");
	 return -1;
      }
#endif /* ifdef VMX86_SERVER */
   }

   return count;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetRatedCpuMhz --
 *
 *      Get the rated CPU speed of a given processor. 
 *      Return value is in MHz.
 *
 * Results:
 *      TRUE on success, FALSE on failure
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetRatedCpuMhz(int32 cpuNumber, // IN
                        uint32 *mHz)     // OUT
{
#if defined(__APPLE__) || defined(__FreeBSD__)

#  if defined(__APPLE__)
#     define CPUMHZ_SYSCTL_NAME "hw.cpufrequency_max"
#  elif __FreeBSD__version >= 50011
#     define CPUMHZ_SYSCTL_NAME "hw.clockrate"
#  endif

#  if defined(CPUMHZ_SYSCTL_NAME)
   uint32 hz;
   size_t hzSize = sizeof hz;

   // 'cpuNumber' is ignored: Intel Macs are always perfectly symetric.

   if (sysctlbyname(CPUMHZ_SYSCTL_NAME, &hz, &hzSize, NULL, 0) == -1) {
      return FALSE;
   }

   *mHz = hz / 1000000;
   return TRUE;
#  else
   return FALSE;
#  endif
#else
   float fMhz = 0;
   char *readVal = HostinfoGetCpuInfo(cpuNumber, "cpu MHz");
   
   if (readVal == NULL) {
      return FALSE;
   }
   
   if (sscanf(readVal, "%f", &fMhz) == 1) {
      *mHz = (unsigned int)(fMhz + 0.5);
   }
   free(readVal);
   return TRUE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetCpuDescription --
 *
 *      Get the descriptive name associated with a given CPU.
 *
 * Results:
 *      On success: Allocated, NUL-terminated string.
 *      On failure: NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_GetCpuDescription(uint32 cpuNumber) // IN
{
#if defined(__APPLE__) || defined(__FreeBSD__)
#  if defined(__APPLE__)
#     define CPUDESC_SYSCTL_NAME "machdep.cpu.brand_string"
#  else
#     define CPUDESC_SYSCTL_NAME "hw.model"
#  endif

   char *desc;
   size_t descSize;

   // 'cpuNumber' is ignored: Intel Macs are always perfectly symetric.

   if (sysctlbyname(CPUDESC_SYSCTL_NAME, NULL, &descSize, NULL, 0)
          == -1) {
      return NULL;
   }

   desc = malloc(descSize);
   if (!desc) {
      return NULL;
   }

   if (sysctlbyname(CPUDESC_SYSCTL_NAME, desc, &descSize, NULL, 0)
          == -1) {
      free(desc);
      return NULL;
   }

   return desc;
#else
#ifdef VMX86_SERVER
   if (HostType_OSIsVMK()) {
      char mName[48];
      if (VMKernel_GetCPUModelName(mName, cpuNumber, sizeof(mName)) == VMK_OK) {
	 mName[sizeof(mName) - 1] = '\0';
         return strdup(mName);
      }
      return NULL;
   }
#endif
   return HostinfoGetCpuInfo(cpuNumber, "model name");
#endif
}


#if !defined(__APPLE__)
#if !defined(__FreeBSD__)
/*
 *----------------------------------------------------------------------
 *
 * HostinfoGetCpuInfo --
 *
 *      Get some attribute from /proc/cpuinfo for a given CPU
 *
 * Results:
 *      On success: Allocated, NUL-terminated attribute string.
 *      On failure: NULL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
HostinfoGetCpuInfo(int nCpu,         // IN 
                   char *name)       // IN
{
   FILE *f;
   char *line;
   int cpu = 0;
   char *value = NULL;

   f = Posix_Fopen("/proc/cpuinfo", "r");
   if (f == NULL) { 
      Warning(LGPFX" HostinfoGetCpuInfo: Unable to open /proc/cpuinfo\n");
      return NULL;
   }
      
   while (cpu <= nCpu && 
          StdIO_ReadNextLine(f, &line, 0, NULL) == StdIO_Success) {
      char *s;
      char *e;

      if ((s = strstr(line, name)) &&
          (s = strchr(s, ':'))) {
         s++;
         e = s + strlen(s);

         /* Skip leading and trailing while spaces */
         for (; s < e && isspace(*s); s++);
         for (; s < e && isspace(e[-1]); e--);
         *e = 0;
         
         /* Free previous value */
         free(value);
         value = strdup(s);
         ASSERT_MEM_ALLOC(value);
         
         cpu++;
      }
      free(line);
   }     

   fclose(f);
   return value; 
}
#endif /* __FreeBSD__ */

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

Bool
HostinfoGetMemInfo(char *name,          //IN
                   unsigned int *value) //OUT
{
   size_t len;
   char   buffer[4096];

   int fd = Posix_Open("/proc/meminfo", O_RDONLY);

   if (fd == -1) {
      Warning(LGPFX" HostinfoGetMemInfo: Unable to open /proc/meminfo\n");
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
 *      TRUE on success: '*totalRam' and '*freeRam' are set if not NULL
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
HostinfoSysinfo(uint64 *totalRam, // OUT: Total RAM in bytes
                uint64 *freeRam)  // OUT: Free RAM in bytes
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
 *      Obtain the minimum memory to be maintained, total memory available, and
 *      free memory available on the host (Linux or COS) in pages.
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
HostinfoGetLinuxMemoryInfoInPages(unsigned int *minSize,     // OUT
                                  unsigned int *maxSize,     // OUT
                                  unsigned int *currentSize) // OUT
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

   if (HostinfoSysinfo(&total, &free) == FALSE) {
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
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetMemoryInfoInPages --
 *
 *      Obtain the minimum memory to be maintained, total memory available, and
 *      free memory available on the host in pages.
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
Hostinfo_GetMemoryInfoInPages(unsigned int *minSize,     // OUT
                              unsigned int *maxSize,     // OUT
                              unsigned int *currentSize) // OUT
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
   error = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&stat,
                           &count);
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
Hostinfo_GetCOSMemoryInfoInPages(unsigned int *minSize,     // OUT
                                 unsigned int *maxSize,     // OUT
                                 unsigned int *currentSize) // OUT
{
   if (HostType_OSIsPureVMK()) {
      return FALSE;
   } else {
      return HostinfoGetLinuxMemoryInfoInPages(minSize, maxSize, currentSize);
   }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_ResetProcessState --
 *
 *      Clean up signal handlers and file descriptors before an exec().
 *      Fds which need to be kept open can be passed as an array.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
Hostinfo_ResetProcessState(const int *keepFds, // IN:
                           size_t numKeepFds)  // IN:
{
   int s, fd;
   struct sigaction sa;
   struct rlimit rlim;
#ifdef __linux__
   int err;
   uid_t euid;
#endif

   /*
    * Disable itimers before resetting the signal handlers.
    * Otherwise, the process may still receive timer signals:
    * SIGALRM, SIGVTARLM, or SIGPROF.
    */
   struct itimerval it;
   it.it_value.tv_sec = it.it_value.tv_usec = 0;
   it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
   setitimer(ITIMER_REAL, &it, NULL);
   setitimer(ITIMER_VIRTUAL, &it, NULL);
   setitimer(ITIMER_PROF, &it, NULL);

   for (s = 1; s <= NSIG; s++) {
      sa.sa_handler = SIG_DFL;
      sigfillset(&sa.sa_mask);
      sa.sa_flags = SA_RESTART;
      sigaction(s, &sa, NULL);
   }
   for (fd = (int) sysconf(_SC_OPEN_MAX) - 1; fd > STDERR_FILENO; fd--) {
      size_t i;
      for (i = 0; i < numKeepFds; i++) {
         if (fd == keepFds[i]) {
            break;
         }
      }
      if (i == numKeepFds) {
         (void) close(fd);
      }
   }

   if (getrlimit(RLIMIT_AS, &rlim) == 0) {
      rlim.rlim_cur = rlim.rlim_max;
      setrlimit(RLIMIT_AS, &rlim);
   }

#ifdef __linux__
   /*
    * Drop iopl to its default value.
    */
   euid = Id_GetEUid();
   /* At this point, _unless we are running as root_, we shouldn't have root
      privileges --hpreg */
   ASSERT(euid != 0 || getuid() == 0);
   Id_SetEUid(0);
   err = iopl(0);
   Id_SetEUid(euid);
   ASSERT_NOT_IMPLEMENTED(err == 0);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_Execute --
 *
 *      Start program COMMAND.  If WAIT is TRUE, wait for program
 *	to complete and return exit status.
 *
 * Results:
 *      Exit status of COMMAND.
 *
 * Side effects:
 *      Run a separate program.
 *
 *----------------------------------------------------------------------
 */
int
Hostinfo_Execute(const char *command,
		 char * const *args,
		 Bool wait)
{
   int pid;
   int status;

   if (command == NULL) {
      return 1;
   }

   pid = fork();

   if (pid == -1) {
      return -1;
   }

   if (pid == 0) {
      Hostinfo_ResetProcessState(NULL, 0);
      Posix_Execvp(command, args);
      exit(127);
   }

   if (wait) {
      for (;;) {
	 if (waitpid(pid, &status, 0) == -1) {
	    if (errno == ECHILD) {
	       return 0;	// This sucks.  We really don't know.
	    }
	    if (errno != EINTR) {
	       return -1;
	    }
	 } else {
	    return status;
	 }
      }
   } else {
      return 0;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSIsSMP --
 *
 *      Host OS SMP capability.
 *
 * Results:
 *      TRUE is host OS is SMP capable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Hostinfo_OSIsSMP(void)
{
   uint32 ncpu;

#if defined(__APPLE__)
   size_t ncpuSize = sizeof ncpu;

   if (sysctlbyname("hw.ncpu", &ncpu, &ncpuSize, NULL, 0) == -1) {
      return FALSE;
   }

#else
   ncpu = Hostinfo_NumCPUs();

   if (ncpu == 0xFFFFFFFF) {
      return FALSE;
   }
#endif

   return ncpu > 1 ? TRUE : FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetModulePath --
 *
 *	Retrieve the full path to the executable. Not supported under VMvisor.
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
Hostinfo_GetModulePath(uint32 priv)
{
   Unicode path;

#if defined(__APPLE__)
   uint32_t pathSize = FILE_MAXPATH;
#else
   Bool isSuper = FALSE;
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
   if (HostType_OSIsPureVMK()) {
      return NULL;
   }
#endif

   // "/proc/self/exe" only exists on Linux 2.2+.
   ASSERT(Hostinfo_OSVersion(0) >= 2 && Hostinfo_OSVersion(1) >= 2);

   if (priv == HGMP_PRIVILEGE) {
      isSuper = IsSuperUser();
      SuperUser(TRUE);
   }

   path = Posix_ReadLink("/proc/self/exe");

   if (priv == HGMP_PRIVILEGE) {
      SuperUser(isSuper);
   }

   if (path == NULL) {
      Warning(LGPFX" %s: readlink failed: %s\n",
              __FUNCTION__, Err_ErrString());
   }
#endif

   return path;
}


/*
 *----------------------------------------------------------------------
 *
 *  Hostinfo_TouchBackDoor --
 *
 *      Access the backdoor. This is used to determine if we are 
 *      running in a VM or on a physical host. On a physical host
 *      this should generate a GP which we catch and thereby determine
 *      that we are not in a VM. However some OSes do not handle the
 *      GP correctly and the process continues running returning garbage.
 *      In this case we check the EBX register which should be 
 *      BDOOR_MAGIC if the IN was handled in a VM. Based on this we
 *      return either TRUE or FALSE.
 *
 * Results:
 *      TRUE if we succesfully accessed the backdoor, FALSE or segfault
 *      if not.
 *
 * Side effects:
 *	Exception if not in a VM. 
 *
 *----------------------------------------------------------------------
 */

Bool
Hostinfo_TouchBackDoor(void)
{
   /*
    * XXX: This can cause Apple's Crash Reporter to erroneously display
    * a crash, even though the process has caught the SIGILL and handled
    * it.
    *
    * It's also annoying in gdb, so we'll turn it off in devel builds.
    */
#if !defined(__APPLE__) && !defined(VMX86_DEVEL)
   uint32 eax;
   uint32 ebx;
   uint32 ecx;

   __asm__ __volatile__(
#   if defined __PIC__ && !vm_x86_64 // %ebx is reserved by the compiler.
      "xchgl %%ebx, %1" "\n\t"
      "inl %%dx, %%eax" "\n\t"
      "xchgl %%ebx, %1"
      : "=a" (eax),
        "=&rm" (ebx),
#   else
      "inl %%dx, %%eax"
      : "=a" (eax),
        "=b" (ebx),
#   endif
        "=c" (ecx)
      :	"0" (BDOOR_MAGIC),
        "1" (~BDOOR_MAGIC),
        "2" (BDOOR_CMD_GETVERSION),
        "d" (BDOOR_PORT)
   );
   if (ebx == BDOOR_MAGIC) {
      return TRUE;
   }
#endif
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetUser --
 *
 *      Return current user name, or NULL if can't tell.
 *      XXX Not thread-safe (somebody could do a setenv()). --hpreg
 *
 * Results:
 *      User name.  Must be free()d by caller.
 *
 * Side effects:
 *	No.
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Hostinfo_GetUser()
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw = &pw;
   Unicode env = NULL;
   Unicode name = NULL;

   if ((Posix_Getpwuid_r(getuid(), &pw, buffer, sizeof buffer, &ppw) == 0) &&
       (ppw != NULL)) {
      if (ppw->pw_name) {
         name = Unicode_Duplicate(ppw->pw_name);
      }
   }

   if (!name) {
      env = Posix_Getenv("USER");
      if (env) {
         name = Unicode_Duplicate(env);
      }
   }
   return name;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_LogMemUsage --
 *      Log system memory usage.
 *
 * Results:
 *      System memory usage is logged.
 *
 * Side effects:
 *      No.
 *
 *-----------------------------------------------------------------------------
 */

void
Hostinfo_LogMemUsage(void)
{
   int fd = Posix_Open("/proc/self/statm", O_RDONLY);

   if (fd != -1) {
      size_t len;
      char buf[64];

      len = read(fd, buf, sizeof buf);
      close(fd);

      if (len != -1) {
         int a[7] = { 0 };

         buf[len < sizeof buf ? len : sizeof buf - 1] = '\0';

         sscanf(buf, "%d %d %d %d %d %d %d",
                &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);

         Log("RUSAGE size=%d resident=%d share=%d trs=%d lrs=%d drs=%d dt=%d\n",
             a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
      }
   }
}

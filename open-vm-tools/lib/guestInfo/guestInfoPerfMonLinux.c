/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * This file gathers the virtual memory stats from Linux guest to be
 * passed on to the vmx.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "vmware.h"
#include "guestInfo.h"
#include "strutil.h"
#include "debug.h"

#ifndef NO_PROCPS
#include "vm_procps.h"
static void GuestInfoMonitorGetStat(GuestMemInfo *vmStats);
static Bool GuestInfoMonitorReadMeminfo(GuestMemInfo *vmStats);
#endif

#define LINUX_MEMINFO_FLAGS (MEMINFO_MEMTOTAL | MEMINFO_MEMFREE | MEMINFO_MEMBUFF |\
                             MEMINFO_MEMCACHE | MEMINFO_MEMACTIVE | MEMINFO_MEMINACTIVE |\
                             MEMINFO_SWAPINRATE | MEMINFO_SWAPOUTRATE |\
                             MEMINFO_IOINRATE | MEMINFO_IOOUTRATE)

/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_PerfMon --
 *
 *      Gather performance stats.
 *
 * Results:
 *      Gathered stats. Returns FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestInfo_PerfMon(GuestMemInfo *vmStats)   // OUT: filled vmstats
{
#ifndef NO_PROCPS
   ASSERT(vmStats);
   vmStats->flags = 0;
   GuestInfoMonitorGetStat(vmStats);
   if (GuestInfoMonitorReadMeminfo(vmStats)) {
      vmStats->flags |= LINUX_MEMINFO_FLAGS;
      return TRUE;
   }
#endif
   return FALSE;
}


#ifndef NO_PROCPS
/*
 *----------------------------------------------------------------------
 *
 * GuestInfoMonitorGetStat --
 *
 *      Calls getstat() to gather memory stats.
 *
 * Results:
 *      Gathered stats in vmStats.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoMonitorGetStat(GuestMemInfo *vmStats)   // OUT: filled vmstats
{
   uint32 hz = Hertz;
   uint32 dummy;
   jiff cpuUse[2];
   jiff cpuNic[2];
   jiff cpuSys[2];
   jiff cpuIdl[2];
   jiff cpuIow[2];
   jiff cpuXxx[2];
   jiff cpuYyy[2];
   jiff cpuZzz[2];
   jiff cpuTotal;
   jiff cpuHalf;
   unsigned long pageIn[2];
   unsigned long pageOut[2];
   unsigned long swapIn[2];
   unsigned long swapOut[2];
   unsigned int dummy2[2];
   unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;

   meminfo();
   getstat(cpuUse, cpuNic, cpuSys, cpuIdl, cpuIow, cpuXxx, cpuYyy, cpuZzz,
           pageIn, pageOut, swapIn, swapOut, dummy2, dummy2, &dummy, &dummy,
           &dummy, &dummy);

   cpuTotal = *cpuUse + *cpuNic + *cpuSys + *cpuXxx +
              *cpuYyy + *cpuIdl + *cpuIow + *cpuZzz;
   cpuHalf = cpuTotal / 2UL;

   vmStats->memFree = kb_main_free;
   vmStats->memBuff = kb_main_buffers;
   vmStats->memCache = kb_main_cached,
   vmStats->memInactive = kb_inactive;
   vmStats->memActive = kb_active;

   vmStats->swapInRate = (uint64)((*swapIn  * kb_per_page * hz + cpuHalf) / cpuTotal);
   vmStats->swapOutRate = (uint64)((*swapOut * kb_per_page * hz + cpuHalf) / cpuTotal);
   vmStats->ioInRate = (uint64)((*pageIn * kb_per_page * hz + cpuHalf) / cpuTotal);
   vmStats->ioOutRate = (uint64)((*pageOut * kb_per_page * hz + cpuHalf) / cpuTotal);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoMonitorReadMeminfo --
 *
 *      Reads /proc/meminfo to gather phsycial memory and huge page stats.
 *
 * Results:
 *      Read /proc/meminfo for total physical memory and huge pages info.
 *      Returns FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
GuestInfoMonitorReadMeminfo(GuestMemInfo *vmStats)   // OUT: filled vmstats
{
   char buf[512];
   uint64 value;
   FILE *fp;

   /* Get the total memory, and huge page info from /proc/meminfo. */
   fp = fopen("/proc/meminfo", "r");
   if (!fp) {
      Log("GuestInfoMonitorReadMeminfo: Error opening /proc/meminfo.\n");
      return FALSE;
   }

   while(!feof(fp)) {
      if (fscanf(fp, "%s %"FMT64"u", buf, &value) != 2) {
         continue;
      }
      if (StrUtil_StartsWith(buf, "MemTotal")) {
         vmStats->memTotal = value;
      }
      if (StrUtil_StartsWith(buf, "HugePages_Total")) {
         vmStats->hugePagesTotal = value;
         vmStats->flags |= MEMINFO_HUGEPAGESTOTAL;
      }
      if (StrUtil_StartsWith(buf, "HugePages_Free")) {
         vmStats->hugePagesFree = value;
         vmStats->flags |= MEMINFO_HUGEPAGESFREE;
      }
   }
   fclose(fp);
   return TRUE;
}
#endif

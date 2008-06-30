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
 * hostinfo.c --
 *
 *    Platform-independent code that calls into hostinfo<OS>-specific
 *    code.
 */

#include "vmware.h"
#include <string.h>

#include "cpuid_info.h"
#include "hostinfo.h"

#define LOGLEVEL_MODULE hostinfo
#include "loglevel_user.h"

#define LGPFX "HOSTINFO:"

static Bool
HostInfoGetIntelCPUCount(CPUIDSummary *cpuid,
                         uint32 *numCoresPerPCPU,
                         uint32 *numThreadsPerCore);

static Bool
HostInfoGetAMDCPUCount(CPUIDSummary *cpuid,
                       uint32 *numCoresPerPCPU,
                       uint32 *numThreadsPerCore);


/*
 *----------------------------------------------------------------------
 *
 * HostInfoGetIntelCPUCount --
 *
 *      For an Intel processor, determine the number of cores per physical
 *      CPU and the number of threads per core.
 *
 * Results:
 *
 *      TRUE if the information was successfully gathered, FALSE if
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostInfoGetIntelCPUCount(CPUIDSummary *cpuid,       // IN
                         uint32 *numCoresPerPCPU,   // OUT
                         uint32 *numThreadsPerCore) // OUT
{
   *numCoresPerPCPU = CPUIDSummary_IntelCoresPerPackage(cpuid,
                                                        numThreadsPerCore);
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HostInfoGetAMDCPUCount --
 *
 *      For an AMD processor, determine the number of cores per physical
 *      CPU and the number of threads per core.
 *
 * Results:
 *
 *      TRUE if the information was successfully gathered, FALSE if
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostInfoGetAMDCPUCount(CPUIDSummary *cpuid,       // IN
                       uint32 *numCoresPerPCPU,   // OUT
                       uint32 *numThreadsPerCore) // OUT
{
   *numCoresPerPCPU = CPUIDSummary_AMDCoresPerPackage(cpuid,
                                                      numThreadsPerCore);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetCpuid --
 *
 *       Get cpuid information for a CPU.  Which CPU the information is for
 *       depends on the OS scheduler. We are assuming that all CPUs in
 *       the system have identical numbers of cores and threads.
 *
 * Results:
 *       TRUE if the processor supports the cpuid instruction and this
 *       is a process we recognize, FALSE otherwise.
 *
 * Side effect:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Hostinfo_GetCpuid(HostinfoCpuIdInfo *info) // OUT
{
   CPUIDSummary cpuid;
   CPUIDRegs id0;
   uint32 numCoresPerPCPU, numThreadsPerCore;

   /*
    * Can't do cpuid = {0} as an initializer above because gcc throws
    * some idiotic warning.
    */

   memset(&cpuid, 0, sizeof(cpuid));

   /*
    * Get basic and extended CPUID info.
    */

   __GET_CPUID(0, &id0);

   cpuid.id0.numEntries = id0.eax;

   if (0 == cpuid.id0.numEntries) {
      Warning(LGPFX" No CPUID information available.\n");
      return FALSE;
   }

   *(uint32*)(cpuid.id0.name + 0)  = id0.ebx;
   *(uint32*)(cpuid.id0.name + 4)  = id0.edx;
   *(uint32*)(cpuid.id0.name + 8)  = id0.ecx;
   *(uint32*)(cpuid.id0.name + 12) = 0;

   __GET_CPUID(1,          (CPUIDRegs*)&cpuid.id1);
   __GET_CPUID(0xa,        (CPUIDRegs*)&cpuid.ida);
   __GET_CPUID(0x80000000, (CPUIDRegs*)&cpuid.id80);
   __GET_CPUID(0x80000001, (CPUIDRegs*)&cpuid.id81);
   __GET_CPUID(0x80000008, (CPUIDRegs*)&cpuid.id88);

   /*
    * Calculate vendor and CPU count information.
    */

   if (0 == strcmp(cpuid.id0.name, CPUID_INTEL_VENDOR_STRING_FIXED)) {
      info->vendor = CPUID_VENDOR_INTEL;
      if (!HostInfoGetIntelCPUCount(&cpuid, &numCoresPerPCPU,
                                    &numThreadsPerCore)) {
         Warning(LGPFX" Failed to get Intel CPU count.\n");
         return FALSE;
      }

      Log(LGPFX" Seeing Intel CPU, numCoresPerCPU %u numThreadsPerCore %u.\n",
          numCoresPerPCPU, numThreadsPerCore);
   } else if (strcmp(cpuid.id0.name, CPUID_AMD_VENDOR_STRING_FIXED) == 0) {
      info->vendor = CPUID_VENDOR_AMD;
      if (!HostInfoGetAMDCPUCount(&cpuid, &numCoresPerPCPU,
                                  &numThreadsPerCore)) {
         Warning(LGPFX" Failed to get AMD CPU count.\n");
         return FALSE;
      }

      Log(LGPFX" Seeing AMD CPU, numCoresPerCPU %u numThreadsPerCore %u.\n",
          numCoresPerPCPU, numThreadsPerCore);
   } else {
      info->vendor = CPUID_VENDOR_UNKNOWN;

      // assume one core per CPU, one thread per core
      numCoresPerPCPU = 1;
      numThreadsPerCore = 1;

      Log(LGPFX" Unknown CPU vendor \"%s\" seen, assuming one core per CPU "
          "and one thread per core.\n", cpuid.id0.name);
   }

   info->numLogCPUs = Hostinfo_NumCPUs();

   if (-1 == info->numLogCPUs) {
      Warning(LGPFX" Failed to get logical CPU count.\n");
      return FALSE;
   }

#ifdef VMX86_SERVER
   /* The host can avoid scheduling hypertwins, regardless of the CPU supporting it
    * or hyperthreading being enabled in the BIOS.  This leads to numThreadsPerCore 
    * set to 2 when it should be 1.
    */
   if (Hostinfo_HTDisabled()) {
      Log(LGPFX" hyperthreading disabled, setting number of threads per core to 1.\n");
      numThreadsPerCore = 1;
   } 
#endif

   info->numPhysCPUs = info->numLogCPUs / (numCoresPerPCPU *
                                           numThreadsPerCore);

   if (0 == info->numPhysCPUs) {
      // UP kernel case, possibly
      Log(LGPFX" numPhysCPUs is 0, bumping to 1.\n");
      info->numPhysCPUs = 1;
   }

   info->numCores = info->numLogCPUs / numThreadsPerCore;

   if (0 == info->numCores) {
      // UP kernel case, possibly
      Log(LGPFX" numCores is 0, bumping to 1.\n");
      info->numCores = 1;
   }

   Log(LGPFX" This machine has %u physical CPUS, %u total cores, and %u "
       "logical CPUs.\n", info->numPhysCPUs, info->numCores,
       info->numLogCPUs);

   /*
    * Pull out versioning and feature information.
    */

   info->version = cpuid.id1.version;
   info->family = CPUID_FAMILY(cpuid.id1.version);
   info->model = CPUID_MODEL(cpuid.id1.version);
   info->stepping = CPUID_STEPPING(cpuid.id1.version);
   info->type = (cpuid.id1.version >> 12) & 0x0003;

   info->extfeatures = cpuid.id1.ecxFeatures;
   info->features = cpuid.id1.edxFeatures;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 *  Hostinfo_TouchXen --
 *
 *      Check for Xen.
 *
 *      Official way is to check CPUID function 0x4000 0000, which
 *         returns a hypervisor string.  See PR156185,
 *         http://xenbits.xensource.com/xen-unstable.hg?file/6a383beedf83/tools/misc/xen-detect.c
 *      (Canonical way is /proc/xen, but CPUID is better).
 *
 * Results:
 *      TRUE if we are running in a Xen dom0 or domU.
 *      Linux:
 *         Illegal instruction exception on real hardware.
 *         Obscure Xen implementations might return FALSE.
 *      Windows:
 *         FALSE on real hardware.
 *
 * Side effects:
 *	Linux: Will raise exception on native hardware.
 *	Windows: None.
 *
 *----------------------------------------------------------------------
 */

Bool
Hostinfo_TouchXen(void)
{
#define XEN_CPUID 0x40000000
#define XEN_STRING "XenVMMXenVMM"
   CPUIDRegs regs;
   uint32 name[4];

   /* 
    * HVM mode: simple cpuid instr 
    * Xen hypervisor traps CPUID and adds information leaf(s)
    * at CPUID leaf XEN_CPUID and higher.
    */
   __GET_CPUID(XEN_CPUID, &regs);
   name[0] = regs.ebx;
   name[1] = regs.ecx;
   name[2] = regs.edx;
   name[3] = 0;
   if (0 == strcmp(XEN_STRING, (const char*)name)) {
      return TRUE;
   }

#ifdef linux
   /* 
    * PV mode: ud2a "xen" cpuid (faults on native hardware).
    * (Only Linux can run PV, so skip others here).
    * Since PV cannot trap CPUID, this is a Xen hook.
    */
   regs.eax = XEN_CPUID;
   __asm__ __volatile__(
      "xchgl %%ebx, %0"  "\n\t"
      "ud2a ; .ascii \"xen\" ; cpuid" "\n\t"
      "xchgl %%ebx, %0"
      : "=&r" (regs.ebx), "=&c" (regs.ecx), "=&d" (regs.edx)
      : "a" (regs.eax)
   );
   name[0] = regs.ebx;
   name[1] = regs.ecx;
   name[2] = regs.edx;
   name[3] = 0;
   if (0 == strcmp(XEN_STRING, (const char*)name)) {
      return TRUE;
   }

   /* Passed checks.  But native and anything non-Xen would #UD before here. */
   NOT_TESTED();
   Log("Xen detected but hypervisor unrecognized (Xen variant?)\n");
   Log("CPUID 0x4000 0000: eax=%x ebx=%x ecx=%x edx=%x\n", 
       regs.eax, regs.ebx, regs.ecx, regs.edx);
#endif

   return FALSE;
}

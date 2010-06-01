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

#if defined(__i386__) || defined(__x86_64__)
#include "cpuid_info.h"
#endif
#include "hostinfo.h"
#include "util.h"
#include "str.h"
#include "dynbuf.h"

#define LOGLEVEL_MODULE hostinfo
#include "loglevel_user.h"

#define LGPFX "HOSTINFO:"

#if defined(__i386__) || defined(__x86_64__)
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
 * HostInfoGetCpuidStrSection --
 *
 *       Append a section (either low or high) of CPUID as a string in DynBuf.
 *       E.g.
 *          00000000:00000005756E65476C65746E49656E69-
 *          00000001:00000F4A000208000000649DBFEBFBFF-
 *       or
 *          80000000:80000008000000000000000000000000-
 *          80000001:00000000000000000000000120100000-
 *          80000008:00003024000000000000000000000000-
 *
 *       The returned eax of args[0] is used to determine the upper bound for
 *       the following input arguments. And the input args should be in ascending
 *       order.
 *
 * Results:
 *       None. The string will be appended in buf.
 *
 * Side effect:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

static void
HostInfoGetCpuidStrSection(const uint32 args[],    // IN: input eax arguments
                           const size_t args_size, // IN: size of the argument array
                           DynBuf *buf)            // IN/OUT: result string in DynBuf
{
   static const char format[] = "%08X:%08X%08X%08X%08X-";
   CPUIDRegs reg;
   uint32 max_arg;
   char temp[64];
   int i;

   __GET_CPUID(args[0], &reg);
   max_arg = reg.eax;
   if (max_arg < args[0]) {
      Warning(LGPFX" No CPUID information available. Based = %08X.\n", args[0]);
      return;
   }
   DynBuf_Append(buf, temp,
      Str_Sprintf(temp, sizeof temp, format, args[0], reg.eax, reg.ebx, reg.ecx, reg.edx));

   for (i = 1; i < args_size && args[i] <= max_arg; i++) {
      ASSERT(args[i] > args[i - 1]); // Ascending order.
      __GET_CPUID(args[i], &reg);

      DynBuf_Append(buf, temp,
         Str_Sprintf(temp, sizeof temp, format, args[i], reg.eax, reg.ebx, reg.ecx, reg.edx));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetCpuidStr --
 *
 *       Get the basic and extended CPUID as a string. E.g.
 *          00000000:00000005756E65476C65746E49656E69-
 *          00000001:00000F4A000208000000649DBFEBFBFF-
 *          80000000:80000008000000000000000000000000-
 *          80000001:00000000000000000000000120100000-
 *          80000008:00003024000000000000000000000000
 *
 *       If the extended CPUID is not available, only returns the basic CPUID.
 *
 * Results:
 *       The CPUID string if the processor supports the CPUID instruction and this
 *       is a processor we recognize. It should never fail, since it would at least
 *       return leaf 0. Caller needs to free the returned string.
 *
 * Side effect:
 *       None
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_GetCpuidStr(void)
{
   static const uint32 basic_args[] = {0x0, 0x1, 0xa};
   static const uint32 extended_args[] = {0x80000000, 0x80000001, 0x80000008};
   DynBuf buf;
   char *result;

   DynBuf_Init(&buf);

   HostInfoGetCpuidStrSection(basic_args, ARRAYSIZE(basic_args), &buf);
   HostInfoGetCpuidStrSection(extended_args, ARRAYSIZE(extended_args), &buf);

   // Trim buffer and set NULL character to replace last '-'.
   DynBuf_Trim(&buf);
   result = (char*)DynBuf_Get(&buf);
   ASSERT(result && result[0]); // We should at least get result from eax = 0x0.
   result[DynBuf_GetSize(&buf) - 1] = '\0';

   return DynBuf_Detach(&buf);
}
#endif // defined(__i386__) || defined(__x86_64__)


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
#if defined(__i386__) || defined(__x86_64__)
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
#else // defined(__i386__) || defined(__x86_64__)
   return FALSE;
#endif // defined(__i386__) || defined(__x86_64__)
}


/*
 *----------------------------------------------------------------------
 *
 *  Hostinfo_HypervisorCPUIDSig --
 *
 *      Get the hypervisor signature string from CPUID.
 *
 * Results:
 *      Unqualified 16 byte nul-terminated hypervisor string
 *	String may contain garbage and caller must free
 *
 *      NULL: Hypervisor vendor signature string was not found
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

char *
Hostinfo_HypervisorCPUIDSig(void)
{
   uint32 *name = NULL;
#if defined(__i386__) || defined(__x86_64__)
   CPUIDRegs regs;

   __GET_CPUID(1, &regs);
   if (!(regs.ecx & CPUID_FEATURE_COMMON_ID1ECX_HYPERVISOR)) {
      return NULL;
   }

   regs.ebx = 0;
   regs.ecx = 0;
   regs.edx = 0;

   __GET_CPUID(0x40000000, &regs);

   if (regs.eax < 0x40000000) {
      Log(LGPFX" CPUID hypervisor bit is set, but no "
          "hypervisor vendor signature is present\n");
   }

   name = Util_SafeMalloc(4 * sizeof *name);

   name[0] = regs.ebx;
   name[1] = regs.ecx;
   name[2] = regs.edx;
   name[3] = 0;
#endif // defined(__i386__) || defined(__x86_64__)

   return (char *)name;
}


/*
 *----------------------------------------------------------------------
 *
 *  Hostinfo_TouchXen --
 *
 *      Check for Xen.
 *
 *      Official way is to call Hostinfo_HypervisorCPUIDSig(), which
 *         returns a hypervisor string.  This is a secondary check
 *	   that guards against a backdoor failure.  See PR156185,
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
#if defined(linux) && (defined(__i386__) || defined(__x86_64__))
#define XEN_CPUID 0x40000000
#define XEN_STRING "XenVMMXenVMM"
   CPUIDRegs regs;
   uint32 name[4];

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

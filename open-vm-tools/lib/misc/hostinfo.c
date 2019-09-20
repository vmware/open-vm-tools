/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
#include "x86cpuid_asm.h"
#endif
#include "hostinfo.h"
#include "hostinfoInt.h"
#include "str.h"
#include "util.h"
#include "str.h"
#include "dynbuf.h"
#include "backdoor_def.h"

#define LOGLEVEL_MODULE hostinfo
#include "loglevel_user.h"

#define LGPFX "HOSTINFO:"


/*
 * HostinfoOSData caches its returned value.
 */

volatile Bool hostinfoCacheValid = FALSE;
char          hostinfoCachedOSName[MAX_OS_NAME_LEN];
char          hostinfoCachedOSFullName[MAX_OS_FULLNAME_LEN];
char          hostinfoCachedDetailedData[MAX_DETAILED_STRING_LEN];


#if defined(__i386__) || defined(__x86_64__)
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
 *       the following input arguments. And the input args should be in
 *       ascending order.
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
      Warning(LGPFX" No CPUID information available. Based = %08X.\n",
              args[0]);
      return;
   }
   DynBuf_Append(buf, temp,
                 Str_Sprintf(temp, sizeof temp, format, args[0], reg.eax,
                             reg.ebx, reg.ecx, reg.edx));

   for (i = 1; i < args_size && args[i] <= max_arg; i++) {
      ASSERT(args[i] > args[i - 1]); // Ascending order.
      __GET_CPUID(args[i], &reg);

      DynBuf_Append(buf, temp,
                    Str_Sprintf(temp, sizeof temp, format, args[i], reg.eax,
                                reg.ebx, reg.ecx, reg.edx));
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
 *       The CPUID string if the processor supports the CPUID instruction and
 *       this is a processor we recognize. It should never fail, since it
 *       would at least return leaf 0. Caller needs to free the returned string.
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
   CPUIDRegs regs = {0};

   /* Get basic and extended CPUID info. */

   __GET_CPUID(0, &regs);
   if (regs.eax == 0) {
      Warning(LGPFX" No CPUID information available.\n");
      return FALSE;
   }

   /* Calculate vendor information. */

   if (memcmp(&regs.ebx, CPUID_INTEL_VENDOR_STRING, 12) == 0) {
      info->vendor = CPUID_VENDOR_INTEL;
   } else if (memcmp(&regs.ebx, CPUID_AMD_VENDOR_STRING, 12) == 0) {
      info->vendor = CPUID_VENDOR_AMD;
   } else if (memcmp(&regs.ebx, CPUID_HYGON_VENDOR_STRING, 12) == 0) {
      info->vendor = CPUID_VENDOR_HYGON;
   } else {
      info->vendor = CPUID_VENDOR_UNKNOWN;
   }

   /* Pull out versioning and feature information. */

   __GET_CPUID(1, &regs);
   info->version = regs.eax;
   info->family = CPUID_GET(1, EAX, FAMILY, regs.eax);
   info->model = CPUID_GET(1, EAX, MODEL, regs.eax);
   info->stepping = CPUID_GET(1, EAX, STEPPING, regs.eax);
   info->type = CPUID_GET(1, EAX, TYPE, regs.eax);
   info->extfeatures = regs.ecx;
   info->features = regs.edx;

   return TRUE;
#else // defined(__i386__) || defined(__x86_64__)
   return FALSE;
#endif // defined(__i386__) || defined(__x86_64__)
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetOSName --
 *
 *      Query the operating system and build a string to identify it.
 *
 *      Examples:
 *         Windows: <OS NAME> <SERVICE PACK> (Build <BUILD_NUMBER>)
 *         example: Windows XP Professional Service Pack 2 (Build 2600)
 *
 *         Linux:   <OS NAME> <OS RELEASE> <SPECIFIC_DISTRO_INFO>
 *         example: Linux 2.4.18-3 Red Hat Linux release 7.3 (Valhalla)
 *
 *         Mac OS:  <OS NAME> <OS VERSION> (<BUILD VERSION>) <KERNEL NAME> <KERNEL RELEASE>
 *         example: Mac OS X 10.8.5 (12F45) Darwin 12.5.0
 *
 * Return value:
 *      NULL  Unable to obtain the OS name.
 *     !NULL  The OS name. The caller is responsible for freeing it.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_GetOSName(void)
{
   char *name;
   Bool data = hostinfoCacheValid ? TRUE : HostinfoOSData();

   if (data) {
       name = Util_SafeStrdup(hostinfoCachedOSFullName);
   } else {
      name = NULL;
   }

   return name;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetOSGuestString --
 *
 *      Query the operating system and build a string to identify it. The
 *      returned string is the same as you'd see in a VM's .vmx file.
 *
 * Return value:
 *      NULL  Unable to obtain the OS name.
 *     !NULL  The OS name. The caller is responsible for freeing it.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_GetOSGuestString(void)
{
   char *name;
   Bool data = hostinfoCacheValid ? TRUE : HostinfoOSData();

   if (data) {
       name = Util_SafeStrdup(hostinfoCachedOSName);
   } else {
      name = NULL;
   }

   return name;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetOSDetailedData --
 *
 *      Query the operating system and build a property list containing
 *      detailed information about the guest OS.
 *
 * Return value:
 *      NULL  Unable to obtain the detailed data.
 *     !NULL  The detailed data string. The caller must free it.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

char *
Hostinfo_GetOSDetailedData(void)
{
   char *detailedData;
   Bool data = hostinfoCacheValid ? TRUE : HostinfoOSData();

   if (data) {
      detailedData = Util_SafeStrdup(hostinfoCachedDetailedData);
   } else {
      detailedData = NULL;
   }

   return detailedData;
}

/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * hostType.c --
 *
 *    Platform-independent code that calls into hostType<OS>-specific
 *    code to determine the host OS type.
 */

#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "hostType.h"
#include "str.h"

/*
 * XXX see bug 651592 for how to make this not warn on newer linux hosts
 * that have deprecated sysctl.
 */
#if defined(VMX86_SERVER) || ((defined(VMX86_VPX) || defined(VMX86_VMACORE)) && defined(linux))
#include <errno.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include "uwvmkAPI.h"
#define DO_REAL_HOST_CHECK
#endif

#define LGPFX "HOSTTYPE:"

/*
 *----------------------------------------------------------------------
 *
 * HostTypeOSVMKernelType --
 *
 *      Are we running on a flavor of VMKernel?  Only if the KERN_OSTYPE
 *      sysctl returns one of USERWORLD_SYSCTL_KERN_OSTYPE,
 *      USERWORLD_SYSCTL_VISOR_OSTYPE or USERWORLD_SYSCTL_VISOR64_OSTYPE
 *
 * Results:
 *      4 if running in a VMvisor UserWorld on the 64-bit vmkernel in ESX.
 *      3 if running directly in a UserWorld on the 64-bit vmkernel** in ESX.
 *      2 if running in a VMvisor UserWorld on the vmkernel in ESX.
 *      1 if running directly in a UserWorld on the vmkernel in ESX.
 *      0 if running on the COS or in a non-server product.
 *
 *      **Note that 64-bit vmkernel in ESX does not currently exist.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
HostTypeOSVMKernelType(void)
{
#ifdef DO_REAL_HOST_CHECK
   static int vmkernelType = -1;

   if (vmkernelType == -1) {
      char osname[128];
      size_t osnameLength;
      int kernOsTypeCtl[] = { CTL_KERN, KERN_OSTYPE };
      int rc;
      
      osnameLength = sizeof(osname);
      rc = sysctl(kernOsTypeCtl, ARRAYSIZE(kernOsTypeCtl),
                  osname, &osnameLength,
                  0, 0);
      if (rc == 0) {
         osnameLength = MAX(sizeof (osname), osnameLength);

         /*
          * XXX Yes, this is backwards in order of probability now, but we
          *     call it only once and anyway someday it won't be backwards ...
          */

         if (! strncmp(osname, USERWORLD_SYSCTL_VISOR64_OSTYPE,
                       osnameLength)) {
            vmkernelType = 4;
         } else if (! strncmp(osname, USERWORLD_SYSCTL_KERN64_OSTYPE,
                              osnameLength)) {
            vmkernelType = 3;
         } else if (! strncmp(osname, USERWORLD_SYSCTL_VISOR_OSTYPE,
                              osnameLength)) {
            vmkernelType = 2;
         } else if (! strncmp(osname, USERWORLD_SYSCTL_KERN_OSTYPE,
                              osnameLength)) {
            vmkernelType = 1;
         } else {
            vmkernelType = 0;
         }
      } else {
         /*
          * XXX too many of the callers don't define Warning.  See bug 125455
          */

         vmkernelType = 0;
      }
   }

   return (vmkernelType);
#else
   /* Non-linux builds are never running in a userworld */
   return 0;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HostType_OSIsVMK --
 *
 *      Are we running on the VMKernel (_any_ varient)?  True if KERN_OSTYPE
 *      sysctl returns _any_ of 
 *
 *          "UserWorld/VMKernel"
 *          "VMKernel"
 *          "UserWorld/VMKernel64"
 *          "VMKernel64"
 *
 * Results:
 *      TRUE if running in a UserWorld on the vmkernel in ESX.
 *      FALSE if running on the COS or in a non-server product.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostType_OSIsVMK(void)
{
   return (HostTypeOSVMKernelType() > 0);
}


/*
 *----------------------------------------------------------------------
 *
 * HostType_OSIsPureVMK --
 *
 *      Are we running on the VMvisor VMKernel (_any_ bitness)?  True if
 *      KERN_OSTYPE sysctl returns "VMKernel" or "VMKernel64".
 *
 * Results:
 *      TRUE if running in a VMvisor UserWorld on the vmkernel in ESX.
 *      FALSE if running on any other type of ESX or in a non-server product.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostType_OSIsPureVMK(void)
{
   return (HostTypeOSVMKernelType() == 2 || HostTypeOSVMKernelType() == 4);
}


/*
 *----------------------------------------------------------------------
 *
 * HostType_OSIsVMK64 --
 *
 *      Are we running on a 64-bit VMKernel?  Only if the KERN_OSTYPE
 *      sysctl returns "UserWorld/VMKernel64" or "VMKernel64".
 *
 * Results:
 *      TRUE if running in a UserWorld on a 64-bit vmkernel in ESX or VMvisor.
 *      FALSE if running on a 32-bit VMkernel in ESX or in a non-server product.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostType_OSIsVMK64(void)
{
   return (HostTypeOSVMKernelType() == 3 || HostTypeOSVMKernelType() == 4);
}

/*
 *----------------------------------------------------------------------
 *
 * HostType_OSIsSimulator --
 *
 *      Are we running on an ESX host simulator? Check presence of the
 *      mockup file.
 *
 * Results:
 *      TRUE if mockup file exists
 *      FALSE if the file doesn't exist
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostType_OSIsSimulator(void)
{
#ifdef DO_REAL_HOST_CHECK
   static int simulatorType = -1;
   if (simulatorType == -1) {
      if (access("/etc/vmware/hostd/mockupEsxHost.txt", 0) != -1) {
         simulatorType = 1;
      } else {
         simulatorType = 0;
      }
   }
   return (simulatorType == 1);
#else
   /* We are only interested in the case where VMX86_SERVER or friends are defined */
   return FALSE;
#endif
}

/*********************************************************
 * Copyright (C) 2006-2017 VMware, Inc. All rights reserved.
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
 *    Platform-independent code that determines the host OS type.
 */

#include <string.h>
#ifdef __linux__
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include "vmware.h"
#include "hostType.h"


/*
 *----------------------------------------------------------------------
 *
 * HostType_OSIsVMK --
 *
 *      Are we running on a flavor of VMKernel?
 *
 *      Per bug 651592 comment #3 and elsewhere:
 *      - all ESX/ESXi compilation defines __linux__ because vmkernel
 *        implements the int80 Linux syscall family.
 *      - ESXi 5.0 and later set utsname.sysname to "VMkernel".
 *      - ESXi 3.5, 4.0, and 4.1 have unverified behavior.
 *      - ESX Classic through 4.x set utsname.sysname to "Linux".
 *      This implementation of the code assumes Classic mode does not
 *      exist and ESXi is at least version 5, which covers all versions
 *      currently supported by this codebase and happens to be a much
 *      simpler matrix.
 *
 *      Should it ever become necessary to revive the check for Classic,
 *      checking for the existence of "/usr/lib/vmware/vmkernel" would
 *      suffice.
 *
 * Results:
 *      TRUE if running in a UserWorld on the vmkernel in ESX.
 *      FALSE if running in a non-ESX product.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HostType_OSIsVMK(void)
{
#if defined __linux__
   /*
    * Implementation note: it might make sense to short-circuit this whole
    * clause with "if (vmx86_server)" so that non-ESX builds get an even
    * cheaper test. I have chosen not to do so: some non-ESX code (e.g. fdm)
    * needs this check to act properly under a VMX86_VPX conditional. Saving
    * a couple of cycles is not worth the effort of maintaining a product-
    * specific macro in multi-product library code.
    */
   static enum {
      VMKTYPE_UNKNOWN = 0,  // to make default in .bss
      VMKTYPE_LINUX,
      VMKTYPE_VMKERNEL,
   } vmkernelType = VMKTYPE_UNKNOWN;

   if (vmkernelType == VMKTYPE_UNKNOWN) {
      struct utsname name;
      int rc;

      rc = uname(&name);
      if (rc == 0 && strcmp("VMkernel", name.sysname) == 0) {
         vmkernelType = VMKTYPE_VMKERNEL;
      } else {
         vmkernelType = VMKTYPE_LINUX;
      }
   }
   return vmkernelType == VMKTYPE_VMKERNEL;
#else
   /* Non-linux builds are never running in a userworld */
   return FALSE;
#endif
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
#if defined __linux__ && (defined VMX86_SERVER || defined VMX86_VPX)
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

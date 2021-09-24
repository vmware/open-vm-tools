/*********************************************************
 * Copyright (C) 2006-2021 VMware, Inc. All rights reserved.
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
 * vmcheck.c --
 *
 *    Utility functions for discovering our virtualization status.
 */

#include <stdlib.h>
#include <string.h>

#ifdef WINNT_DDK
#   include <ntddk.h>
#endif

#if (!defined(WINNT_DDK) && defined(_WIN32))
// include windows.h, otherwise DWORD type used in hostinfo.h is not defined
#  include "windows.h"
#endif

#include "vmware.h"
#include "vm_version.h"
#include "vm_tools_version.h"

#if !defined(WINNT_DDK)
#  include "hostinfo.h"
#  include "str.h"
#  include "x86cpuid.h"
#endif

/*
 * backdoor.h includes some files which redefine constants in ntddk.h.  Ignore
 * warnings about these redefinitions for WIN32 platform.
 */
#ifdef WINNT_DDK
#pragma warning (push)
// Warning: Conditional expression is constant.
#pragma warning( disable:4127 )
#endif

#include "backdoor.h"

#ifdef WINNT_DDK
#pragma warning (pop)
#endif

#include "backdoor_def.h"
#include "debug.h"

#if !defined(_WIN32)
#   include "vmsignal.h"
#   include "setjmp.h"
#endif

typedef Bool (*SafeCheckFn)(void);


#if !defined(_WIN32)
static sigjmp_buf jmpBuf;
static Bool       jmpIsSet;


/*
 *----------------------------------------------------------------------
 *
 * VmCheckSegvHandler --
 *
 *    Signal handler for segv. Return to the program state saved
 *    by a previous call to sigsetjmp, or Panic if sigsetjmp hasn't
 *    been called yet. This function never returns;
 *
 * Return Value:
 *    None.
 *
 * Side effects:
 *    See the manpage for sigsetjmp for details.
 *
 *----------------------------------------------------------------------
 */

static void
VmCheckSegvHandler(int clientData) // UNUSED
{
   if (jmpIsSet) {
      siglongjmp(jmpBuf, 1);
   } else {
      Panic("Received SEGV, exiting.");
   }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * VmCheckSafe --
 *
 *      Calls a potentially unsafe function, trapping possible exceptions.
 *
 * Results:
 *
 *      Return value of the passed function, or FALSE in case of exception.
 *
 * Side effects:
 *
 *      Temporarily suppresses signals / SEH exceptions
 *
 *----------------------------------------------------------------------
 */

static Bool
VmCheckSafe(SafeCheckFn checkFn)
{
   Bool result = FALSE;

   /*
    * On a real host this call should cause a GP and we catch
    * that and set result to FALSE.
    */

#if defined(_WIN32)
   __try {
      result = checkFn();
   } __except(EXCEPTION_EXECUTE_HANDLER) {
      /* no op */
   }
#else
   do {
      int signals[] = {
         SIGILL,
         SIGSEGV,
      };
      struct sigaction olds[ARRAYSIZE(signals)];

      if (Signal_SetGroupHandler(signals, olds, ARRAYSIZE(signals),
                                 VmCheckSegvHandler) == 0) {
         Warning("%s: Failed to set signal handlers.\n", __FUNCTION__);
         break;
      }

      if (sigsetjmp(jmpBuf, TRUE) == 0) {
         jmpIsSet = TRUE;
         result = checkFn();
      } else {
         jmpIsSet = FALSE;
      }

      if (Signal_ResetGroupHandler(signals, olds, ARRAYSIZE(signals)) == 0) {
         Warning("%s: Failed to reset signal handlers.\n", __FUNCTION__);
      }
   } while (0);
#endif

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VmCheck_GetVersion --
 *
 *    Retrieve the version of VMware that's running on the
 *    other side of the backdoor.
 *
 * Return value:
 *    TRUE on success
 *       *version contains the VMX version
 *       *type contains the VMX type
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
VmCheck_GetVersion(uint32 *version, // OUT
                    uint32 *type)    // OUT
{
   Backdoor_proto bp;

   ASSERT(version);
   ASSERT(type);

   /* Make sure EBX does not contain BDOOR_MAGIC */
   bp.in.size = (size_t)~BDOOR_MAGIC;
   /* Make sure ECX does not contain any known VMX type */
   bp.in.cx.halfs.high = 0xFFFF;

   bp.in.cx.halfs.low = BDOOR_CMD_GETVERSION;
   Backdoor(&bp);
   if (bp.out.ax.word == 0xFFFFFFFF) {
      /*
       * No backdoor device there. This code is not executing in a VMware
       * virtual machine. --hpreg
       */
      return FALSE;
   }

   if (bp.out.bx.word != BDOOR_MAGIC) {
      return FALSE;
   }

   *version = bp.out.ax.word;

   /*
    * Old VMXs (workstation and express) didn't set their type. In that case,
    * our special pattern will still be there. --hpreg
    */

   /*
    * Need to expand this out since the toolchain's gcc doesn't like mixing
    * integral types and enums in the same trinary operator.
    */
   if (bp.in.cx.halfs.high == 0xFFFF)
      *type = VMX_TYPE_UNSET;
   else
      *type = bp.out.cx.word;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VmCheck_IsVirtualWorld --
 *
 *    Verify that we're running in a VM & we're version compatible with our
 *    environment.
 *
 * Return value:
 *    TRUE if we're in a virtual machine or a Linux compilation using Valgrind,
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

Bool
VmCheck_IsVirtualWorld(void)
{
   uint32 version;
   uint32 dummy;

#if !defined(WINNT_DDK)
#ifdef USE_VALGRIND
   /*
    * Valgrind can't handle the backdoor check.
    */
   return TRUE;
#endif
#if defined VM_X86_ANY
   char *hypervisorSig;
   uint32 i;

   /*
    * Check for other environments like Xen and VirtualPC only if we haven't
    * already detected that we are on a VMware hypervisor. See PR 1035346.
    */
   hypervisorSig = Hostinfo_HypervisorCPUIDSig();
   if (hypervisorSig == NULL ||
         Str_Strcmp(hypervisorSig, CPUID_VMWARE_HYPERVISOR_VENDOR_STRING) != 0) {
      if (hypervisorSig != NULL) {
         static const struct {
            const char *vendorSig;
            const char *hypervisorName;
         } hvVendors[] = {
            { CPUID_KVM_HYPERVISOR_VENDOR_STRING, "Linux KVM" },
            { CPUID_XEN_HYPERVISOR_VENDOR_STRING, "Xen" },
         };

         for (i = 0; i < ARRAYSIZE(hvVendors); i++) {
            if (Str_Strcmp(hypervisorSig, hvVendors[i].vendorSig) == 0) {
               Debug("%s: detected %s.\n", __FUNCTION__,
                     hvVendors[i].hypervisorName);
               free(hypervisorSig);
               return FALSE;
            }
         }
      }

      free(hypervisorSig);

      if (VmCheckSafe(Hostinfo_TouchXen)) {
         Debug("%s: detected Xen.\n", __FUNCTION__);
         return FALSE;
      }

      if (VmCheckSafe(Hostinfo_TouchVirtualPC)) {
         Debug("%s: detected Virtual PC.\n", __FUNCTION__);
         return FALSE;
      }

   } else {
      free(hypervisorSig);
   }
#endif

   if (!VmCheckSafe(Hostinfo_TouchBackDoor)) {
      Debug("%s: backdoor not detected.\n", __FUNCTION__);
      return FALSE;
   }

   /* It should be safe to use the backdoor without a crash handler now. */
   if (!VmCheck_GetVersion(&version, &dummy)) {
      Debug("%s: VmCheck_GetVersion failed.\n", __FUNCTION__);
      return FALSE;
   }
#else
   /*
    * The Win32 vmwvaudio driver uses this function, so keep the old,
    * VMware-only check.
    */
   __try {
      if (!VmCheck_GetVersion(&version, &dummy)) {
         Debug("%s: VmCheck_GetVersion failed.\n", __FUNCTION__);
         return FALSE;
      }
   } __except (GetExceptionCode() == STATUS_PRIVILEGED_INSTRUCTION) {
      return FALSE;
   }
#endif

   if (version != VERSION_MAGIC) {
      Debug("The version of this program is incompatible with your %s.\n"
            "For information on updating your VMware Tools please see the\n"
            "'Upgrading VMware Tools' section of the 'VMware Tools User Guide'"
            "\nat https://docs.vmware.com/en/VMware-Tools/index.html\n"
            "\n", PRODUCT_LINE_NAME);
      return FALSE;
   }

   return TRUE;
}


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
 * vmcheck.c --
 *
 *    Utility functions for discovering our virtualization status.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "vmware.h"
#include "vm_version.h"
#include "vm_tools_version.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "debug.h"

#if !defined(_WIN32) && !defined(N_PLAT_NLM)
#   include "vmsignal.h"
#   include "setjmp.h"
#endif


#if !defined(_WIN32) && !defined(N_PLAT_NLM)
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
      Panic("Recieved SEGV, exiting");
   }
}
#endif


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
   bp.in.size = ~BDOOR_MAGIC;
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
 *    TRUE if we're in a virtual machine, FALSE otherwise.
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
#ifdef N_PLAT_NLM
   /*
    * We are running at CPL0. So we'll not receive SIGSEGV on access
    * and we must do it other way... --petr
    */
   if (!VmCheck_GetVersion(&version, &dummy)) {
      return FALSE;
   }
#elif defined _WIN32
   __try {
      VmCheck_GetVersion(&version, &dummy);
   } __except (GetExceptionCode() == STATUS_PRIVILEGED_INSTRUCTION) {
      return FALSE;
   }
#else // POSIX
   int signals[] = {
      SIGSEGV,
   };
   struct sigaction olds[ARRAYSIZE(signals)];

   if (Signal_SetGroupHandler(signals, olds, ARRAYSIZE(signals),
                              VmCheckSegvHandler) == 0) {
      exit(1);
   }
   if (sigsetjmp(jmpBuf, TRUE) == 0) {
      jmpIsSet = TRUE;
      VmCheck_GetVersion(&version, &dummy);
   } else {
      jmpIsSet = FALSE;
      return FALSE;
   }

   if (Signal_ResetGroupHandler(signals, olds, ARRAYSIZE(signals)) == 0) {
      exit(1);
   }
#endif
   if (version != VERSION_MAGIC) {
      Debug("The version of this program is incompatible with your %s.\n"
            "For information on updating your VMware Tools please see\n"
            "http://www.vmware.com/info?id=99\n"
            "\n", PRODUCT_LINE_NAME);
      return FALSE;
   }

   return TRUE;
}

#ifdef __cplusplus
}
#endif

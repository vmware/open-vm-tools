/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * atomic.c --
 *
 *	Support for atomic instructions.
 *
 *	This is the user-level and vmkernel version.
 *	The monitor-only version is in vmcore/vmm/main.
 */

#include "vmware.h"
#include "vm_atomic.h"
#if defined(__i386__) || defined(__x86_64__)
#include "x86cpuid.h"
#include "x86cpuid_asm.h"
#endif
#include "vm_basic_asm.h"
#include "vmk_exports.h"


/*
 * AMD Rev E/F CPUs suffer from erratum 147 (see AMD docs). Our work-around
 * is to execute a "fence" after every atomic instruction. Since this is 
 * expensive we conditionalize on "AtomicUseFence".
 * ESX no longer supports any of the CPUs, so for SERVER builds neither
 * the vmx nor the vmkernel define these variables in order to force all 
 * code in these (performance critical) components to use the constant 
 * version of AtomicUseFence from vm_atomic.h.
 * For other components, we continue to define the variables to allow the
 * code to work whether it is compiled with VMX86_SERVER set or not. This
 * is conservative but the performance penalty should be minimal. And it
 * is a (longer term) temporary sitation: when we eventually remove Rev F 
 * support from our hosted products, this will all go away.
 */
#if !defined(VMKERNEL) && !(defined(VMX86_VMX) && defined(VMX86_SERVER))
#undef AtomicUseFence
#undef atomicFenceInitialized
Bool AtomicUseFence;
Bool atomicFenceInitialized;
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * AtomicInitFence --
 *
 *	Compute AtomicUseFence.
 *
 *	This version of fence initialization doesn't take into account
 *	the number of CPUs.  Code that cares uses Atomic_SetFence() directly.
 *	See Atomic_Init in vm_atomic.h.
 *
 *	This code is rather weird because while the logic belongs
 *	in x86cpuid.h, it's almost impossible to put this whole function
 *	there, so here it is.  -- edward
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      As described.
 *
 *-----------------------------------------------------------------------------
 */

void
AtomicInitFence(void)
{
   Bool needFence = FALSE;
#if MAY_NEED_AMD_REVF_WORKAROUND && (defined(__i386__) || defined(__x86_64__))
   {
      CPUIDRegs regs;

      __GET_CPUID(0, &regs);
      if (CPUID_ID0RequiresFence(&regs)) {
         __GET_CPUID(1, &regs);
         if (CPUID_ID1RequiresFence(&regs)) {
            needFence = TRUE;
         }
      }
   }
#endif
   Atomic_SetFence(needFence);
}

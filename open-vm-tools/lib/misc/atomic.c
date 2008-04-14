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
 * atomic.c --
 *
 *	Support for atomic instructions.
 *
 *	This is the user-level version.
 *	The monitor-only version is in vmcore/vmm/main.
 */

#include "vmware.h"
#include "vm_atomic.h"
#include "x86cpuid.h"
#include "vm_basic_asm.h"


Bool AtomicUseFence;

Bool atomicFenceInitialized;


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
   CPUIDRegs regs;
   Bool needFence;

   ASSERT(!atomicFenceInitialized);

   needFence = FALSE;
   __GET_CPUID(0, &regs);
   if (CPUID_ID0RequiresFence(&regs)) {
      __GET_CPUID(1, &regs);
      if (CPUID_ID1RequiresFence(&regs)) {
	 needFence = TRUE;
      }
   }

   Atomic_SetFence(needFence);
}

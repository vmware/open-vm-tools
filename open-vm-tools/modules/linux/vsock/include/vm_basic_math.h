/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vm_basic_math.h --
 *
 *	Standard mathematical macros for VMware source code.
 */

#ifndef _VM_BASIC_MATH_H_
#define _VM_BASIC_MATH_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"
#include "vm_basic_types.h" // For INLINE.
#include "vm_basic_asm.h"   // For Div64...


static INLINE uint32
RatioOf(uint32 numer1, uint32 numer2, uint32 denom)
{
   uint64 numer = (uint64)numer1 * numer2;
   /* Calculate "(numer1 * numer2) / denom" avoiding round-off errors. */
#if defined(VMM)
   return numer / denom;
#else
   uint32 ratio;
   uint32 unused;
   Div643232(numer, denom, &ratio, &unused);
   return ratio;
#endif
}

static INLINE uint32
ExponentialAvg(uint32 avg, uint32 value, uint32 gainNumer, uint32 gainDenom)
{
   uint32 term1 = gainNumer * avg;
   uint32 term2 = (gainDenom - gainNumer) * value;
   return (term1 + term2) / gainDenom;
}

#endif // ifndef _VM_BASIC_MATH_H_

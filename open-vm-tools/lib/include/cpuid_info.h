/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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

#ifndef _CPUID_INFO_H
#define _CPUID_INFO_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL

#include "includeCheck.h"

#include "vm_basic_asm.h"
#include "x86cpuid_asm.h"


typedef struct CPUID0 {
   int numEntries;
   char name[16];      // 4 extra bytes to null terminate
} CPUID0;

typedef struct CPUID1 {
   uint32 version;
   uint32 ebx;
   uint32 ecxFeatures;
   uint32 edxFeatures;
} CPUID1;

typedef struct CPUID80 {
   uint32 numEntries;
   uint32 ebx;
   uint32 ecx;
   uint32 edx;
} CPUID80;

typedef struct CPUID81 {
   uint32 eax;
   uint32 ebx;
   uint32 ecxFeatures;
   uint32 edxFeatures;
} CPUID81;

typedef struct CPUIDSummary {
   CPUID0  id0;
   CPUID1  id1;
   CPUIDRegs ida;
   CPUID80 id80;
   CPUID81 id81;
   CPUIDRegs id88, id8a;
} CPUIDSummary;


/*
 *----------------------------------------------------------------------
 *
 * CPUIDSummary_RegsFromCpuid0 --
 *
 *      Fills in the given CPUIDRegs struct with the values from the CPUID0 struct.
 *
 * Results:
 *      Returns the CPUIDRegs pointer passed in.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static INLINE CPUIDRegs*
CPUIDSummary_RegsFromCpuid0(CPUID0* id0In,
                            CPUIDRegs* id0Out)
{
   id0Out->eax = id0In->numEntries;
   id0Out->ebx = *(uint32 *) (id0In->name + 0);
   id0Out->edx = *(uint32 *) (id0In->name + 4);
   id0Out->ecx = *(uint32 *) (id0In->name + 8);
   return id0Out;
}

#endif

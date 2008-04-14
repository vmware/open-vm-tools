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

#ifndef _CPUID_INFO_H
#define _CPUID_INFO_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "x86cpuid.h"
#include "vm_basic_asm.h"



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
   CPUID80 id80;
   CPUID81 id81;
   CPUIDRegs id88, id8a;
} CPUIDSummary;

/* The following two functions return the number of cores per package
   and set *numThreadsPerCore to the number of hardware threads per core. */ 
static INLINE uint32 
CPUIDSummary_AMDCoresPerPackage(CPUIDSummary *cpuid,
                                uint32 *numThreadsPerCore)
{
   uint32 version = cpuid->id1.version, numEntries = cpuid->id80.numEntries;
   *numThreadsPerCore = 1;
   return CPUID_FAMILY_IS_K8STAR(version) && numEntries >= 0x80000008 ?
             CPUID_AMDCoresPerPackage(cpuid->id88.ecx) : 1;
}

static INLINE uint32
CPUIDSummary_IntelCoresPerPackage(CPUIDSummary *cpuid,
                                  uint32 *numThreadsPerCore)
{
   uint32 numCoresPerPackage;
   
   *numThreadsPerCore = numCoresPerPackage = 1;
   /*
    * Multi-core processors have the HT feature bit set even if they don't
    * support HT.  The reported number of HT is the total, not per core.
    */
   if (cpuid->id1.edxFeatures & CPUID_FEATURE_COMMON_ID1EDX_HT) {
      *numThreadsPerCore = CPUID_LCPU_COUNT(cpuid->id1.ebx);
       if (cpuid->id0.numEntries >= 4) {
         numCoresPerPackage =
            CPUID_IntelCoresPerPackage(__GET_EAX_FROM_CPUID4(0));
#ifdef ASSERT
         ASSERT(*numThreadsPerCore % numCoresPerPackage == 0);
#endif
         *numThreadsPerCore /= numCoresPerPackage;
      }
   }
   return numCoresPerPackage;
}

#endif

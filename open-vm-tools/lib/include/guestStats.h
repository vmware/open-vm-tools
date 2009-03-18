/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

/**
 * @file guestStats.h
 *
 *    Common declarations that aid in sending guest statistics to the vmx
 *    and may be further to vmkernel.
 */

#ifndef _GUEST_STATS_H_
#define _GUEST_STATS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

typedef
#include "vmware_pack_begin.h"
struct GuestMemInfo {
   uint32 version;            ///< MemInfo structure version. 
   uint32 flags;              ///< Indicates which stats are valid. 
   uint64 memTotal;           ///< Total physical memory in Kb. 
   uint64 memFree;            ///< Physical memory available in Kb. 
   uint64 memBuff;            ///< Physical memory used as buffer cache in Kb. 
   uint64 memCache;           ///< Physical memory used as cache in Kb. 
   uint64 memActive;          ///< Physical memory actively in use in Kb (working set) 
   uint64 memInactive;        ///< Physical memory inactive in Kb (cold pages) 
   uint64 swapInRate;         ///< Memory swapped out in Kb / sec. 
   uint64 swapOutRate;        ///< Memory swapped out in Kb / sec. 
   uint64 ioInRate;           ///< Amount of I/O in Kb / sec. 
   uint64 ioOutRate;          ///< Amount of I/O out in Kb / sec. 
   uint64 hugePagesTotal;     ///< Total number of huge pages. 
   uint64 hugePagesFree;      ///< Available number of huge pages. 
   uint64 memPinned;          ///< Unreclaimable physical memory in 4K page size. 
}
#include "vmware_pack_end.h"
GuestMemInfo;

/* Flags for GuestMemInfo. */
#define MEMINFO_MEMTOTAL         (1 << 0)
#define MEMINFO_MEMFREE          (1 << 1)
#define MEMINFO_MEMBUFF          (1 << 2)
#define MEMINFO_MEMCACHE         (1 << 3)
#define MEMINFO_MEMACTIVE        (1 << 4)
#define MEMINFO_MEMINACTIVE      (1 << 5)
#define MEMINFO_SWAPINRATE       (1 << 6)
#define MEMINFO_SWAPOUTRATE      (1 << 7)
#define MEMINFO_IOINRATE         (1 << 8)
#define MEMINFO_IOOUTRATE        (1 << 9)
#define MEMINFO_HUGEPAGESTOTAL   (1 << 10)
#define MEMINFO_HUGEPAGESFREE    (1 << 11)
#define MEMINFO_MEMPINNED        (1 << 12)

#endif // _GUEST_STATS_H_


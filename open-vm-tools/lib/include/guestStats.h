/*********************************************************
 * Copyright (C) 2008-2018,2020-2021 VMware, Inc. All rights reserved.
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

#include "vm_assert.h"
#include "vm_basic_types.h"

#define PUBLISH_EXPERIMENTAL_STATS   0
#define ADD_NEW_STATS                0

/*
 * Version 1: Legacy data
 * Version 2: Dead
 * Version 3: Dead
 * Version 4: Dead
 * Version 5: Legacy structure followed by one or more GuestStat
 *            structures and data.
 */

#define GUESTMEMINFO_V1 1
#define GUESTMEMINFO_V2 2
#define GUESTMEMINFO_V3 3
#define GUESTMEMINFO_V4 4
#define GUESTMEMINFO_V5 5

/*
 * Flags for GuestMemInfoLegacy
 *
 * !!! DON'T ADD/CHANGE FLAGS !!!
 *
 * This is deprecated. All new values are returned via a GuestStat list.
 */

#define MEMINFO_MEMTOTAL         (1 << 0)
#define MEMINFO_DEPRECATED1      (1 << 1)
#define MEMINFO_DEPRECATED2      (1 << 2)
#define MEMINFO_DEPRECATED3      (1 << 3)
#define MEMINFO_DEPRECATED4      (1 << 4)
#define MEMINFO_DEPRECATED5      (1 << 5)
#define MEMINFO_DEPRECATED6      (1 << 6)
#define MEMINFO_DEPRECATED7      (1 << 7)
#define MEMINFO_DEPRECATED8      (1 << 8)
#define MEMINFO_DEPRECATED9      (1 << 9)
#define MEMINFO_HUGEPAGESTOTAL   (1 << 10)
#define MEMINFO_DEPRECATED10     (1 << 11)
#define MEMINFO_DEPRECATED11     (1 << 12)
#define MEMINFO_MEMNEEDED        (1 << 13)

/*
 * Legacy GuestMemInfo structure.
 *
 * !!! DON'T CHANGE IT !!!
 *
 * It should stay the same to ensure binary compatibility.
 */

#pragma pack(push, 1)
typedef struct GuestMemInfoLegacy {
   uint32 version;            ///< MemInfo structure version.
   uint32 flags;              ///< Indicates which stats are valid.
   uint64 memTotal;           ///< Total physical memory in Kb.
   uint64 deprecated1[9];     ///< No longer used.
   uint64 hugePagesTotal;     ///< Total number of huge pages.
   uint64 deprecated2[2];     ///< No longer used.
} GuestMemInfoLegacy;
#pragma pack(pop)

/*
 * A stat begins with a header. The header has a mask which says what data
 * follows. Each datum has a size field which says how much data follows so it
 * can be used or ignored. The order of the data that follows is that of the
 * bits, lowest order bit to highest.
 */

typedef enum {
    GUEST_DATUM_PRAGMA            = 0x0001,  // escape hatch (future expansion)
    GUEST_DATUM_NAMESPACE         = 0x0002,  // UTF8 string
    GUEST_DATUM_ID                = 0x0004,  // uint8 - uint64
    GUEST_DATUM_VALUE_TYPE_ENUM   = 0x0008,  // uint8 - uint32
    GUEST_DATUM_VALUE_TYPE_STRING = 0x0010,  // UTF8 string
    GUEST_DATUM_VALUE_UNIT_ENUM   = 0x0020,  // uint8 - uint32
    GUEST_DATUM_VALUE_UNIT_STRING = 0x0040,  // UTF8 string
    GUEST_DATUM_VALUE             = 0x0080,  // value data
} GuestDatum;

#pragma pack(push, 1)
typedef struct GuestStatHeader {
   GuestDatum  datumFlags;  // Indicates how many and which data follow
} GuestStatHeader;
#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning (disable :4200) // non-std extension: zero-sized array in struct
#endif

#pragma pack(push, 1)
typedef struct GuestDatumHeader {
   uint16  dataSize;  // dataSize - May be zero
   char    data[0];   // data - if dataSize is not zero.
} GuestDatumHeader;
#pragma pack(pop)

/*
 * Units datum enum.
 * Note: The entirety (all bits) of the units must always be understood by a client.
 *
 * First we define some modifiers, then the enum itself
 *
 * bits 0-5 define base types (information, time, etc.)
 * bits 6-10 are modifiers, four of which are reserved (in the future, we could
 * define two of them as custom modifiers, for things like changing the radix from 2^10
 * to 10^3 for storage, or for denoting rates are in 100ns units).
 */
#define GuestUnitsModifier_Rate      0x0040
#define GuestUnitsModifier_Reserved0 0x0080
#define GuestUnitsModifier_Reserved1 0x0100
#define GuestUnitsModifier_Reserved2 0x0200
#define GuestUnitsModifier_Reserved3 0x0400

/*
 * bits 11-15 are scale modifiers:
 * This includes common scales: (P)ositive powers, (N)egative powers,
 * and (C)ustom scales (bits, pages, etc.), which are always type specific.
 */
#define GuestUnitsScale_P0           0x0000
#define GuestUnitsScale_P1           0x0800
#define GuestUnitsScale_P2           0x1000
#define GuestUnitsScale_P3           0x1800
#define GuestUnitsScale_P4           0x2000
#define GuestUnitsScale_P5           0x2800
#define GuestUnitsScale_P6           0x3000
#define GuestUnitsScale_Reserved0    0x3800

#define GuestUnitsScale_N1           0x4000
#define GuestUnitsScale_N2           0x4800
#define GuestUnitsScale_N3           0x5000
#define GuestUnitsScale_N4           0x5800
#define GuestUnitsScale_N5           0x6000
#define GuestUnitsScale_N6           0x6800
#define GuestUnitsScale_Reserved1    0x7000
#define GuestUnitsScale_Reserved2    0x7800

#define GuestUnitsScale_C0           0x8000
#define GuestUnitsScale_C1           0x8800
#define GuestUnitsScale_C2           0x9000
#define GuestUnitsScale_C3           0x9800
// 0xA000-0xF800 are reserved.

typedef enum {
   GuestUnitsInvalid            = 0, // Must never be sent
   GuestUnitsNone               = 1, // A valid value, but not any of the below units.
   GuestUnitsNumber             = 2, // default radix is 1000
   GuestUnitsInformation        = 3, // default radix is 1024
   GuestUnitsDuration           = 4, // default radix is 1000
   GuestUnitsCycles             = 5, // default radix is 1000

   GuestUnitsBytes              = GuestUnitsInformation | GuestUnitsScale_P0,
   GuestUnitsKiB                = GuestUnitsInformation | GuestUnitsScale_P1,
   GuestUnitsMiB                = GuestUnitsInformation | GuestUnitsScale_P2,
   GuestUnitsPages              = GuestUnitsInformation | GuestUnitsScale_C0,
   GuestUnitsHugePages          = GuestUnitsInformation | GuestUnitsScale_C1,
   GuestUnitsBytesPerSecond     = GuestUnitsBytes       | GuestUnitsModifier_Rate,
   GuestUnitsKiBPerSecond       = GuestUnitsKiB         | GuestUnitsModifier_Rate,
   GuestUnitsMiBPerSecond       = GuestUnitsMiB         | GuestUnitsModifier_Rate,
   GuestUnitsPagesPerSecond     = GuestUnitsPages       | GuestUnitsModifier_Rate,
   GuestUnitsHugePagesPerSecond = GuestUnitsHugePages   | GuestUnitsModifier_Rate,

   GuestUnitsAttoSeconds        = GuestUnitsDuration    | GuestUnitsScale_N6,
   GuestUnitsFemtoSeconds       = GuestUnitsDuration    | GuestUnitsScale_N5,
   GuestUnitsPicoSeconds        = GuestUnitsDuration    | GuestUnitsScale_N4,
   GuestUnitsNanoSeconds        = GuestUnitsDuration    | GuestUnitsScale_N3,
   GuestUnitsMicroSeconds       = GuestUnitsDuration    | GuestUnitsScale_N2,
   GuestUnitsMilliSeconds       = GuestUnitsDuration    | GuestUnitsScale_N1,
   GuestUnitsSeconds            = GuestUnitsDuration    | GuestUnitsScale_P0,

   GuestUnitsHz                 = GuestUnitsCycles | GuestUnitsScale_P0 | GuestUnitsModifier_Rate,
   GuestUnitsKiloHz             = GuestUnitsCycles | GuestUnitsScale_P1 | GuestUnitsModifier_Rate,
   GuestUnitsMegaHz             = GuestUnitsCycles | GuestUnitsScale_P2 | GuestUnitsModifier_Rate,
   GuestUnitsGigaHz             = GuestUnitsCycles | GuestUnitsScale_P3 | GuestUnitsModifier_Rate,
   GuestUnitsTeraHz             = GuestUnitsCycles | GuestUnitsScale_P4 | GuestUnitsModifier_Rate,

   GuestUnitsPercent            = GuestUnitsNumber | GuestUnitsScale_C0, // integers: must be 0...100; FP: 0.0...1.0
   GuestUnitsNumberPerSecond    = GuestUnitsNumber | GuestUnitsModifier_Rate,
} GuestValueUnits;

/*
 * Data type datum enum.
 * Note: The entirety (all bits) of the type must always be understood by a client.
 *
 * Bits 0-5 are for types.
 * Bits 6-15 are reserved. In the future, one bit will denote arrays.
 */

#define GuestTypeModifier_Reserved0 0x0040 // More Reserved1-9 not shown.

typedef enum {
   GuestTypeInvalid,  // Must never be sent
   GuestTypeNil,      // A stat that has no value
   GuestTypeInt8,     // Little endian
   GuestTypeUint8,    // Little endian
   GuestTypeInt16,    // Little endian
   GuestTypeUint16,   // Little endian
   GuestTypeInt32,    // Little endian
   GuestTypeUint32,   // Little endian
   GuestTypeInt64,    // Little endian
   GuestTypeUint64,   // Little endian
   GuestTypeFloat,    // IEEE 754
   GuestTypeDouble,   // IEEE 754
   GuestTypeString,   // NUL terminated UTF8
   GuestTypeBinary,   // Binary blob
} GuestValueType;

/*
 * Defines the namespace used for guest tools buildin query.
 */
#define GUEST_TOOLS_NAMESPACE "_tools/v1"

/*
 * Defined stat IDs for guest tools builtin query.
 * See vmx/vigorapi/GuestStats.java for documentation
 *
 * NOTE: These IDs are relative to GUEST_TOOLS_NAMESPACE
 * NOTE: DO NOT re-order or remove the IDs. IDs can only be added to the end,
 *       unless you make the totally backward-compatibility breaking change
 *       of bumping the namespace version.
 */
#define GUEST_STAT_TOOLS_IDS \
   /* 6.0u1 stats */ \
   DEFINE_GUEST_STAT(GuestStatID_Invalid,                         0,  "__INVALID__") \
   DEFINE_GUEST_STAT(GuestStatID_None,                            1,  "__NONE__") \
   DEFINE_GUEST_STAT(GuestStatID_ContextSwapRate,                 2,  "guest.contextSwapRate") \
   DEFINE_GUEST_STAT(GuestStatID_MemActiveFileCache,              3,  "guest.mem.activeFileCache") \
   DEFINE_GUEST_STAT(GuestStatID_MemFree,                         4,  "guest.mem.free") \
   DEFINE_GUEST_STAT(GuestStatID_MemNeeded,                       5,  "guest.mem.needed") \
   DEFINE_GUEST_STAT(GuestStatID_MemPhysUsable,                   6,  "guest.mem.physUsable") \
   DEFINE_GUEST_STAT(GuestStatID_PageInRate,                      7,  "guest.page.inRate") \
   DEFINE_GUEST_STAT(GuestStatID_PageOutRate,                     8,  "guest.page.outRate") \
   DEFINE_GUEST_STAT(GuestStatID_SwapSpaceRemaining,              9,  "guest.swap.spaceRemaining") \
   DEFINE_GUEST_STAT(GuestStatID_PhysicalPageSize,                10, "guest.page.size") \
   DEFINE_GUEST_STAT(GuestStatID_HugePageSize,                    11, "guest.hugePage.size") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_HugePagesTotal,            12, "guest.hugePage.total") \
   /* 6.5 stats */ \
   DEFINE_GUEST_STAT(GuestStatID_MemNeededReservation,            13, "guest.mem.neededReservation") \
   DEFINE_GUEST_STAT(GuestStatID_PageSwapInRate,                  14, "guest.swap.pageInRate") \
   DEFINE_GUEST_STAT(GuestStatID_PageSwapOutRate,                 15, "guest.swap.pageOutRate") \
   DEFINE_GUEST_STAT(GuestStatID_ProcessCreationRate,             16, "guest.processCreationRate") \
   DEFINE_GUEST_STAT(GuestStatID_SwapSpaceUsed,                   17, "guest.swap.used") \
   DEFINE_GUEST_STAT(GuestStatID_SwapFilesCurrent,                18, "guest.swap.filesCurrent") \
   DEFINE_GUEST_STAT(GuestStatID_SwapFilesMax,                    19, "guest.swap.filesMax") \
   DEFINE_GUEST_STAT(GuestStatID_ThreadCreationRate,              20, "guest.threadCreationRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_HugePagesFree,             21, "guest.hugePage.free") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_LowWaterMark,              22, "guest.mem.lowWaterMark") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemActive,                 23, "guest.mem.active") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemActiveAnon,             24, "guest.mem.activeAnon") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemAvailable,              25, "guest.mem.available") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemBuffers,                26, "guest.mem.buffers") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemCached,                 27, "guest.mem.cached") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemCommitted,              28, "guest.mem.committed") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemDirty,                  29, "guest.mem.dirty") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemInactive,               30, "guest.mem.inactive") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemInactiveAnon,           31, "guest.mem.inactiveAnon") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemInactiveFile,           32, "guest.mem.inactiveFile") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemPinned,                 33, "guest.mem.pinned") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemSlabReclaim,            34, "guest.mem.slabReclaim") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemSwapCached,             35, "guest.mem.swap.cached") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageDirectScanRate,        36, "guest.page.directScanRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageFaultRate,             37, "guest.page.faultRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageFreeRate,              38, "guest.page.freeRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageMajorFaultRate,        39, "guest.page.majorFaultRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageStealRate,             40, "guest.page.stealRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_PageSwapScanRate,          41, "guest.page.swapScanRate") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_Swappiness,                42, "guest.swappiness") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemModifiedPages,        43, "guest.mem.modifiedPages") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemAvailableToMm,        44, "guest.mem.availableToMm") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemStandbyCore,          45, "guest.mem.standby.core") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemStandbyNormal,        46, "guest.mem.standby.normal") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemStandbyReserve,       47, "guest.mem.standby.reserve") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemPagedPoolResident,    48, "guest.mem.pagedPool.resident") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemSystemCodeResident,   49, "guest.system.codeResident") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemSystemDriverResident, 50, "guest.system.driverResident") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemNonPagedPool,         51, "guest.mem.nonPagedPool.size") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemCache,                52, "guest.mem.cache") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_FreeSystemPtes,          53, "guest.system.freePtes") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemCommitLimit,          54, "guest.mem.commitLimit") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemCommitted,            55, "guest.mem.committed") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_MemPrivateWorkingSet,    56, "guest.mem.privateWorkingSet") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_DiskReadRate,            57, "guest.disk.readRate") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_DiskWriteRate,           58, "guest.disk.writeRate") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_AutomaticSwapFileMax,    59, "guest.swap.automaticFileMax") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_MemTotal,                  60, "guest.mem.total") \
   /* (6.7, ] stats */ \
   DEFINE_GUEST_STAT(GuestStatID_Linux_CpuRunQueue,               61, "guest.cpu.runQueue") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_DiskRequestQueue,          62, "guest.disk.requestQueue") \
   DEFINE_GUEST_STAT(GuestStatID_Linux_DiskRequestQueueAvg,       63, "guest.disk.requestQueueAvg") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_ProcessorQueue,          64, "guest.processor.queue") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_DiskQueue,               65, "guest.disk.queue") \
   DEFINE_GUEST_STAT(GuestStatID_Windows_DiskQueueAvg,            66, "guest.disk.queueAvg") \
   DEFINE_GUEST_STAT(GuestStatID_Max,                             67, "__MAX__")

/*
 * Define stats enumeration
 */
#undef DEFINE_GUEST_STAT
#define DEFINE_GUEST_STAT(x,y,z) x,
typedef enum GuestStatToolsID {
   GUEST_STAT_TOOLS_IDS
} GuestStatToolsID;

/*
 * Enforce ordering and compactness of the enumeration
 */
#undef DEFINE_GUEST_STAT
#define DEFINE_GUEST_STAT(x,y,z) ASSERT_ON_COMPILE(x==y);

MY_ASSERTS(GUEST_STAT_IDS_ARE_WELL_ORDERED, GUEST_STAT_TOOLS_IDS)

#undef DEFINE_GUEST_STAT

#endif // _GUEST_STATS_H_

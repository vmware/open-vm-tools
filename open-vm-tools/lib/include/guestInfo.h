/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * guestInfo.h --
 *
 *    Common declarations that aid in sending guest information to the host.
 */

#ifndef _GUEST_INFO_H_
#define _GUEST_INFO_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#include "dbllnklst.h"

#define GUEST_INFO_COMMAND_TWO "SetGuestInfo2"
#define GUEST_INFO_COMMAND "SetGuestInfo"
#define MAX_VALUE_LEN 100
#define MAX_NICS     16
#define MAX_IPS      8     // Max number of IP addresses for a single NIC
#define MAC_ADDR_SIZE 19
#define IP_ADDR_SIZE 16
#define IP_ADDR_SIZE_V2 48 // 40 bytes for address + 3 for netmask + 5 padding
#define PARTITION_NAME_SIZE MAX_VALUE_LEN
#define GUESTINFO_TIME_INTERVAL_MSEC 3000  /* time interval in msec */

typedef enum {
   INFO_ERROR,       /* Zero is unused so that errors in atoi can be caught. */
   INFO_DNS_NAME,
   INFO_IPADDRESS,
   INFO_DISK_FREE_SPACE,
   INFO_TOOLS_VERSION,
   INFO_OS_NAME_FULL,
   INFO_OS_NAME,
   INFO_UPTIME,
   INFO_MEMORY,
   INFO_MAX
} GuestInfoType;

typedef enum {
   INFO_IP_ADDRESS_FAMILY_IPV4,
   INFO_IP_ADDRESS_FAMILY_IPV6
} GuestInfoIPAddressFamilyType;

/*
 * For backward compatibility's sake over wire (from Tools to VMX), new fields 
 * in this struct must be added at the end.  THis is the part that goes over wire.
 */
typedef struct VmIpAddressEntryProtocol {
   uint32 addressFamily;             /* uint8 should be enough.  However we */
                                     /* need it to be multiple of 4 bytes */
                                     /* in order to have the same size on */
                                     /* different hardware architectures */
   uint32 dhcpEnabled;   /* This is a boolean.  However we need it to be */
                         /* multiple of 4 bytes, in order to be the same */
                         /* on different hardware architecture */
   char ipAddress[IP_ADDR_SIZE_V2];
   char subnetMask[IP_ADDR_SIZE_V2];
   uint32 totalIpEntrySizeOnWire;
} VmIpAddressEntryProtocol;

typedef struct VmIpAddressEntry {
   DblLnkLst_Links links;
   VmIpAddressEntryProtocol ipEntryProto;
} VmIpAddressEntry;

typedef struct NicEntryV1 {
   unsigned int numIPs;
   char macAddress[MAC_ADDR_SIZE];  // In the format "12-23-34-45-56-67"
   char ipAddress[MAX_IPS][IP_ADDR_SIZE];
} NicEntryV1;

/*
 * For backward compatibility's sake over wire (from Tools to VMX), new fields 
 * in this struct must be added at the end.  THis is the part that goes over wire.
 */
typedef struct NicEntryProtocol {     
   char macAddress[MAC_ADDR_SIZE];   /* In the format "12-23-34-45-56-67" */
   char pad[1];                      /* MAC_ADDR_SIZE happens to be 19.  */
                                     /* Pad it to be multiple of 4 bytes */
   uint32 numIPs;
   uint32 ipAddressSizeOnWire;      /* size of struct VmIpAddresses over wire*/
   uint32 totalNicEntrySizeOnWire;
} NicEntryProtocol;

typedef struct NicEntry {
   DblLnkLst_Links links;           
   NicEntryProtocol nicEntryProto;
   DblLnkLst_Links ipAddressList;
/* DblLnkLst_Links gatewayList; */
} NicEntry;

typedef struct NicInfoV1 {
   unsigned int numNicEntries;
   NicEntryV1 nicList[MAX_NICS];
} NicInfoV1;

/*
 * For backward compatibility's sake over wire (from Tools to VMX), new fields 
 * in this struct must be added at the end.  This is the part that goes over wire.
 */
typedef struct NicInfoProtocol { 
   uint32 version;
   uint32 nicEntrySizeOnWire;      /* length of NicEntry over wire.  Lengths differ */
                                   /* with different versions. */
   uint32 numNicEntries;
   uint32 totalInfoSizeOnWire;    
} NicInfoProtocol;

typedef struct NicInfo {
   NicInfoProtocol    nicInfoProto;
   DblLnkLst_Links    nicList;     /* Pointers in it must be initialized to NULL */
} NicInfo;

typedef
#include "vmware_pack_begin.h"
struct _PartitionEntry {
   uint64 freeBytes;
   uint64 totalBytes;
   char name[PARTITION_NAME_SIZE]; 
}
#include "vmware_pack_end.h"
PartitionEntry, *PPartitionEntry;

typedef struct _DiskInfo {
   unsigned int numEntries;
   PPartitionEntry partitionList;
} DiskInfo, *PDiskInfo;

typedef
#include "vmware_pack_begin.h"
struct MemInfo {
   uint32 version;            /* MemInfo structure version. */
   uint32 flags;              /* Indicates which stats are valid. */
   uint64 memTotal;           /* Total physical memory in Kb. */
   uint64 memFree;            /* Physical memory available in Kb. */
   uint64 memBuff;            /* Physical memory used as buffer cache in Kb. */
   uint64 memCache;           /* Physical memory used as cache in Kb. */
   uint64 memActive;          /* Physical memory actively in use in Kb (working set) */
   uint64 memInactive;        /* Physical memory inactive in Kb (cold pages) */
   uint64 swapInRate;         /* Memory swapped out in Kb / sec. */
   uint64 swapOutRate;        /* Memory swapped out in Kb / sec. */
   uint64 ioInRate;           /* Amount of I/O in in blocks / sec. */
   uint64 ioOutRate;          /* Amount of I/O out in blocks / sec. */
   uint64 hugePagesTotal;     /* Total number of huge pages. */
   uint64 hugePagesFree;      /* Available number of huge pages. */
   uint64 memPinned;          /* Unreclaimable physical memory in 4K page size. */
}
#include "vmware_pack_end.h"
MemInfo;

/* Flags for MemInfo. */
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

/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_FreeDynamicMemoryInNic --
 *
 *      Traverse the link list and free all dynamically allocated memory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void 
GuestInfo_FreeDynamicMemoryInNic(NicEntry *nicEntry)        // IN
{
   VmIpAddressEntry *ipAddressCur;
   DblLnkLst_Links *sCurrent;
   DblLnkLst_Links *sNext;

   if (NULL == nicEntry) {
      return;
   }

   if (0 == nicEntry->nicEntryProto.numIPs) {
      return;
   }

   DblLnkLst_ForEachSafe(sCurrent, sNext, &nicEntry->ipAddressList) {

      ipAddressCur = DblLnkLst_Container(sCurrent,
                                         VmIpAddressEntry, 
                                         links);

      DblLnkLst_Unlink1(&ipAddressCur->links);
      free(ipAddressCur);
   }

   DblLnkLst_Init(&nicEntry->ipAddressList);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_FreeDynamicMemoryInNicInfo --
 *
 *      Free all dynamically allocated memory in the struct pointed to 
 *      by nicInfo.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void 
GuestInfo_FreeDynamicMemoryInNicInfo(NicInfo *nicInfo)      // IN
{
   NicEntry *nicEntryCur = NULL;
   DblLnkLst_Links *sCurrent;
   DblLnkLst_Links *sNext;

   if (NULL == nicInfo) {
      return;
   }

   if (0 == nicInfo->nicInfoProto.numNicEntries) {
      return;
   }

   DblLnkLst_ForEachSafe(sCurrent, sNext, &nicInfo->nicList) {
      nicEntryCur = DblLnkLst_Container(sCurrent,
                                        NicEntry, 
                                        links);

      GuestInfo_FreeDynamicMemoryInNic(nicEntryCur);
      DblLnkLst_Unlink1(&nicEntryCur->links);
      free (nicEntryCur);
   } 

   DblLnkLst_Init(&nicInfo->nicList);
}


#endif // _GUEST_INFO_H_


/*********************************************************
 * Copyright (C) 2003-2019 VMware, Inc. All rights reserved.
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
 * @file guestInfo.h
 *
 * Common declarations that aid in sending guest information to the host.
 */

/**
 * @defgroup vmtools_guestInfoAPI GuestInfo API Reference
 * @{
 *
 * @brief APIs implementing the GuestInfo feature.
 *
 * Definitions below are used for communication across the backdoor between
 * the VMware Tools Service (running in the guest) and the VMX (running in
 * the host).
 *
 * @sa @ref vmtools_guestInfo for a high level overview.
 */

#ifndef _GUEST_INFO_H_
#define _GUEST_INFO_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#include "dbllnklst.h"
#include "guestrpc/nicinfo.h"

#define GUEST_INFO_COMMAND "SetGuestInfo"
#define GUEST_DISK_INFO_COMMAND "SetGuestDiskInfo"
#define MAX_VALUE_LEN 100

#define MAX_NICS     16
#define MAX_IPS      8     // Max number of IP addresses for a single NIC
#define INFO_IPADDRESS_V2_MAX_IPS 64
#define MAC_ADDR_SIZE 19
#define IP_ADDR_SIZE 16
#define PARTITION_NAME_SIZE MAX_VALUE_LEN
#define FSTYPE_SIZE 260 // Windows fs types can be up to MAX_PATH chars
#define DISK_DEVICE_NAME_SIZE 15 // Max size for disk device name - scsi?:?

/* Value to be used when "primary" IP address is indeterminable. */
#define GUESTINFO_IP_UNKNOWN "unknown"

typedef enum {
   INFO_ERROR,       /* Zero is unused so that errors in atoi can be caught. */
   INFO_DNS_NAME,
   INFO_IPADDRESS,
   INFO_DISK_FREE_SPACE,
   INFO_BUILD_NUMBER,
   INFO_OS_NAME_FULL,
   INFO_OS_NAME,
   INFO_UPTIME,
   INFO_MEMORY,
   INFO_IPADDRESS_V2,
   INFO_IPADDRESS_V3,
   INFO_OS_DETAILED,
   INFO_MAX
} GuestInfoType;

typedef enum {
   INFO_IP_ADDRESS_FAMILY_IPV4,
   INFO_IP_ADDRESS_FAMILY_IPV6
} GuestInfoIPAddressFamilyType;

typedef struct NicEntryV1 {
   unsigned int numIPs;
   char macAddress[MAC_ADDR_SIZE];  // In the format "12-23-34-45-56-67"
   char ipAddress[MAX_IPS][IP_ADDR_SIZE];
} NicEntryV1;

typedef struct GuestNicInfoV1 {
   unsigned int numNicEntries;
   NicEntryV1 nicList[MAX_NICS];
} GuestNicInfoV1;

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
} GuestDiskInfo, *PGuestDiskInfo;

#define DISK_INFO_VERSION_1 1

/* Disk info json keys */
#define DISK_INFO_KEY_VERSION          "version"
#define DISK_INFO_KEY_DISKS            "disks"
#define DISK_INFO_KEY_DISK_NAME        "name"
#define DISK_INFO_KEY_DISK_FREE        "free"
#define DISK_INFO_KEY_DISK_SIZE        "size"
#define DISK_INFO_KEY_DISK_UUID        "uuid"
#define DISK_INFO_KEY_DISK_FSTYPE      "fstype"
#define DISK_INFO_KEY_DISK_DEVICE_ARR  "devices"

/**
 * @}
 */

#endif // _GUEST_INFO_H_

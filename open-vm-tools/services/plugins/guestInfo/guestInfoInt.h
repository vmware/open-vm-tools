/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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

#ifndef _GUESTINFOINT_H_
#define _GUESTINFOINT_H_

/**
 * @file guestInfoInt.h
 *
 * Declares internal functions and data structures of the guestInfo plugin.
 */

#define G_LOG_DOMAIN "guestinfo"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"

#include "nicInfo.h"
#include "dynbuf.h"

/* Default for whether to query and report disk UUIDs */
#define CONFIG_GUESTINFO_REPORT_UUID_DEFAULT TRUE

/* Default for whether to query and report disk devices */
#define CONFIG_GUESTINFO_REPORT_DEVICE_DEFAULT TRUE

/*
 * Plugin-specific data structures for the DiskGuestInfo.
 *
 * These expand upon the GuestDiskInfo in bora/public/guestInfo.h,
 * but are not shared and need not maintain any version compatibility.
 */

typedef char DiskDevName[DISK_DEVICE_NAME_SIZE];

typedef struct _PartitionEntryInt {
   uint64 freeBytes;
   uint64 totalBytes;
   char name[PARTITION_NAME_SIZE];
   char fsType[FSTYPE_SIZE];
#ifdef _WIN32
   /* UUID of the disk, if known.  Currently only Windows */
   char uuid[PARTITION_NAME_SIZE];
#else
   /* Linux LVM mounted filesystems can span multiple disk devices. */
   int diskDevCnt;
   DiskDevName *diskDevNames;
#endif
} PartitionEntryInt;

typedef struct _GuestDiskInfoInt {
   unsigned int numEntries;
   PartitionEntryInt *partitionList;
} GuestDiskInfoInt;

extern int guestInfoPollInterval;

Bool
GuestInfo_ServerReportStats(ToolsAppCtx *ctx,  // IN
                            DynBuf *stats);    // IN

gboolean
GuestInfo_StatProviderPoll(gpointer data);

#ifndef _WIN32
GuestDiskInfoInt *
GuestInfoGetDiskInfoWiper(Bool includeReserved,
                          Bool reportDevices);
#endif

GuestDiskInfoInt *
GuestInfo_GetDiskInfo(const ToolsAppCtx *ctx);

void
GuestInfo_FreeDiskInfo(GuestDiskInfoInt *di);

void
GuestInfo_StatProviderShutdown(void);

#endif /* _GUESTINFOINT_H_ */


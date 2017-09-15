/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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
 * @file diskInfo.c
 *
 *	Get disk information.
 */

#include <stdlib.h>
#include <string.h>

#if defined _WIN32
#   include <ws2tcpip.h>
#endif

#include "vm_assert.h"
#include "debug.h"
#include "guestInfoInt.h"
#include "str.h"
#include "util.h"
#include "xdrutil.h"
#include "netutil.h"
#include "wiper.h"


/*
 ******************************************************************************
 * GuestInfo_FreeDiskInfo --                                             */ /**
 *
 * @brief Frees memory allocated by GuestInfoGetDiskInfo.
 *
 * @param[in] di    DiskInfo container.
 *
 ******************************************************************************
 */

void
GuestInfo_FreeDiskInfo(GuestDiskInfo *di)
{
   if (di) {
      free(di->partitionList);
      free(di);
   }
}


/*
 * Private library functions.
 */


/*
 ******************************************************************************
 * GuestInfoGetDiskInfoWiper --                                          */ /**
 *
 * Uses wiper library to enumerate fixed volumes and lookup utilization data.
 *
 * @return Pointer to a GuestDiskInfo structure on success or NULL on failure.
 *         Caller should free returned pointer with GuestInfoFreeDiskInfo.
 *
 ******************************************************************************
 */

GuestDiskInfo *
GuestInfoGetDiskInfoWiper(void)
{
   WiperPartition_List pl;
   DblLnkLst_Links *curr;
   unsigned int partCount = 0;
   uint64 freeBytes = 0;
   uint64 totalBytes = 0;
   unsigned int partNameSize = 0;
   Bool success = FALSE;
   GuestDiskInfo *di;

   /* Get partition list. */
   if (!WiperPartition_Open(&pl)) {
      g_warning("GetDiskInfo: ERROR: could not get partition list\n");
      return FALSE;
   }

   di = Util_SafeCalloc(1, sizeof *di);
   partNameSize = sizeof (di->partitionList)[0].name;

   DblLnkLst_ForEach(curr, &pl.link) {
      WiperPartition *part = DblLnkLst_Container(curr, WiperPartition, link);

      if (part->type != PARTITION_UNSUPPORTED) {
         PPartitionEntry newPartitionList;
         PPartitionEntry partEntry;
         unsigned char *error;

         error = WiperSinglePartition_GetSpace(part, &freeBytes, &totalBytes);
         if (strlen(error)) {
            g_warning("GetDiskInfo: ERROR: could not get space for partition %s: %s\n",
                    part->mountPoint, error);
            goto out;
         }

         if (strlen(part->mountPoint) + 1 > partNameSize) {
            g_warning("GetDiskInfo: ERROR: Partition name buffer too small\n");
            goto out;
         }

         newPartitionList = Util_SafeRealloc(di->partitionList,
                                             (partCount + 1) *
                                             sizeof *di->partitionList);

         partEntry = &newPartitionList[partCount++];
         Str_Strcpy(partEntry->name, part->mountPoint, partNameSize);
         partEntry->freeBytes = freeBytes;
         partEntry->totalBytes = totalBytes;

         di->partitionList = newPartitionList;
      }
   }

   di->numEntries = partCount;
   success = TRUE;

out:
   if (!success) {
      GuestInfo_FreeDiskInfo(di);
      di = NULL;
   }
   WiperPartition_Close(&pl);
   return di;
}

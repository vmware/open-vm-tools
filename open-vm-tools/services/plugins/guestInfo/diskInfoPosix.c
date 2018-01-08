/*********************************************************
 * Copyright (C) 2014-2017 VMware, Inc. All rights reserved.
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
 * @file diskInfoPosix.c
 *
 * Contains POSIX-specific bits of gettting disk information.
 */

#include "conf.h"
#include "util.h"
#include "vmware.h"
#include "guestInfoInt.h"


/*
 ******************************************************************************
 * GuestInfo_GetDiskInfo --                                              */ /**
 *
 * Uses wiper library to enumerate fixed volumes and lookup utilization data.
 *
 * @return Pointer to a GuestDiskInfo structure on success or NULL on failure.
 *         Caller should free returned pointer with GuestInfoFreeDiskInfo.
 *
 ******************************************************************************
 */

GuestDiskInfo *
GuestInfo_GetDiskInfo(const ToolsAppCtx *ctx)
{
   gboolean includeReserved;

   /*
    * In order to be consistent with the way 'df' reports
    * disk free space, we don't include the reserved space
    * while reporting the disk free space by default.
    */
   includeReserved = VMTools_ConfigGetBoolean(ctx->config,
                                              CONFGROUPNAME_GUESTINFO,
                                              CONFNAME_DISKINFO_INCLUDERESERVED,
                                              FALSE);
   if (includeReserved) {
      g_debug("Including reserved space in diskInfo stats.\n");
   } else {
      g_debug("Excluding reserved space from diskInfo stats.\n");
   }

   return GuestInfoGetDiskInfoWiper(includeReserved);
}

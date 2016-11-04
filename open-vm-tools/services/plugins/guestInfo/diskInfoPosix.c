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
 * @file diskInfoPosix.c
 *
 * Contains POSIX-specific bits of gettting disk information.
 */

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
GuestInfo_GetDiskInfo(void)
{
   return GuestInfoGetDiskInfoWiper();
}

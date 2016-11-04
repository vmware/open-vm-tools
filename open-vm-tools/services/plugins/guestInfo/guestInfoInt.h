/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * Declares internal functions of the guestInfo plugin.
 */

#define G_LOG_DOMAIN "guestinfo"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"

#include "nicInfo.h"
#include "dynbuf.h"

/*
 * Internal stat IDs used by intermediate stats collected
 * for computing derived stats.
 *
 * NOTE: These IDs should not be published to the host as
 * these may change.
 */
#define GuestStatID_SwapSpaceUsed         ((GuestStatToolsID) (GuestStatID_Max + 0))
#define GuestStatID_SwapFilesCurrent      ((GuestStatToolsID) (GuestStatID_Max + 1))
#define GuestStatID_SwapFilesMax          ((GuestStatToolsID) (GuestStatID_Max + 2))
#define GuestStatID_Linux_LowWaterMark    ((GuestStatToolsID) (GuestStatID_Max + 3))
#define GuestStatID_Linux_MemAvailable    ((GuestStatToolsID) (GuestStatID_Max + 4))
#define GuestStatID_Linux_MemBuffers      ((GuestStatToolsID) (GuestStatID_Max + 5))
#define GuestStatID_Linux_MemCached       ((GuestStatToolsID) (GuestStatID_Max + 6))
#define GuestStatID_Linux_MemInactiveFile ((GuestStatToolsID) (GuestStatID_Max + 7))
#define GuestStatID_Linux_MemSlabReclaim  ((GuestStatToolsID) (GuestStatID_Max + 8))
#define GuestStatID_Linux_MemTotal        ((GuestStatToolsID) (GuestStatID_Max + 9))
#define GuestStatID_Linux_Internal_Max    ((GuestStatToolsID) (GuestStatID_Max + 10))

extern int guestInfoPollInterval;

Bool
GuestInfo_ServerReportStats(ToolsAppCtx *ctx,  // IN
                            DynBuf *stats);    // IN

gboolean
GuestInfo_StatProviderPoll(gpointer data);

GuestDiskInfo *
GuestInfoGetDiskInfoWiper(void);

GuestDiskInfo *
GuestInfo_GetDiskInfo(void);

void
GuestInfo_FreeDiskInfo(GuestDiskInfo *di);

#endif /* _GUESTINFOINT_H_ */


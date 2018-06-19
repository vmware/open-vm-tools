/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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

#ifndef _TIMESYNC_INT_H_
#define _TIMESYNC_INT_H_

/**
 * @file timeSync.h
 *
 * Functions and definitions related to syncing time.
 */

#define G_LOG_DOMAIN "timeSync"
#include "vm_basic_types.h"

#define US_PER_SEC 1000000

Bool
TimeSync_GetCurrentTime(int64 *now);

Bool
TimeSync_AddToCurrentTime(int64 delta);

Bool
TimeSync_Slew(int64 delta,
              int64 timeSyncPeriod,
              int64 *remaining);

Bool
TimeSync_DisableTimeSlew(void);

Bool
TimeSync_PLLUpdate(int64 offset);

Bool
TimeSync_PLLSetFrequency(int64 ppmCorrection);

Bool
TimeSync_PLLSupported(void);

Bool
TimeSync_IsGuestSyncServiceRunning(void);

Bool
TimeSync_DoGuestResync(void *_ctx);

#endif /* _TIMESYNC_INT_H_ */


/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

Bool
TimeSync_GetCurrentTime(int64 *secs,
                        int64 *usecs);

Bool
TimeSync_AddToCurrentTime(int64 deltaSecs,
                          int64 deltaUsecs);

Bool
TimeSync_EnableTimeSlew(int64 delta,
                        int64 timeSyncPeriod);

Bool
TimeSync_DisableTimeSlew(void);

Bool
TimeSync_IsTimeSlewEnabled(void);

#endif /* _TIMESYNC_INT_H_ */


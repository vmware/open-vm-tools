/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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

#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

/**
 * @file timesync.h
 *
 * Definitions related to the time sync functionality.
 */

#define TOOLSOPTION_SYNCTIME                        "synctime"
#define TOOLSOPTION_SYNCTIME_PERIOD                 "synctime.period"
#define TOOLSOPTION_SYNCTIME_ALLOW                \
   "time.synchronize.allow"
#define TOOLSOPTION_SYNCTIME_ENABLE               \
   "time.synchronize.tools.enable"
#define TOOLSOPTION_SYNCTIME_STARTUP_BACKWARD     \
   "time.synchronize.tools.startup.backward"
#define TOOLSOPTION_SYNCTIME_STARTUP              \
   "time.synchronize.tools.startup"
#define TOOLSOPTION_SYNCTIME_SLEWCORRECTION       \
   "time.synchronize.tools.slewCorrection"
#define TOOLSOPTION_SYNCTIME_PERCENTCORRECTION    \
   "time.synchronize.tools.percentCorrection"
#define TOOLSOPTION_SYNCTIME_GUEST_RESYNC         \
   "time.synchronize.guest.resync"
#define TOOLSOPTION_SYNCTIME_GUEST_RESYNC_TIMEOUT \
   "time.synchronize.guest.resync.timeout"

#define TIMESYNC_SYNCHRONIZE                    "Time_Synchronize"

#endif /* _TIMESYNC_H_ */


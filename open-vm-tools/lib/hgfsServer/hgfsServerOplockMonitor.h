/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * hgfsServerOplockMonitor.h --
 *
 *	Header file for public common data types used in the HGFS
 *	opportunistic lock monitoring subfeature.
 */

#ifndef _HGFS_SERVER_OPLOCK_MONITOR_H_
#define _HGFS_SERVER_OPLOCK_MONITOR_H_

#include "hgfsServerOplockInt.h"


/*
 * Data structures
 */

/*
 * Global variables
 */
#define HGFS_OPLOCK_INVALID_MONITOR_HANDLE 0


/*
 * Global functions
 */

Bool
HgfsOplockMonitorInit(void);
void
HgfsOplockMonitorDestroy(void);
HOM_HANDLE
HgfsOplockMonitorFileChange(char *utf8Name,
                            HgfsSessionInfo *session,
                            HgfsOplockCallback callback,
                            void *data);
void
HgfsOplockUnmonitorFileChange(HOM_HANDLE handle);
#endif // ifndef _HGFS_SERVER_OPLOCK_MONITOR_H_

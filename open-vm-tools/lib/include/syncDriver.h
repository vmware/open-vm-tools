/*********************************************************
 * Copyright (C) 2005-2018 VMware, Inc. All rights reserved.
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
 * syncDriver.h --
 *
 *    Interface to the Sync Driver.
 */

#ifndef _SYNC_DRIVER_H_
#define _SYNC_DRIVER_H_

#include "vm_basic_types.h"

#if defined(_WIN32) /* { */

# include <windows.h>
# define SYNCDRIVER_INVALID_HANDLE INVALID_HANDLE_VALUE
typedef HANDLE SyncDriverHandle;

#else /* }{ POSIX */

# define INFINITE -1
# define SYNCDRIVER_INVALID_HANDLE NULL

typedef struct SyncHandle * SyncDriverHandle;

#endif /* } */

typedef enum {
   SYNCDRIVER_IDLE,
   SYNCDRIVER_BUSY,
   SYNCDRIVER_ERROR
} SyncDriverStatus;

Bool SyncDriver_Init(void);
Bool SyncDriver_Freeze(const char *drives, Bool enableNullDriver,
                       SyncDriverHandle *handle,
                       const char *excludedFileSystems);
Bool SyncDriver_Thaw(const SyncDriverHandle handle);
SyncDriverStatus SyncDriver_QueryStatus(const SyncDriverHandle handle,
                                        int32 timeout);
void SyncDriver_CloseHandle(SyncDriverHandle *handle);
#if defined(__linux__)
void SyncDriver_GetAttr(const SyncDriverHandle handle, const char **name,
                        Bool *quiesces);
#endif

#endif


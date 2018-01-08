/*********************************************************
 * Copyright (C) 2011-2017 VMware, Inc. All rights reserved.
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

#ifndef _SYNCDRIVERINT_H_
#define _SYNCDRIVERINT_H_

/**
 * @file syncDriverInt.h
 *
 * Internal definitions for the sync driver library.
 */

#include <glib.h>
#include "syncDriver.h"

#define LGPFX "SyncDriver: "

#if !defined(Win32)

#define SYNCDRIVER_PATH_SEPARATOR   ':'

typedef enum {
   SD_SUCCESS,
   SD_ERROR,
   SD_UNAVAILABLE,
} SyncDriverErr;

typedef SyncDriverErr (*SyncFreezeFn)(const GSList *paths,
                                      SyncDriverHandle *handle);

typedef struct SyncHandle {
   SyncDriverErr (*thaw)(const SyncDriverHandle handle);
   void (*close)(SyncDriverHandle handle);
#if defined(__linux__)
   void (*getattr)(const SyncDriverHandle handle, const char **name,
                   Bool *quiesces);
#endif
} SyncHandle;

#if defined(__linux__)
SyncDriverErr
LinuxDriver_Freeze(const GSList *userPaths,
                   SyncDriverHandle *handle);

SyncDriverErr
VmSync_Freeze(const GSList *userPaths,
              SyncDriverHandle *handle);

SyncDriverErr
NullDriver_Freeze(const GSList *userPaths,
                  SyncDriverHandle *handle);
#endif

#endif

#endif /* _SYNCDRIVERINT_H_ */


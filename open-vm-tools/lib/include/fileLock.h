/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * fileLock.h --
 *
 *      Interface to file locking functions
 */

#ifndef _FILELOCK_H_
#define _FILELOCK_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "unicodeTypes.h"

// The default time, in msec, to wait for a lock before giving up
#define	FILELOCK_DEFAULT_WAIT 2500

// The wait time that provides "try lock" functionality
#define	FILELOCK_TRYLOCK_WAIT 0

// Wait "forever" to acquire the lock (maximum uint32)
#define	FILELOCK_INFINITE_WAIT 0xFFFFFFFF

/*
 * This is the maximum path length overhead that the file locking code
 * may introduce via all of its components.
 */

#define	FILELOCK_OVERHEAD 15

// File locking functions
typedef struct FileLockToken FileLockToken;

FileLockToken *FileLock_Lock(ConstUnicode filePath,
                             const Bool readOnly,
                             const uint32 msecMaxWaitTime,
                             int *err);

Unicode FileLock_TokenPathName(const FileLockToken *fileLockToken);
int FileLock_Unlock(const FileLockToken *lockToken);

Bool FileLock_IsLocked(ConstUnicode filePath,
                       int *err);

int FileLock_Remove(ConstUnicode filePath);
int FileLock_CleanupVM(ConstUnicode cfgfilePath);

// Device locking functions, for compatibility
int FileLock_LockDevice(const char *device);
Bool FileLock_UnlockDevice(const char *device);

#endif // ifndef _FILELOCK_H_

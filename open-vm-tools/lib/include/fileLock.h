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

#if !defined(_WIN32)
/*
 * Set the file type config variables for linux.
 */
EXTERN void FileLock_Init(int lockerPid, Bool userWorld);
#endif

// Horrible hack that exists to please VMX; should be removed ASAP
EXTERN int  FileLock_DeleteFileVMX(ConstUnicode filePath);

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
EXTERN void *FileLock_Lock(ConstUnicode filePath,
                           const Bool readOnly,
                           const uint32 msecMaxWaitTime,
                           int *err);

EXTERN int FileLock_Unlock(ConstUnicode filePath,
                           const void *fileLockToken);

EXTERN int FileLock_Remove(ConstUnicode filePath);
EXTERN int FileLock_CleanupVM(ConstUnicode cfgfilePath);

// Device locking functions, for compatibility
EXTERN int FileLock_LockDevice(const char *device);
EXTERN Bool FileLock_UnlockDevice(const char *device);

#endif // ifndef _FILELOCK_H_

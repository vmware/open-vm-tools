/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
#include "msgList.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* The default time, in milliseconds, to wait for a lock before giving up */
#define	FILELOCK_DEFAULT_WAIT (vmx86_server ? 7000 : 3500)

/* The wait time that provides "try lock" functionality */
#define	FILELOCK_TRYLOCK_WAIT 0

/* Wait "forever" to acquire the lock (maximum uint32) */
#define	FILELOCK_INFINITE_WAIT 0xFFFFFFFF

/*
 * This is the maximum path length overhead that the file locking code
 * may introduce via all of its components.
 */

#define	FILELOCK_OVERHEAD 15

/* File locking functions */
typedef struct FileLockToken FileLockToken;

char *FileLock_TokenPathName(const FileLockToken *fileLockToken);

FileLockToken *FileLock_Lock(const char *filePath,
                             const Bool readOnly,
                             const uint32 maxWaitTimeMsec,
                             int *err,
                             MsgList **msgs);

Bool FileLock_Unlock(const FileLockToken *lockToken,
                     int *err,
                     MsgList **msgs);

Bool FileLock_IsLocked(const char *filePath,
                       int *err,
                       MsgList **msgs);

Bool FileLock_Remove(const char *filePath,
                     int *err,
                     MsgList **msgs);

Bool FileLock_CleanupVM(const char *cfgfilePath,
                        int *err,
                        MsgList **msgs);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _FILELOCK_H_

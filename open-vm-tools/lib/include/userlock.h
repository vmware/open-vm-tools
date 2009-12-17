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

#ifndef _USERLOCK_H_
#define _USERLOCK_H_

#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_atomic.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"

/*
 * Statistics interfaces will come soon.
 *
 * Each lock is expected to keep:
 *   - Min
 *   - Max
 *   - Mean
 *   - S.D.
 *   - Histogram (log2?)
 *
 * for time-till-acquistion and time-locked.
 */

typedef struct MXUserExclLock   MXUserExclLock;
typedef struct MXUserRecLock    MXUserRecLock;
typedef struct MXUserRWLock     MXUserRWLock;

/*
 * Exclusive ownership lock
 */

MXUserExclLock *
MXUser_CreateExclLock(const char *name,
                    MX_Rank rank);

void MXUser_AcquireExclLock(MXUserExclLock *lock);
Bool MXUser_TryAcquireExclLock(MXUserExclLock *lock);
void MXUser_ReleaseExclLock(MXUserExclLock *lock);
void MXUser_DestroyExclLock(MXUserExclLock *lock);
Bool MXUser_IsLockedByCurThreadExclLock(const MXUserExclLock *lock);

MXUserExclLock *
MXUser_CreateSingletonExclLock(Atomic_Ptr *lockStorage,
                               const char *name,
                               MX_Rank rank);

/*
 * Recursive lock.
 */

MXUserRecLock *
MXUser_CreateRecLock(const char *name,
                     MX_Rank rank);

void MXUser_AcquireRecLock(MXUserRecLock *lock);
Bool MXUser_TryAcquireRecLock(MXUserRecLock *lock);
void MXUser_ReleaseRecLock(MXUserRecLock *lock);
void MXUser_DestroyRecLock(MXUserRecLock *lock);
Bool MXUser_IsLockedByCurThreadRecLock(const MXUserRecLock *lock);

MXUserRecLock *
MXUser_CreateSingletonRecLock(Atomic_Ptr *lockStorage,
                              const char *name,
                              MX_Rank rank);

/*
 * Eventually there will be an API to bind an recursive MX lock to a
 * MXUser recursive lock. This will allow such MXUser recursive locks
 * to be used for Poll.
 */

/*
 * Read-write lock
 */

MXUserRWLock *
MXUser_CreateRWLock(const char *name,
                    MX_Rank rank);

void MXUser_AcquireForRead(MXUserRWLock *lock);
void MXUser_AcquireForWrite(MXUserRWLock *lock);
void MXUser_ReleaseRWLock(MXUserRWLock *lock);
void MXUser_DestroyRWLock(MXUserRWLock *lock);
#endif  // _USERLOCK_H_

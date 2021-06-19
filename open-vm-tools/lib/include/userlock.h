/*********************************************************
 * Copyright (C) 2009-2020 VMware, Inc. All rights reserved.
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

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdarg.h>

#include "vm_atomic.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "mutexRank.h"
#include "vthreadBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct MXUserExclLock   MXUserExclLock;
typedef struct MXUserRecLock    MXUserRecLock;
typedef struct MXUserRWLock     MXUserRWLock;
typedef struct MXUserRankLock   MXUserRankLock;
typedef struct MXUserCondVar    MXUserCondVar;
typedef struct MXUserSemaphore  MXUserSemaphore;
typedef struct MXUserEvent      MXUserEvent;
typedef struct MXUserBarrier    MXUserBarrier;

/*
 * Exclusive ownership lock
 */

MXUserExclLock *MXUser_CreateExclLock(const char *name,
                                      MX_Rank rank);

void MXUser_AcquireExclLock(MXUserExclLock *lock);
Bool MXUser_TryAcquireExclLock(MXUserExclLock *lock);
void MXUser_ReleaseExclLock(MXUserExclLock *lock);
void MXUser_DestroyExclLock(MXUserExclLock *lock);
Bool MXUser_IsCurThreadHoldingExclLock(MXUserExclLock *lock);

/* Use only when necessary */
MXUserExclLock *MXUser_CreateSingletonExclLockInt(Atomic_Ptr *lockStorage,
                                                  const char *name,
                                                  MX_Rank rank);

/* This is the public interface */
static INLINE MXUserExclLock *
MXUser_CreateSingletonExclLock(Atomic_Ptr *lockStorage,
                               const char *name,
                               MX_Rank rank)
{
   MXUserExclLock *lock;

   ASSERT(lockStorage);

   lock = (MXUserExclLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      lock = MXUser_CreateSingletonExclLockInt(lockStorage, name, rank);
   }

   return lock;
}

MXUserCondVar *MXUser_CreateCondVarExclLock(MXUserExclLock *lock);

void MXUser_WaitCondVarExclLock(MXUserExclLock *lock,
                                MXUserCondVar *condVar);

void MXUser_TimedWaitCondVarExclLock(MXUserExclLock *lock,
                                     MXUserCondVar *condVar,
                                     uint32 waitTimeMS);

Bool MXUser_EnableStatsExclLock(MXUserExclLock *lock,
                                Bool trackAcquisitionTime,
                                Bool trackHeldTime);

Bool MXUser_DisableStatsExclLock(MXUserExclLock *lock);

Bool MXUser_SetContentionRatioFloorExclLock(MXUserExclLock *lock,
                                            double ratio);

Bool MXUser_SetContentionCountFloorExclLock(MXUserExclLock *lock,
                                            uint64 count);

Bool MXUser_SetContentionDurationFloorExclLock(MXUserExclLock *lock,
                                               uint64 count);

/*
 * Recursive lock.
 */

MXUserRecLock *MXUser_CreateRecLock(const char *name,
                                    MX_Rank rank);

void MXUser_AcquireRecLock(MXUserRecLock *lock);
Bool MXUser_TryAcquireRecLock(MXUserRecLock *lock);
void MXUser_ReleaseRecLock(MXUserRecLock *lock);
void MXUser_DestroyRecLock(MXUserRecLock *lock);
Bool MXUser_IsCurThreadHoldingRecLock(MXUserRecLock *lock);

/* Use only when necessary */
MXUserRecLock *MXUser_CreateSingletonRecLockInt(Atomic_Ptr *lockStorage,
                                                const char *name,
                                                MX_Rank rank);

/* This is the public interface */
static INLINE MXUserRecLock *
MXUser_CreateSingletonRecLock(Atomic_Ptr *lockStorage,
                              const char *name,
                              MX_Rank rank)
{
   MXUserRecLock *lock;

   ASSERT(lockStorage);

   lock = (MXUserRecLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      lock = MXUser_CreateSingletonRecLockInt(lockStorage, name, rank);
   }

   return lock;
}

void MXUser_DumpRecLock(MXUserRecLock *lock);

MXUserCondVar *MXUser_CreateCondVarRecLock(MXUserRecLock *lock);

void MXUser_WaitCondVarRecLock(MXUserRecLock *lock,
                               MXUserCondVar *condVar);

void MXUser_TimedWaitCondVarRecLock(MXUserRecLock *lock,
                                    MXUserCondVar *condVar,
                                    uint32 waitTimeMS);

void MXUser_IncRefRecLock(MXUserRecLock *lock);

void MXUser_DecRefRecLock(MXUserRecLock *lock);

Bool MXUser_EnableStatsRecLock(MXUserRecLock *lock,
                               Bool trackAcquisitionTime,
                               Bool trackHeldTime);

Bool MXUser_DisableStatsRecLock(MXUserRecLock *lock);

Bool MXUser_SetContentionRatioFloorRecLock(MXUserRecLock *lock,
                                           double ratio);

Bool MXUser_SetContentionCountFloorRecLock(MXUserRecLock *lock,
                                           uint64 count);

Bool MXUser_SetContentionDurationFloorRecLock(MXUserRecLock *lock,
                                              uint64 count);


/*
 * Read-write lock
 */

MXUserRWLock *MXUser_CreateRWLock(const char *name,
                                   MX_Rank rank);

void MXUser_AcquireForRead(MXUserRWLock *lock);
void MXUser_AcquireForWrite(MXUserRWLock *lock);
void MXUser_ReleaseRWLock(MXUserRWLock *lock);
void MXUser_DestroyRWLock(MXUserRWLock *lock);

#define MXUSER_RW_FOR_READ   0
#define MXUSER_RW_FOR_WRITE  1
#define MXUSER_RW_LOCKED     2

Bool MXUser_IsCurThreadHoldingRWLock(MXUserRWLock *lock,
                                     uint32 queryType);

/* Use only when necessary */
MXUserRWLock *MXUser_CreateSingletonRWLockInt(Atomic_Ptr *lockStorage,
                                              const char *name,
                                              MX_Rank rank);

/* This is the public interface */
static INLINE MXUserRWLock *
MXUser_CreateSingletonRWLock(Atomic_Ptr *lockStorage,
                             const char *name,
                             MX_Rank rank)
{
   MXUserRWLock *lock;

   ASSERT(lockStorage);

   lock = (MXUserRWLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      lock = MXUser_CreateSingletonRWLockInt(lockStorage, name, rank);
   }

   return lock;
}

/*
 * Stateful auto-reset event
 */

MXUserEvent *MXUser_CreateEvent(const char *name,
                                MX_Rank rank);

void MXUser_SignalEvent(MXUserEvent *event);
void MXUser_WaitEvent(MXUserEvent *event);
Bool MXUser_TryWaitEvent(MXUserEvent *event);
PollDevHandle MXUser_GetHandleForEvent(MXUserEvent *event);
void MXUser_DestroyEvent(MXUserEvent *event);

MXUserEvent *MXUser_CreateSingletonEvent(Atomic_Ptr *eventStorage,
                                         const char *name,
                                         MX_Rank rank);

/*
 * Computational barrier
 */

MXUserBarrier *MXUser_CreateBarrier(const char *name,
                                    MX_Rank rank,
                                    uint32 count);

void MXUser_DestroyBarrier(MXUserBarrier *barrier);
void MXUser_EnterBarrier(MXUserBarrier *barrier);

MXUserBarrier *MXUser_CreateSingletonBarrier(Atomic_Ptr *barrierStorage,
                                             const char *name,
                                             MX_Rank rank,
                                             uint32 count);

/*
 * Counting semaphore
 */

MXUserSemaphore *MXUser_CreateSemaphore(const char *name,
                                        MX_Rank rank);

void MXUser_DestroySemaphore(MXUserSemaphore *sema);
void MXUser_UpSemaphore(MXUserSemaphore *sema);
void MXUser_DownSemaphore(MXUserSemaphore *sema);
Bool MXUser_TryDownSemaphore(MXUserSemaphore *sema);

Bool MXUser_TimedDownSemaphore(MXUserSemaphore *sema,
                               uint32 waitTimeMS);
Bool MXUser_TimedDownSemaphoreNS(MXUserSemaphore *sema,
                                 uint64 waitTimeNS);

MXUserSemaphore *MXUser_CreateSingletonSemaphore(Atomic_Ptr *semaStorage,
                                                 const char *name,
                                                 MX_Rank rank);

/*
 * Rank lock
 *
 * Rank "locks" are entities that perform rank checking but do not provide
 * any form of mutual exclusion. Their main use is for protecting certain
 * situations involving Poll and friends/enemies.
 */

MXUserRankLock *MXUser_CreateRankLock(const char *name,
                                      MX_Rank rank);

void MXUser_AcquireRankLock(MXUserRankLock *lock);
void MXUser_ReleaseRankLock(MXUserRankLock *lock);
void MXUser_DestroyRankLock(MXUserRankLock *lock);

/*
 * Generic conditional variable functions.
 */

#define MXUSER_WAIT_INFINITE 0xFFFFFFFF

void MXUser_SignalCondVar(MXUserCondVar *condVar);
void MXUser_BroadcastCondVar(MXUserCondVar *condVar);
void MXUser_DestroyCondVar(MXUserCondVar *condVar);

void MXUser_LockingTreeCollection(Bool enabled);
Bool MXUser_IsLockingTreeAvailable(void);
void MXUser_DumpLockTree(const char *fileName,
                         const char *timeStamp);

void MXUser_EmptyLockTree(void);


#if defined(VMX86_DEBUG) && !defined(DISABLE_MXUSER_DEBUG)
#define MXUSER_DEBUG  // debugging "everywhere" when requested
#endif

#if defined(MXUSER_DEBUG)
void MXUser_TryAcquireFailureControl(Bool (*func)(const char *lockName));
Bool MXUser_IsCurThreadHoldingLocks(void);
#endif
void MXUser_StatisticsControl(double contentionRatioFloor,
                              uint64 minAccessCountFloor,
                              uint64 contentionDurationFloor);

void MXUser_PerLockData(void);
void MXUser_SetStatsFunc(void *context,
                         uint32 maxLineLength,
                         Bool trackHeldTime,
                         void (*statsFunc)(void *context,
                                           const char *fmt,
                                           va_list ap));

void MXUser_SetInPanic(void);
Bool MXUser_InPanic(void);

struct MX_MutexRec;

MXUserRecLock       *MXUser_BindMXMutexRec(struct MX_MutexRec *mutex,
                                           MX_Rank rank);

struct MX_MutexRec  *MXUser_GetRecLockVmm(MXUserRecLock *lock);
MX_Rank              MXUser_GetRecLockRank(MXUserRecLock *lock);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // _USERLOCK_H_

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


#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdarg.h>

#include "vm_atomic.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "mutexRank.h"
#include "vthreadBase.h"

typedef struct MXUserExclLock   MXUserExclLock;
typedef struct MXUserRecLock    MXUserRecLock;
typedef struct MXUserRWLock     MXUserRWLock;
typedef struct MXUserRankLock   MXUserRankLock;
typedef struct MXUserCondVar    MXUserCondVar;
typedef struct MXUserSemaphore  MXUserSemaphore;
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
Bool MXUser_IsCurThreadHoldingExclLock(const MXUserExclLock *lock);

MXUserExclLock *MXUser_CreateSingletonExclLock(Atomic_Ptr *lockStorage,
                                               const char *name,
                                               MX_Rank rank);

Bool MXUser_ControlExclLock(MXUserExclLock *lock,
                            uint32 command,
                            ...);

MXUserCondVar *MXUser_CreateCondVarExclLock(MXUserExclLock *lock);

void MXUser_WaitCondVarExclLock(MXUserExclLock *lock,
                                MXUserCondVar *condVar);

void MXUser_TimedWaitCondVarExclLock(MXUserExclLock *lock,
                                     MXUserCondVar *condVar,
                                     uint32 msecWait);


/*
 * Recursive lock.
 */

MXUserRecLock *MXUser_CreateRecLock(const char *name,
                                    MX_Rank rank);

MXUserRecLock *MXUser_CreateRecLockSilent(const char *name,
                                          MX_Rank rank);

void MXUser_AcquireRecLock(MXUserRecLock *lock);
Bool MXUser_TryAcquireRecLock(MXUserRecLock *lock);
void MXUser_ReleaseRecLock(MXUserRecLock *lock);
void MXUser_DestroyRecLock(MXUserRecLock *lock);
Bool MXUser_IsCurThreadHoldingRecLock(const MXUserRecLock *lock);

MXUserRecLock *MXUser_CreateSingletonRecLock(Atomic_Ptr *lockStorage,
                                             const char *name,
                                             MX_Rank rank);

Bool MXUser_ControlRecLock(MXUserRecLock *lock,
                           uint32 command,
                           ...);

void MXUser_DumpRecLock(MXUserRecLock *lock);

MXUserCondVar *MXUser_CreateCondVarRecLock(MXUserRecLock *lock);

void MXUser_WaitCondVarRecLock(MXUserRecLock *lock,
                               MXUserCondVar *condVar);

void MXUser_TimedWaitCondVarRecLock(MXUserRecLock *lock,
                                    MXUserCondVar *condVar,
                                    uint32 msecWait);

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

MXUserRWLock *MXUser_CreateSingletonRWLock(Atomic_Ptr *lockStorage,
                                           const char *name,
                                           MX_Rank rank);

Bool MXUser_ControlRWLock(MXUserRWLock *lock,
                          uint32 command,
                          ...);

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
                               uint32 msecWait);

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

/*
 * MXUser_Control[Excl, Rec, RW] commands
 */

#define MXUSER_CONTROL_ACQUISITION_HISTO   0     // minValue, decades
#define MXUSER_CONTROL_HELD_HISTO          1     // minValue, decades
#define MXUSER_CONTROL_ENABLE_STATS        2     // no arguments

#define MXUSER_DEFAULT_HISTO_MIN_VALUE_NS  1000  // 1 usec
#define MXUSER_DEFAULT_HISTO_DECADES       7     // 1 usec to 10 seconds

struct MX_MutexRec;

#if defined(VMX86_VMX)
MXUserRecLock *MXUser_InitFromMXRec(const char *name,
                                    struct MX_MutexRec *mutex,
                                    MX_Rank rank,
                                    Bool isBelowBull);

#endif

#if defined(VMX86_DEBUG) && !defined(DISABLE_MXUSER_DEBUG)
#define MXUSER_DEBUG  // debugging "everywhere" when requested
#endif

#if defined(MXUSER_DEBUG)
void MXUser_TryAcquireFailureControl(Bool (*func)(const char *lockName));
Bool MXUser_IsCurThreadHoldingLocks(void);
#endif

void MXUser_StatisticsControl(double contentionRatio,
                              uint64 minCount);

void MXUser_PerLockData(void);
void MXUser_SetStatsFunc(void *context,
                         uint32 maxLineLength,
                         void (*statsFunc)(void *context,
                                           const char *fmt,
                                           va_list ap));

void MXUser_SetInPanic(void);
Bool MXUser_InPanic(void);

MXUserRecLock       *MXUser_BindMXMutexRec(struct MX_MutexRec *mutex,
                                           MX_Rank rank);
struct MX_MutexRec  *MXUser_GetRecLockVmm(const MXUserRecLock *lock);
MX_Rank              MXUser_GetRecLockRank(const MXUserRecLock *lock);
#endif  // _USERLOCK_H_

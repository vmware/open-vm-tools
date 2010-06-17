/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "vmware.h"
#include "str.h"
#include "err.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"

/*
 * A portable condition variable
 */

struct MXUserCondVar {
   uint32             signature;
   char              *name;
   MXRecLock         *ownerLock;
   Atomic_uint32      referenceCount;

#if defined(_WIN32)
   union {
      struct {
         CRITICAL_SECTION   condVarLock;
         HANDLE             signalEvent;
         uint32             numWaiters;
         uint32             numForRelease;
      } compat;
      CONDITION_VARIABLE    condObject;
   } x;
#else
   pthread_cond_t     condObject;
#endif
};

#define MXUSER_CONDVAR_SIGNATURE 0x444E4F43 // 'COND' in memory

#if defined(_WIN32)
typedef VOID (WINAPI *InitializeConditionVariableFn)(PCONDITION_VARIABLE cv);
typedef BOOL (WINAPI *SleepConditionVariableCSFn)(PCONDITION_VARIABLE cv,
                                                  PCRITICAL_SECTION cs,
                                                  DWORD msSleep);
typedef VOID (WINAPI *WakeAllConditionVariableFn)(PCONDITION_VARIABLE cv);
typedef VOID (WINAPI *WakeConditionVariableFn)(PCONDITION_VARIABLE cv);

static InitializeConditionVariableFn  pInitializeConditionVariable;
static SleepConditionVariableCSFn     pSleepConditionVariableCS;
static WakeAllConditionVariableFn     pWakeAllConditionVariable;
static WakeConditionVariableFn        pWakeConditionVariable;


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserNativeCVSupported --
 *
 *      Does native condition variable support exist for the Windows the
 *      caller is running on?
 *
 * Results:
 *      TRUE   Yes
 *      FALSE  No
 *
 * Side effects:
 *      Function pointers to the native routines are initialized upon success.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
MXUserNativeCVSupported(void)
{
   static Bool result;
   static Bool done = FALSE;

   if (!done) {
      HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");

      if (kernel32) {
         pInitializeConditionVariable = (InitializeConditionVariableFn)
                                        GetProcAddress(kernel32,
                                             "InitializeConditionVariable");

         pSleepConditionVariableCS = (SleepConditionVariableCSFn)
                                     GetProcAddress(kernel32,
                                                "SleepConditionVariableCS");

         pWakeAllConditionVariable = (WakeAllConditionVariableFn)
                                     GetProcAddress(kernel32,
                                                "WakeAllConditionVariable");

         pWakeConditionVariable = (WakeConditionVariableFn)
                                   GetProcAddress(kernel32,
                                                   "WakeConditionVariable");

         result = ((pInitializeConditionVariable == NULL) ||
                   (pSleepConditionVariableCS == NULL) ||
                   (pWakeAllConditionVariable == NULL) ||
                   (pWakeConditionVariable == NULL)) ? FALSE : TRUE;

      } else {
         result = FALSE;
      }

      done = TRUE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCreateInternal --
 *
 *      Create/initialize the environmentally specific portion of an
 *      MXUserCondVar.
 *
 * Results:
 *      TRUE   Success
 *      FALSE  Failure
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
MXUserCreateInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   Bool success;

   if (MXUserNativeCVSupported()) {
      ASSERT(pInitializeConditionVariable);
      (*pInitializeConditionVariable)(&condVar->x.condObject);
      success = TRUE;
   } else {
      if (InitializeCriticalSectionAndSpinCount(&condVar->x.compat.condVarLock,
                                             0x80000400) == 0) {
         success = FALSE;
      } else {
         condVar->x.compat.numWaiters = 0;
         condVar->x.compat.numForRelease = 0;

         condVar->x.compat.signalEvent = CreateEvent(NULL,  // no security
                                                     TRUE,  // manual-reset
                                                     FALSE, // non-signaled
                                                     NULL); // unnamed

         success = (condVar->x.compat.signalEvent != NULL);

         if (!success) {
            DeleteCriticalSection(&condVar->x.compat.condVarLock);
         }
      }
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDestroyInternal --
 *
 *      Destroy the environmentally specific portion of an MXUserCondVar.
 *
 * Results:
 *      As expect.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserDestroyInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   if (pInitializeConditionVariable == NULL) {
      DeleteCriticalSection(&condVar->x.compat.condVarLock);
      CloseHandle(condVar->x.compat.signalEvent);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserWaitInternal --
 *
 *      Perform the environmentally specific portion of a wait on an
 *      MXUserCondVar.
 *
 * Results:
 *      0   No error AND *signalled may be:
 *             TRUE   condVar was signalled
 *             FALSE  timed out waiting for condVar
 *      !0  Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
MXUserWaitInternal(MXRecLock *lock,         // IN:
                   MXUserCondVar *condVar,  // IN:
                   uint32 msecWait,         // IN: unused
                   Bool *signalled)         // OUT:
{
   int err;

   /*
    * When using the native lock found within the MXUser lock, be sure to
    * decrement the count before the wait/sleep and increment it after the
    * wait/sleep - the (native) wait/sleep will perform a lock release before
    * the wait/sleep and a lock acquisition after the wait/sleep. The
    * MXUser internal accounting information must be maintained.
    */

   if (pSleepConditionVariableCS) {
      MXRecLockDecCount(lock);
      err = (*pSleepConditionVariableCS)(&condVar->x.condObject,
                                         &lock->nativeLock, INFINITE) ?
                                         0 : GetLastError();
      MXRecLockIncCount(lock, GetReturnAddress());
   } else {
      Bool done = FALSE;

      EnterCriticalSection(&condVar->x.compat.condVarLock);
      condVar->x.compat.numWaiters++;
      LeaveCriticalSection(&condVar->x.compat.condVarLock);

      MXRecLockRelease(lock);

      do {
         WaitForSingleObject(condVar->x.compat.signalEvent, INFINITE);

         EnterCriticalSection(&condVar->x.compat.condVarLock);

         ASSERT(condVar->x.compat.numWaiters > 0);

         if (condVar->x.compat.numForRelease > 0) {
            condVar->x.compat.numWaiters--;

            if (--condVar->x.compat.numForRelease == 0) {
               ResetEvent(condVar->x.compat.signalEvent);
            }

            done = TRUE;
         }

         LeaveCriticalSection(&condVar->x.compat.condVarLock);
      } while (!done);

      MXRecLockAcquire(lock, GetReturnAddress());

      err = 0;
   }

   *signalled = TRUE;

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserSignalInternal --
 *
 *      Perform the environmentally specific portion of signalling an
 *      MXUserCondVar.
 *
 * Results:
 *      0    Success
 *      !0   Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
MXUserSignalInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   if (pWakeConditionVariable) {
      (*pWakeConditionVariable)(&condVar->x.condObject);
   } else {
      EnterCriticalSection(&condVar->x.compat.condVarLock);

      if (condVar->x.compat.numWaiters > condVar->x.compat.numForRelease) {
         SetEvent(condVar->x.compat.signalEvent);
         condVar->x.compat.numForRelease++;
      }

      LeaveCriticalSection(&condVar->x.compat.condVarLock);
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserBroadcastInternal --
 *
 *      Perform the environmentally specific portion of broadasting on an
 *      MXUserCondVar.
 *
 * Results:
 *      0    Success
 *      !0   Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */


static INLINE int
MXUserBroadcastInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   if (pWakeAllConditionVariable) {
      (*pWakeAllConditionVariable)(&condVar->x.condObject);
   } else {
      EnterCriticalSection(&condVar->x.compat.condVarLock);

      if (condVar->x.compat.numWaiters > condVar->x.compat.numForRelease) {
         SetEvent(condVar->x.compat.signalEvent);
         condVar->x.compat.numForRelease = condVar->x.compat.numWaiters;
      }

      LeaveCriticalSection(&condVar->x.compat.condVarLock);
   }

   return 0;
}
#else
/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCreateInternal --
 *
 *      Create/initialize the environmentally specific portion of an
 *      MXUserCondVar.
 *
 * Results:
 *      TRUE   Success
 *      FALSE  Failure
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
MXUserCreateInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   return pthread_cond_init(&condVar->condObject, NULL) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDestroyInternal --
 *
 *      Destroy the environmentally specific portion of an MXUserCondVar.
 *
 * Results:
 *      As expected.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserDestroyInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   pthread_cond_destroy(&condVar->condObject);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserWaitInternal --
 *
 *      Perform the environmentally specific portion of a wait on an
 *      MXUserCondVar.
 *
 * Results:
 *      0   Success AND *signalled may be:
 *             TRUE   condVar was signalled
 *             FALSE  timed out waiting for condVar
 *      !0  Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
MXUserWaitInternal(MXRecLock *lock,         // IN:
                   MXUserCondVar *condVar,  // IN:
                   uint32 msecWait,         // IN:
                   Bool *signalled)         // OUT:
{
   int err;

   /*
    * When using the native lock found within the MXUser lock, be sure to
    * decrement the count before the wait/sleep and increment it after the
    * wait/sleep - the (native) wait/sleep will perform a lock release before
    * the wait/sleep and a lock acquisition after the wait/sleep. The
    * MXUser internal accounting information must be maintained.
    */

   MXRecLockDecCount(lock);
   err = pthread_cond_wait(&condVar->condObject, &lock->nativeLock);
   MXRecLockIncCount(lock, GetReturnAddress());

   *signalled = TRUE;

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserSignalInternal --
 *
 *      Perform the environmentally specific portion of signalling an
 *      MXUserCondVar.
 *
 * Results:
 *      0    Success
 *      !0   Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
MXUserSignalInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   return pthread_cond_signal(&condVar->condObject);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserBroadcaseInternal --
 *
 *      Perform the environmentally specific portion of broadasting on an
 *      MXUserCondVar.
 *
 * Results:
 *      0    Success
 *      !0   Error
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
MXUserBroadcastInternal(MXUserCondVar *condVar)  // IN/OUT:
{
   return pthread_cond_broadcast(&condVar->condObject);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCreateCondVar --
 *
 *      Create/initialize a condition variable associated with the specified
 *      lock.
 *
 * Results:
 *      !NULL  Success; a pointer to the (new) condition variable
 *      NULL   Failure
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

MXUserCondVar *
MXUserCreateCondVar(MXUserHeader *header,  // IN:
                    MXRecLock *lock)       // IN:
{
   MXUserCondVar *condVar = Util_SafeCalloc(1, sizeof(*condVar));

   if (MXUserCreateInternal(condVar)) {
      condVar->signature = MXUSER_CONDVAR_SIGNATURE;
      condVar->name = Util_SafeStrdup(header->lockName);
      condVar->ownerLock = lock;
   } else {
      free(condVar);
      condVar = NULL;
   }

   return condVar;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserWaitCondVar --
 *
 *      The internal wait on a condition variable routine.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      An attempt to use a lock other than the one the specified condition
 *      variable was specified for will result in a panic.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserWaitCondVar(MXUserHeader *header,    // IN:
                  MXRecLock *lock,         // IN:
                  MXUserCondVar *condVar)  // IN/OUT:
{
   int err;
   Bool signalled;

   ASSERT(header);
   ASSERT(lock);
   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

   if (condVar->ownerLock != lock) {
      MXUserDumpAndPanic(header,
                         "%s: invalid use of lock %s with condVar (%p; %s)\n",
                         __FUNCTION__, header->lockName, condVar->name);
   }

   if (MXRecLockCount(lock) == 0) {
      MXUserDumpAndPanic(header,
                         "%s: unlocked lock %s with condVar (%p; %s)\n",
                         __FUNCTION__, header->lockName, condVar->name);
   }

   Atomic_Inc(&condVar->referenceCount);

   err = MXUserWaitInternal(lock, condVar, 0, &signalled);

   if (err != 0) {
      MXUserDumpAndPanic(header, "%s: failure %d on condVar (%p; %s)\n",
                         __FUNCTION__, err, condVar, condVar->name);
   }

   Atomic_Dec(&condVar->referenceCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_SignalCondVar
 *
 *      Signal on a condVar - wake up one thread blocked on the specified
 *      condition variable.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_SignalCondVar(MXUserCondVar *condVar)  // IN:
{
   int err;

   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

   err = MXUserSignalInternal(condVar);

   if (err != 0) {
      Panic("%s: failure %d on condVar (%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->name);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_BroadcastCondVar
 *
 *      Broadcast on a condVar - wake up all threads blocked on the specified
 *      condition variable.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_BroadcastCondVar(MXUserCondVar *condVar)  // IN:
{
   int err;

   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

   err = MXUserBroadcastInternal(condVar);

   if (err != 0) {
      Panic("%s: failure %d on condVar (%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->name);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyCondVar --
 *
 *      Destroy a condition variable.
 *
 * Results:
 *      As above. Don't use the pointer again...
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyCondVar(MXUserCondVar *condVar)  // IN:
{
   if (condVar != NULL) {
      ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

      if (Atomic_Read(&condVar->referenceCount) != 0) {
         Panic("%s: Attempted destroy on active condVar (%p; %s)\n",
               __FUNCTION__, condVar, condVar->name);
      }

      MXUserDestroyInternal(condVar);

      condVar->signature = 0;  // just in case...
      free(condVar->name);
      condVar->name = NULL;
      condVar->ownerLock = NULL;

      free(condVar);
   }
}

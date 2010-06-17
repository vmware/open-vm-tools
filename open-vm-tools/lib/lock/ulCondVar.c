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
         MXRecLock       condVarLock;
         HANDLE          signalEvent;
         uint32          numWaiters;
         uint32          numForRelease;
      } compat;
      CONDITION_VARIABLE condObject;
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
   Bool success;
   MXUserCondVar *condVar = Util_SafeCalloc(1, sizeof(*condVar));

#if defined(_WIN32)
   if (MXUserNativeCVSupported()) {
      ASSERT(pInitializeConditionVariable);
      (*pInitializeConditionVariable)(&condVar->x.condObject);
      success = TRUE;
   } else {
      condVar->x.compat.numWaiters = 0;
      condVar->x.compat.numForRelease = 0;

      if (MXRecLockInit(&condVar->x.compat.condVarLock)) {
         condVar->x.compat.signalEvent = CreateEvent(NULL,  // no security
                                                     TRUE,  // manual-reset
                                                     FALSE, // non-signaled
                                                     NULL); // unnamed

         success = (condVar->x.compat.signalEvent != NULL);

         if (!success) {
            MXRecLockDestroy(&condVar->x.compat.condVarLock);
         }
      } else {
         success = FALSE;
      }
   }
#else
   success = pthread_cond_init(&condVar->condObject, NULL) == 0;
#endif

   if (success) {
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
                  MXUserCondVar *condVar)  // IN:
{
   int err;

   ASSERT(header);
   ASSERT(lock);
   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

   if (condVar->ownerLock != lock) {
      MXUserDumpAndPanic(header,
                         "%s: invalid use of lock %s with condVar (%p; %s)\n",
                         __FUNCTION__, header->lockName, condVar->name);
   }

   Atomic_Inc(&condVar->referenceCount);

#if defined(_WIN32)
   if (pSleepConditionVariableCS) {
      err = (*pSleepConditionVariableCS)(&condVar->x.condObject,
                                         &lock->nativeLock, INFINITE) ?
                                         0 : GetLastError();
   } else {
      Bool done = FALSE;

      MXRecLockAcquire(&condVar->x.compat.condVarLock, GetReturnAddress());
      condVar->x.compat.numWaiters++;
      MXRecLockRelease(&condVar->x.compat.condVarLock);

      MXRecLockRelease(lock);

      do {
         WaitForSingleObject(condVar->x.compat.signalEvent, INFINITE);

         MXRecLockAcquire(&condVar->x.compat.condVarLock, GetReturnAddress());

         ASSERT(condVar->x.compat.numWaiters > 0);

         if (condVar->x.compat.numForRelease > 0) {
            condVar->x.compat.numWaiters--;

            if (--condVar->x.compat.numForRelease == 0) {
               ResetEvent(condVar->x.compat.signalEvent);
            }

            done = TRUE;
         }

         MXRecLockRelease(&condVar->x.compat.condVarLock);
      } while (!done);

      MXRecLockAcquire(lock, GetReturnAddress());

      err = 0;
   }
#else
   err = pthread_cond_wait(&condVar->condObject, &lock->nativeLock);
#endif

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
#if !defined(_WIN32)
   int err;
#endif

   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

#if defined(_WIN32)
   if (pWakeConditionVariable) {
      (*pWakeConditionVariable)(&condVar->x.condObject);
   } else {
      MXRecLockAcquire(&condVar->x.compat.condVarLock, GetReturnAddress());

      if (condVar->x.compat.numWaiters > condVar->x.compat.numForRelease) {
         SetEvent(condVar->x.compat.signalEvent);
         condVar->x.compat.numForRelease++;
      }

      MXRecLockRelease(&condVar->x.compat.condVarLock);
   }
#else
   err = pthread_cond_signal(&condVar->condObject);

   if (err != 0) {
      Panic("%s: failure %d on condVar (%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->name);
   }
#endif
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
#if !defined(_WIN32)
   Err_Number err;
#endif

   ASSERT(condVar && (condVar->signature == MXUSER_CONDVAR_SIGNATURE));

#if defined(_WIN32)
   if (pWakeAllConditionVariable) {
      (*pWakeAllConditionVariable)(&condVar->x.condObject);
   } else {
      MXRecLockAcquire(&condVar->x.compat.condVarLock, GetReturnAddress());

      if (condVar->x.compat.numWaiters > condVar->x.compat.numForRelease) {
         SetEvent(condVar->x.compat.signalEvent);
         condVar->x.compat.numForRelease = condVar->x.compat.numWaiters;
      }

      MXRecLockRelease(&condVar->x.compat.condVarLock);
   }
#else
   err = pthread_cond_broadcast(&condVar->condObject);

   if (err != 0) {
      Panic("%s: failure %d on condVar (%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->name);
   }
#endif
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

#if defined(_WIN32)
      if (pInitializeConditionVariable == NULL) {
         CloseHandle(condVar->x.compat.signalEvent);
         MXRecLockDestroy(&condVar->x.compat.condVarLock);
      }
#else
      pthread_cond_destroy(&condVar->condObject);
#endif

      condVar->signature = 0;  // just in case...
      free(condVar->name);
      condVar->name = NULL;
      condVar->ownerLock = NULL;

      free(condVar);
   }
}

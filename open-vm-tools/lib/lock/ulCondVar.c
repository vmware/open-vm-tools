/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
#else
#include <sys/time.h>
#endif

#include "vmware.h"
#include "vm_basic_asm.h"
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
   MXUserHeader      *header;
   MXRecLock         *ownerLock;
   Atomic_uint32      referenceCount;

#if defined(_WIN32)
   CONDITION_VARIABLE condObject;
#else
   pthread_cond_t     condObject;
#endif
};

#if defined(_WIN32)
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
   InitializeConditionVariable(&condVar->condObject);

   return TRUE;
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
 *      As above
 *
 * Side effects:
 *      It is possible to return from this routine without the condition
 *      variable having been signalled (spurious wake up); code accordingly!
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserWaitInternal(MXRecLock *lock,         // IN/OUT:
                   MXUserCondVar *condVar,  // IN/OUT:
                   uint32 waitTimeMsec)     // IN:
{
   int lockCount = MXRecLockCount(lock);
   DWORD waitTime = (waitTimeMsec == MXUSER_WAIT_INFINITE) ? INFINITE :
                                                             waitTimeMsec;

   /*
    * When using the native lock found within the MXUser lock, be sure to
    * decrement the count before the wait/sleep and increment it after the
    * wait/sleep - the (native) wait/sleep will perform a lock release
    * before the wait/sleep and a lock acquisition after the wait/sleep.
    * The MXUser internal accounting information must be maintained.
    */

   MXRecLockDecCount(lock, lockCount);
   SleepConditionVariableCS(&condVar->condObject, &lock->nativeLock,
                            waitTime);
   MXRecLockIncCount(lock, lockCount);
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
   WakeConditionVariable(&condVar->condObject);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserBroadcastInternal --
 *
 *      Perform the environmentally specific portion of broadcasting on an
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
   WakeAllConditionVariable(&condVar->condObject);

   return 0;
}

#else /* _WIN32 */

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
 *      As above
 *
 * Side effects:
 *      It is possible to return from this routine without the condition
 *      variable having been signalled (spurious wake up); code accordingly!
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserWaitInternal(MXRecLock *lock,         // IN/OUT:
                   MXUserCondVar *condVar,  // IN/OUT:
                   uint32 waitTimeMsec)     // IN:
{
   int err;
   int lockCount = MXRecLockCount(lock);

   /*
    * When using the native lock found within the MXUser lock, be sure to
    * decrement the count before the wait/sleep and increment it after the
    * wait/sleep - the (native) wait/sleep will perform a lock release before
    * the wait/sleep and a lock acquisition after the wait/sleep. The
    * MXUser internal accounting information must be maintained.
    */

   MXRecLockDecCount(lock, lockCount);

   if (waitTimeMsec == MXUSER_WAIT_INFINITE) {
      err = pthread_cond_wait(&condVar->condObject, &lock->nativeLock);
   } else {
      struct timeval curTime;
      struct timespec endTime;
      uint64 endNS;

      /*
       * pthread_cond_timedwait takes an absolute time. Yes, this is
       * beyond ridiculous, and the justifications for this
       * vs. relative time make no sense. But IIWII.
       */
#define A_BILLION (1000 * 1000 * 1000)
      gettimeofday(&curTime, NULL);
      endNS = ((uint64) curTime.tv_sec * A_BILLION) +
              ((uint64) curTime.tv_usec * 1000) +
              ((uint64) waitTimeMsec * (1000 * 1000));

      endTime.tv_sec = (time_t) (endNS / A_BILLION);
      endTime.tv_nsec = (long int) (endNS % A_BILLION);
#undef A_BILLION

      err = pthread_cond_timedwait(&condVar->condObject, &lock->nativeLock,
                                   &endTime);
   }

   MXRecLockIncCount(lock, lockCount);

   if (err != 0) {
      if (err != ETIMEDOUT) {
         Panic("%s: failure %d on condVar (0x%p; %s)\n", __FUNCTION__, err,
               condVar, condVar->header->name);
      }
   }
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
 *      Perform the environmentally specific portion of broadcasting on an
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

#endif /* _WIN32 */


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
   MXUserCondVar *condVar = Util_SafeCalloc(1, sizeof *condVar);

   if (UNLIKELY(!MXUserCreateInternal(condVar))) {
      Panic("%s: native lock initialization routine failed\n", __FUNCTION__);
   }

   condVar->signature = MXUserGetSignature(MXUSER_TYPE_CONDVAR);
   condVar->header = header;
   condVar->ownerLock = lock;

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
 *      As above
 *
 * Side effects:
 *      An attempt to use a lock other than the one the specified condition
 *      variable was specified for will result in a panic.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserWaitCondVar(MXUserHeader *header,    // IN:
                  MXRecLock *lock,         // IN/OUT:
                  MXUserCondVar *condVar,  // IN/OUT:
                  uint32 waitTimeMsec)     // IN:
{
   ASSERT(header != NULL);
   ASSERT(lock != NULL);
   ASSERT(condVar != NULL);
   ASSERT(condVar->signature == MXUserGetSignature(MXUSER_TYPE_CONDVAR));

   if (condVar->ownerLock != lock) {
      Panic("%s: invalid use of lock %s with condVar (0x%p; %s)\n",
             __FUNCTION__, header->name, condVar, condVar->header->name);
   }

   if (vmx86_debug && !MXRecLockIsOwner(lock)) {
      Panic("%s: lock %s for condVar (0x%p) not owned\n",
            __FUNCTION__, condVar->header->name, condVar);
   }

   Atomic_Inc(&condVar->referenceCount);
   MXUserWaitInternal(lock, condVar, waitTimeMsec);
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
MXUser_SignalCondVar(MXUserCondVar *condVar)  // IN/OUT:
{
   int err;

   ASSERT(condVar != NULL);
   ASSERT(condVar->signature == MXUserGetSignature(MXUSER_TYPE_CONDVAR));

   err = MXUserSignalInternal(condVar);

   if (err != 0) {
      Panic("%s: failure %d on condVar (0x%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->header->name);
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
MXUser_BroadcastCondVar(MXUserCondVar *condVar)  // IN/OUT:
{
   int err;

   ASSERT(condVar != NULL);
   ASSERT(condVar->signature == MXUserGetSignature(MXUSER_TYPE_CONDVAR));

   err = MXUserBroadcastInternal(condVar);

   if (err != 0) {
      Panic("%s: failure %d on condVar (0x%p; %s) \n", __FUNCTION__, err,
            condVar, condVar->header->name);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyCondVar --
 *
 *      Destroy a condition variable.
 *
 *      A condVar must be destroyed before the lock it is associated with.
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
MXUser_DestroyCondVar(MXUserCondVar *condVar)  // IN/OUT:
{
   if (condVar != NULL) {
      ASSERT(condVar->signature == MXUserGetSignature(MXUSER_TYPE_CONDVAR));

      if (Atomic_Read(&condVar->referenceCount) != 0) {
         Panic("%s: Attempted destroy on active condVar (0x%p; %s)\n",
               __FUNCTION__, condVar, condVar->header->name);
      }

      condVar->signature = 0;  // just in case...

      MXUserDestroyInternal(condVar);

      condVar->header = NULL;
      condVar->ownerLock = NULL;

      free(condVar);
   }
}

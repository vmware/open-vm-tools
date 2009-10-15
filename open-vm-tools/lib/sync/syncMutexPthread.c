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
 * syncMutexPthread.c --
 *
 *      Implements a non-recursive mutex using pthreads
 */

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

#include "vm_assert.h"
#include "syncMutex.h"
#include "util.h"

/*
 *----------------------------------------------------------------------
 *
 * SyncMutex_Init --
 *
 *      Initializes a mutex structure. The 'path' parameter names the
 *      mutex. If 'path' is NULL, then an anonymous mutex is
 *      created. (see SyncWaitQ_Init)
 *
 * Results:
 *      TRUE on success and FALSE otherwise
 *
 *---------------------------------------------------------------------- 
 */

Bool
SyncMutex_Init(SyncMutex *that,   // OUT:
	       char const *path)  // IN:
{
   int error;

   error = pthread_mutex_init(&that->_mutex, NULL);
   if (error != 0) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SyncMutex_Destroy --
 *
 *      Frees members of the specified mutex structure
 *      (see SyncWaitQ_Destroy for side effects)
 *
 *----------------------------------------------------------------------
 */

void
SyncMutex_Destroy(SyncMutex *that)  // IN:
{
#ifdef VMX86_DEBUG
   int error = 
#endif
      pthread_mutex_destroy(&that->_mutex);
#ifdef VMX86_DEBUG
   ASSERT(error != EBUSY);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * SyncMutex_Lock --
 *
 *      Obtains the mutex
 *
 * Results:
 *      TRUE on success and FALSE otherwise
 *
 *----------------------------------------------------------------------
 */

Bool
SyncMutex_Lock(SyncMutex *that) // IN
{
   int error = pthread_mutex_lock(&that->_mutex);

   ASSERT(error != EINVAL);
   if (error != 0) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SyncMutex_Unlock --
 *
 *      Releases the mutex
 *
 * Results:
 *      TRUE on success and FALSE otherwise
 *
 *----------------------------------------------------------------------
 */

Bool
SyncMutex_Unlock(SyncMutex *that) // IN
{
   int error = pthread_mutex_unlock(&that->_mutex);

   ASSERT(error != EINVAL);
   if (error != 0) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncMutex_CreateSingleton --
 *
 *      Creates and returns a mutex backed by the specified storage in a
 *      thread-safe manner. This is useful for modules that need to
 *      protect something with a lock but don't have an existing Init()
 *      entry point where a lock can be created.
 *
 * Results:
 *      A pointer to the mutex. Don't destroy it.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

SyncMutex *
SyncMutex_CreateSingleton(Atomic_Ptr *lckStorage) // IN
{
   SyncMutex *before, *lck;

   /* Check if we've created the lock already. */
   lck = (SyncMutex *) Atomic_ReadPtr(lckStorage);

   if (UNLIKELY(NULL == lck)) {
      /* We haven't, create it. */
      lck = (SyncMutex *) Util_SafeMalloc(sizeof *lck);
      SyncMutex_Init(lck, NULL);

      /*
       * We have successfully created the lock, save it.
       */

      before = (SyncMutex *) Atomic_ReadIfEqualWritePtr(lckStorage, NULL, lck);

      if (before) {
         /* We raced and lost, but it's all cool. */
         SyncMutex_Destroy(lck);
         free(lck);
         lck = before;
      }
   }

   return lck;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncMutex_Trylock --
 *
 *      Tries to lock the mutex. Returns without blocking.
 *
 * Results:
 *      TRUE if the mutex was locked, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncMutex_Trylock(SyncMutex *that)  // IN:
{
   ASSERT(that);
#ifdef VMX86_SERVER
   NOT_IMPLEMENTED();
   return FALSE;
#else
   return (0 == pthread_mutex_trylock(&that->_mutex));
#endif
}

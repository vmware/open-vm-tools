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

#include "vmware.h"
#include "userlock.h"
#include "ulInt.h"


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonExclLock --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      exclusive lock. This is useful for modules that need to protect
 *      something with a lock but don't have an existing Init() entry point
 *      where a lock can be created.
 *
 * Results:
 *      A pointer to the requested lock.
 *
 * Side effects:
 *      Generally the lock's resources are intentionally leaked (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserExclLock *
MXUser_CreateSingletonExclLock(Atomic_Ptr *lockStorage,  // IN/OUT:
                               const char *name,         // IN:
                               MX_Rank rank)             // IN:
{
   MXUserExclLock *lock = (MXUserExclLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserExclLock *before;

      lock = MXUser_CreateExclLock(name, rank);

      before = (MXUserExclLock *) Atomic_ReadIfEqualWritePtr(lockStorage, NULL,
                                                           (void *) lock);

      if (before) {
         MXUser_DestroyExclLock(lock);

         lock = before;
      }
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonRecLock --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      recursive lock. This is useful for modules that need to protect
 *      something with a lock but don't have an existing Init() entry point
 *      where a lock can be created.
 *
 * Results:
 *      A pointer to the requested lock.
 *
 * Side effects:
 *      Generally the lock's resources are intentionally leaked (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserRecLock *
MXUser_CreateSingletonRecLock(Atomic_Ptr *lockStorage,  // IN/OUT:
                              const char *name,         // IN:
                              MX_Rank rank)             // IN:
{
   MXUserRecLock *lock = (MXUserRecLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserRecLock *before;

      lock = MXUser_CreateRecLock(name, rank);

      before = (MXUserRecLock *) Atomic_ReadIfEqualWritePtr(lockStorage, NULL,
                                                            (void *) lock);

      if (before) {
         MXUser_DestroyRecLock(lock);

         lock = before;
      }
   }

   return lock;
}


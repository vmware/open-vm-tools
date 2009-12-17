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
 * MXUser_TryAcquireExclLock --
 *
 *      An attempt is made to conditionally acquire (lock) an exclusive lock.
 *
 * Results:
 *      TRUE    Acquired (locked)
 *      FALSE   Not acquired
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_TryAcquireExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   Bool success;

   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   success = MXRecLockTryAcquire(&lock->lockRecursive, GetReturnAddress());

   if (success && (MXRecLockCount(&lock->lockRecursive) > 1)) {
      MXUserDumpAndPanic(&lock->lockHeader,
                         "%s: Acquire on an acquired exclusive lock",
                         __FUNCTION__);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TryAcquireRecLock --
 *
 *      An attempt is made to conditionally acquire (lock) a recursive lock.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
 *
 * Results:
 *      TRUE    Acquired (locked)
 *      FALSE   Not acquired
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_TryAcquireRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   return MXRecLockTryAcquire(&lock->lockRecursive, GetReturnAddress());
}

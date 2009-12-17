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
 * MXUser_DestroyExclLock --
 *
 *      Destroy an exclusive lock.
 *
 * Results:
 *      Lock is destroyed. Don't use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyExclLock(MXUserExclLock *lock)  // IN:
{
   if (lock != NULL) {
      ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

      if (MXRecLockCount(&lock->lockRecursive) > 0) {
         MXUserDumpAndPanic(&lock->lockHeader,
                            "%s: Destroy of an acquired exclusive lock",
                            __FUNCTION__);
      }

      MXRecLockDestroy(&lock->lockRecursive);
      free((void *) lock->lockHeader.lockName);
      free(lock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyRecLock --
 *
 *      Destroy a recursive lock.
 *
 * Results:
 *      Lock is destroyed. Don't use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyRecLock(MXUserRecLock *lock)  // IN:
{
   if (lock != NULL) {
      ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

      if (MXRecLockCount(&lock->lockRecursive) > 0) {
         MXUserDumpAndPanic(&lock->lockHeader,
                            "%s: Destroy of an acquired recursive lock",
                            __FUNCTION__);
      }

      MXRecLockDestroy(&lock->lockRecursive);
      free((void *) lock->lockHeader.lockName);
      free(lock);
   }
}

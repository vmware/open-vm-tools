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
 * MXUserReleaseExclLock --
 *
 *      Release (unlock) a lock.
 *
 * Results:
 *      The lock is released.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserReleaseExclLock(MXRecLock *lock,  // IN/OUT:
                      char *type)       // IN:
{
   if (!MXRecLockIsOwner(lock)) {
      if (MXRecLockCount(lock) == 0) {
         MXUserDumpAndPanic(lock, "%s: Release of an unacquired %s lock",
                            __FUNCTION__, type);
      } else {
         MXUserDumpAndPanic(lock, "%s: Release of owned %s lock",
                            __FUNCTION__, type);
      }
   }

   MXRecLockRelease(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ReleaseExclLock --
 *
 *      Release (unlock) an exclusive lock.
 *
 * Results:
 *      The lock is released.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_ReleaseExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   MXUserReleaseExclLock(&lock->basic, "exclusive");
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ReleaseRecLock --
 *
 *      Release (unlock) a recursive lock.
 *
 * Results:
 *      The lock is released.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_ReleaseRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   MXUserReleaseExclLock(&lock->basic, "recursive");
}

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
 * MXUser_IsLockedByCurThreadExclLock --
 *
 *      Is an exclusive lock held by the calling thread?
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_IsLockedByCurThreadExclLock(const MXUserExclLock *lock)  // IN:
{
   return MXRecLockIsOwner(&lock->lockRecursive);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IsLockedByCurThreadRecLock --
 *
 *      Is a recursive lock held by the calling thread?
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_IsLockedByCurThreadRecLock(const MXUserRecLock *lock)  // IN:
{
   return MXRecLockIsOwner(&lock->lockRecursive);
}


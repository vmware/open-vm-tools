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
#include "str.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpExclLock
 *
 *      Dump an exclusive lock.
 *
 * Results:
 *      A dump.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserDumpExclLock(MXUserHeader *header)  // IN:
{
   MXUserExclLock *lock = (MXUserExclLock *) header;

   Warning("%s: Exclusive lock @ %p\n", __FUNCTION__, lock);

   Warning("\tsignature %X\n", lock->lockHeader.lockSignature);
   Warning("\tname %s\n", lock->lockHeader.lockName);
   Warning("\trank %d\n", lock->lockHeader.lockRank);

   Warning("\tcount %u\n", lock->lockRecursive.lockCount);

#if defined(VMX86_DEBUG)
   Warning("\tcaller %p\n", lock->lockRecursive.lockCaller);
   Warning("\tVThreadID %d\n", (int) lock->lockRecursive.lockVThreadID);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateExclLock --
 *
 *      Create an exclusive lock.
 *
 * Results:
 *      NULL  Creation failed
 *      !NULL Creation succeeded
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserExclLock *
MXUser_CreateExclLock(const char *userName,  // IN:
                      MX_Rank rank)          // IN:
{
   char *properName;
   MXUserExclLock *lock;

   lock = Util_SafeCalloc(1, sizeof(*lock));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "X-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   lock->lockHeader.lockName = properName;
   lock->lockHeader.lockSignature = USERLOCK_SIGNATURE;
   lock->lockHeader.lockRank = rank;
   lock->lockHeader.lockDumper = MXUserDumpExclLock;

   if (!MXRecLockInit(&lock->lockRecursive)) {
      free(lock);
      free(properName);
      lock = NULL;
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpRecLock
 *
 *      Dump an recursive lock.
 *
 * Results:
 *      A dump.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserDumpRecLock(MXUserHeader *header)  // IN:
{
   MXUserRecLock *lock = (MXUserRecLock *) header;

   Warning("%s: Recursive lock @ %p\n", __FUNCTION__, lock);

   Warning("\tsignature %X\n", lock->lockHeader.lockSignature);
   Warning("\tname %s\n", lock->lockHeader.lockName);
   Warning("\trank %d\n", lock->lockHeader.lockRank);

   Warning("\tcount %u\n", lock->lockRecursive.lockCount);

#if defined(VMX86_DEBUG)
   Warning("\tcaller %p\n", lock->lockRecursive.lockCaller);
   Warning("\tVThreadID %d\n", (int) lock->lockRecursive.lockVThreadID);
#endif

   Warning("\tlockVmm %p\n", lock->lockVmm);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateRecLock --
 *
 *      Create a recursive lock.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
 *
 * Results:
 *      NULL  Creation failed
 *      !NULL Creation succeeded
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserRecLock *
MXUser_CreateRecLock(const char *userName,  // IN:
                     MX_Rank rank)          // IN:
{
   char *properName;
   MXUserRecLock *lock;

   lock = Util_SafeCalloc(1, sizeof(*lock));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "R-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   lock->lockHeader.lockName = properName;
   lock->lockHeader.lockSignature = USERLOCK_SIGNATURE;
   lock->lockHeader.lockRank = rank;
   lock->lockHeader.lockDumper = MXUserDumpRecLock;

   if (MXRecLockInit(&lock->lockRecursive)) {
      lock->lockVmm = NULL;
   } else {
      free(lock);
      free(properName);
      lock = NULL;
   }

   return lock;
}

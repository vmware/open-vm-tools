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

#ifndef _ULINTSHARED_H_
#define _ULINTSHARED_H_


#if defined(MXUSER_DEBUG)
extern void MXUserListLocks(void);
#else
static INLINE void
MXUserListLocks(void)
{
   return;
}
#endif

MX_Rank MXUserCurrentRank(void);

extern void MXUserInstallMxHooks(void (*theLockListFunc)(void),
                                 MX_Rank (*theRankFunc)(void),
                                 void (*theLockFunc)(struct MX_MutexRec *lock),
                                 void (*theUnlockFunc)(struct MX_MutexRec *lock),
                                 Bool (*theTryLockFunc)(struct MX_MutexRec *lock),
                                 Bool (*theIsLockedFunc)(const struct MX_MutexRec *lock),
                                 char *(*theNameFunc)(const struct MX_MutexRec *lock),
                                 void (*theSetInPanicFunc)(void),
                                 Bool (*theInPanicFunc)(void));

typedef struct MXUserLockTreeNode MXUserLockTreeNode;

void MXUserLockTreeAcquire(void);
void MXUserLockTreeRelease(void);

MXUserLockTreeNode *MXUserLockTreeAdd(MXUserLockTreeNode *searchNode,
                                      const char *name,
                                      uint64 serialNumber,
                                      MX_Rank rank);

#endif

/*********************************************************
 * Copyright (C) 2006-2017 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * pollImpl.h --
 *
 *      Header file for poll implementations. Poll consumers should not
 *      include is file.
 */


#ifndef _POLLIMPL_H_
#define _POLLIMPL_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "poll.h"
#include "vm_basic_asm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * PollImpl:
 *
 * A Poll implementation should provide a filled in PollImpl
 * to pass to Poll_Init.
 */

typedef struct PollImpl {
   void         (*Init)                 (void);
   void         (*Exit)                 (void);
   void         (*LoopTimeout)          (Bool loop, Bool *exit,
                                         PollClass c, int timeout);
   VMwareStatus (*Callback)             (PollClassSet classSet, int flags,
                                         PollerFunction f, void *clientData,
                                         PollEventType type,
                                         PollDevHandle info,
                                         MXUserRecLock *lock);
   Bool         (*CallbackRemove)       (PollClassSet classSet, int flags,
                                         PollerFunction f, void *clientData,
                                         PollEventType type);
   Bool         (*CallbackRemoveOneByCB)(PollClassSet classSet, int flags,
                                         PollerFunction f, PollEventType type,
                                         void **clientData);
   Bool         (*LockingEnabled)       (void);
   void         (*NotifyChange)         (PollClassSet classSet);
} PollImpl;


void Poll_InitWithImpl(const PollImpl *impl);

/* Check if a PollClass is part of the set. */
static INLINE Bool
PollClassSet_IsMember(PollClassSet set, PollClass c)
{
   return (set.bits & PollClassSet_Singleton(c).bits) != 0;
}

/* Compare two PollClassSets. */
static INLINE Bool
PollClassSet_Equals(PollClassSet lhs, PollClassSet rhs)
{
   return lhs.bits == rhs.bits;
}

/* Verifies if the class set is empty. */
static INLINE Bool
PollClassSet_IsEmpty(PollClassSet cs)
{
   return PollClassSet_Equals(cs, PollClassSet_Empty());
}

/* Remove from a PollClassSet. */
static INLINE void
PollClassSet_Remove(PollClassSet *set, PollClass c)
{
   set->bits &= ~PollClassSet_Singleton(c).bits;
}

/* Find first set.  POLL_MAX_CLASSES for none set. */
static INLINE PollClass
PollClassSet_FFS(PollClassSet *set)
{
   if (set->bits != 0) {
      return (PollClass)lssbPtr_0(set->bits);
   }
   return POLL_MAX_CLASSES;
}


/*
 *----------------------------------------------------------------------------
 *
 * PollLockingAlwaysEnabled --
 * PollLockingNotAvailable --
 *
 *      Simple LockingEnabled() functions for poll implementation that does
 *      not dynamically enable locking.
 *
 * Results:
 *      Bool.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
PollLockingAlwaysEnabled(void)
{
   return TRUE;
}

static INLINE Bool
PollLockingNotAvailable(void)
{
   return FALSE;
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _POLLIMPL_H_ */

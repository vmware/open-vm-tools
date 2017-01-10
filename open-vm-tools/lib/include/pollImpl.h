/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
   ASSERT(c < POLL_MAX_CLASSES);
   return ((set.bits[c / _POLL_ELEMSIZE] >> (c % _POLL_ELEMSIZE)) & 0x1) != 0;
}

/* Compare two PollClassSets. */
static INLINE Bool
PollClassSet_Equals(PollClassSet lhs, PollClassSet rhs)
{
   unsigned i;

   for (i = 0; i < ARRAYSIZE(lhs.bits); i++) {
      if (lhs.bits[i] != rhs.bits[i]) {
         return FALSE;
      }
   }
   return TRUE;
}

/* Remove from a PollClassSet. */
static INLINE void
PollClassSet_Remove(PollClassSet *set, PollClass c)
{
   ASSERT(c < POLL_MAX_CLASSES);
   set->bits[c / _POLL_ELEMSIZE] &= ~(CONST3264U(1) << (c % _POLL_ELEMSIZE));
}

/* Find first set.  POLL_MAX_CLASSES for none set. */
static INLINE PollClass
PollClassSet_FFS(PollClassSet *set)
{
   unsigned i, j;

   /* XXX TODO: use lssbPtr */
   for (i = 0; i < ARRAYSIZE(set->bits); i++) {
      if (set->bits[i] != 0) {
         for (j = 0; j < _POLL_ELEMSIZE; j++) {
            if ((set->bits[i] & (CONST3264U(1) << j)) != 0) {
               PollClass c = (PollClass)(i * _POLL_ELEMSIZE + j);
               ASSERT(c < POLL_MAX_CLASSES);
               return c;
            }
         }
      }
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

#endif /* _POLLIMPL_H_ */

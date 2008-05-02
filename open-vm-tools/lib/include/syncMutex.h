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
 * syncMutex.h --
 *
 *      Implements a platform independent mutex
 */

#ifndef _SYNC_MUTEX_H_
#define _SYNC_MUTEX_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if !defined(_WIN32)
#if defined(N_PLAT_NLM)
#include <nwerrno.h>
#include <nwadv.h>
#include <nwthread.h>
#include <nwsemaph.h>
#else
#include <pthread.h>
#endif
#endif

#include "syncWaitQ.h"
#include "vm_atomic.h"

/*
 * SyncMutex --
 */

typedef struct SyncMutex {
#if defined(N_PLAT_NLM)
   LONG semaphoreHandle;
#else
   SyncWaitQ wq;

   /* Is the mutex unlocked? --hpreg */
   Atomic_uint32 unlocked;
#if !defined(_WIN32)
   pthread_mutex_t _mutex;
#endif
#endif
} SyncMutex;

Bool SyncMutex_Init(SyncMutex *that,
                    char const *path);
void SyncMutex_Destroy(SyncMutex *that);
Bool SyncMutex_Lock(SyncMutex *that);
Bool SyncMutex_Unlock(SyncMutex *that);

#if !defined(N_PLAT_NLM)
Bool SyncMutex_Trylock(SyncMutex *that);
#endif

SyncMutex *SyncMutex_CreateSingleton(Atomic_Ptr *lckStorage);

#endif // #ifndef _SYNC_MUTEX_H_

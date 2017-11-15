/*********************************************************
 * Copyright (C) 2004-2017 VMware, Inc. All rights reserved.
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
 * syncEvent.h --
 *
 *      Implements a platform independent condition event that
 *      you can either wait on or pass to Poll() or your event loop.
 *
 *      WARNING!
 *      This is an auto-reset event. So, you cannot use it for devices
 *      in Poll that may be holding a device lock. It works fine for Poll if
 *      you don't specify a lock when you register the handle with Poll().
 */

#ifndef _SYNC_EVENT_H_
#define _SYNC_EVENT_H_

//#include "syncWaitQ.h"
#include "vm_atomic.h"

#if defined(__cplusplus)
extern "C" {
#endif

 
#ifndef _WIN32
typedef enum
{
   READ_FD_INDEX      = 0,
   WRITE_FD_INDEX     = 1,
   NUM_SYNC_EVENT_FDS = 2
} SyncEventFDTypes;
#endif // _WIN32


/*
 * SyncEvent --
 */
typedef struct SyncEvent {
   /*
    * Whether the waitqueue has been initialized;
    */
   Bool           initialized;

#ifdef _WIN32
   HANDLE         event;
#else
   Atomic_uint32  signalled;
   int            fdList[NUM_SYNC_EVENT_FDS];
#endif // #ifdef _WIN32
} SyncEvent;


/*
 * Be careful, on win64, handles are 64 bits, but Poll takes an int32.
 */
typedef int32 SyncEventSelectableHandle;


Bool SyncEvent_Init(SyncEvent *that);
void SyncEvent_Destroy(SyncEvent *that);

void SyncEvent_Signal(SyncEvent *that);
Bool SyncEvent_TryWait(SyncEvent *that);
void SyncEvent_Wait(SyncEvent *that);

SyncEventSelectableHandle SyncEvent_GetHandle(SyncEvent *that);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // #ifndef _SYNC_EVENT_H_

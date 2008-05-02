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
 * syncWaitQ.h --
 *
 *      Implements a platform independent wait queue
 */

#ifndef _SYNC_WAITQ_H_
#define _SYNC_WAITQ_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if __APPLE__
#include <pthread.h>
#endif

#include "vm_atomic.h"

/*
 * syncWaitQ.h --
 *
 *      The semantics of this wait queue primitive are as follows:
 *
 *      o Client threads can add themselves to a waitqueue object and
 *      receive a pollable handle via a call to SyncWaitQ_Add
 *
 *      o When the waitqueue is woken up, each handle that was
 *      previously obtained via a call to SyncWaitQ_Add becomes
 *      signaled and remains so until it is removed via a call to
 *      SyncWaitQ_Remove. Any calls to SyncWaitQ_Add, after the queue
 *      has been woken up, will return fresh, unsignaled handles.
 *
 *      For more information please refer to comments in the
 *      respective syncWaitQ{host}.c files
 *
 *      -- Ticho.
 * 
 */

/*
 * SyncWaitQ --
 *
 *      Memory buffer that stores information about an wait queue
 *      object. 
 *
 *      In the case of named queues, this structure can be allocated
 *      on shared memory and shared between multiple processes.  
 *
 *      This structure, however, cannot be memcpy()-ed
 */


typedef struct SyncWaitQ {
   /*
    * Common members used for both named and unnamed objects
    */

   // Whether the waitqueue has been initialized;
   Bool            initialized;
   // A unique sequence number of the queue
   Atomic_uint64   seq;
   // Whether there are any waiters on this queue
   Atomic_uint32   waiters;

   /*
    * Members used in case of named objects
    */

   // Name of the waitqueue object (FIFO path on Linux or Event name on Win32)
   char *pathName;

   /*
    * The following handles are only used only in the case of
    * anonymous pipes.
    *
    * On Win32 the readHandle is a handle to an event object
    *
    * On Posix the rwHandles are the read and write ends of an anonymous pipe
    *
    *    -- Ticho 
    */
   
#ifdef _WIN32
   Atomic_uint64 readHandle;
#else 
   Atomic_uint64 rwHandles;
#   if __APPLE__
   pthread_mutex_t mutex;
#   endif
#endif // #ifdef _WIN32
} SyncWaitQ;

Bool SyncWaitQ_Init(SyncWaitQ *that, char const *path);
void SyncWaitQ_Destroy(SyncWaitQ *that);
PollDevHandle  SyncWaitQ_Add(SyncWaitQ *that);
Bool SyncWaitQ_Remove(SyncWaitQ *that, PollDevHandle fd);
Bool SyncWaitQ_WakeUp(SyncWaitQ *that);

#endif // #infdef _SYNC_WAITQ_H_

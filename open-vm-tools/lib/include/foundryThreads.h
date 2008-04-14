/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * This header file is shared only by files in the Foundry implementation.
 * It defines functions that are shared across several files, but are
 * not part of the public API we will ship to customers.
 * The public Foundry API is defined in vix.h
 *
 */

#ifndef _VIX_Threads_H_
#define _VIX_Threads_H_

#ifdef __cplusplus
extern "C"{
#endif 

#if !_WIN32
#include <pthread.h>
#endif

struct FoundryWorkerThread;

typedef void (*FoundryThreadProc)(struct FoundryWorkerThread *threadState);

struct FoundryWorkerThread *FoundryThreads_StartThread(FoundryThreadProc proc,
                                                       void *threadParam);
void FoundryThreads_StopThread(struct FoundryWorkerThread *threadState);
void FoundryThreads_Free(struct FoundryWorkerThread *threadState);
Bool FoundryThreads_IsCurrentThread(struct FoundryWorkerThread *threadState);


/*
 * This is the state of a single thread.
 */
typedef struct FoundryWorkerThread {
#if _WIN32
   DWORD                   threadId;
   HANDLE                  threadHandle;
#else
   pthread_t               threadInfo;
#endif

   FoundryThreadProc       threadProc;
   Bool                    stopThread;

   void                    *threadParam;
} FoundryWorkerThread;





#ifdef __cplusplus
} // extern "C" {
#endif 


#endif // _VIX_Threads_H_



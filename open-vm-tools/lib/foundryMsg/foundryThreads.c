/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * FoundryThreads.c --
 * 
 * This is a simple cross-platform library for creating threads. It doesn't
 * have all of the functionality of VThreads, but then it doesn't require 
 * all of the infrastructure either so it can be easily used in applications
 * outside the VMX.
 *
 * On Windows, we sometimes run as a COM object in a DLL. DLL's cannot
 * use thread-local-storage, even though they can use threads. As a
 * result, we must build with no-threads, and we cannot use the
 * VThread library. Instead, we must use our OS-specific threads.
 *
 */

#include "vmware.h"
#include "vm_version.h"
#include "util.h"

#include "vix.h"
#include "foundryThreads.h"

#if _WIN32
#include <objbase.h.> // For CoInitializeEx
static DWORD WINAPI FoundryThreadWrapperProc(LPVOID lpParameter);
#else // Linux
#include <pthread.h>
static void *FoundryThreadWrapperProc(void *lpParameter);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryThreads_StartThread --
 *
 *      Start a worker thread.
 *
 * Results:
 *      FoundryWorkerThread *
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

FoundryWorkerThread *
FoundryThreads_StartThread(FoundryThreadProc proc,    // IN
                           void *threadParam)         // IN
{
   VixError err = VIX_OK;
   FoundryWorkerThread *threadState = NULL;
   static const char *createThreadFailureMsg =
      "%s: thread creation failed with error %d.\n";

   ASSERT(proc);

   /*
    * Start the worker threads.
    */
   threadState = (FoundryWorkerThread *) Util_SafeCalloc(1, sizeof(*threadState));
   threadState->threadProc = proc;
   threadState->threadParam = threadParam;

#ifdef _WIN32
   threadState->threadHandle = CreateThread(NULL,
                                            0,
                                            FoundryThreadWrapperProc,
                                            threadState,
                                            0,
                                            &(threadState->threadId));
   if (NULL == threadState->threadHandle) {
      Log(createThreadFailureMsg, __FUNCTION__, GetLastError());
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }
#else // Linux
   {
      pthread_attr_t attr;
      int result;

      pthread_attr_init(&attr);

      pthread_attr_setstacksize(&attr, 512 * 1024);

      result = pthread_create(&(threadState->threadInfo),
                              &attr, 
                              FoundryThreadWrapperProc, 
                              threadState);
      if (0 != result) {
         Log(createThreadFailureMsg, __FUNCTION__, result);
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   }
#endif

abort:
   if (VIX_OK != err) {
      free(threadState);
      threadState = NULL;
   }

   return threadState;
} // FoundryThreads_StartThread


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryThreads_StopThread --
 *
 *      Shutdown a thread and destroys its thread state.
 * 
 * Results:
 *      None.
 *
 * Side effects:
 *      May block while the given thread stops.
 *
 *-----------------------------------------------------------------------------
 */

void
FoundryThreads_StopThread(FoundryWorkerThread *threadState)    // IN
{
#ifdef _WIN32
   DWORD waitResult;
#endif

   if (NULL == threadState) {
      ASSERT(0);
      return;
   }

   /*
    * Stop the thread.
    */
   threadState->stopThread = TRUE;

   ASSERT(!FoundryThreads_IsCurrentThread(threadState));

#ifdef _WIN32
   waitResult = WaitForSingleObject(threadState->threadHandle, 30000);
   if (WAIT_OBJECT_0 != waitResult) {
      TerminateThread(threadState->threadHandle, 0x1);
   }
#else
   pthread_join(threadState->threadInfo, NULL);
#endif

   FoundryThreads_Free(threadState);
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryThreads_Free --
 *
 *      Destroys the thread state.
 * 
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
FoundryThreads_Free(FoundryWorkerThread *threadState)    // IN
{
   if (NULL != threadState) {
#ifdef _WIN32
      CloseHandle(threadState->threadHandle);
      threadState->threadHandle = NULL;
#else
      threadState->threadInfo = 0;
#endif

      free(threadState);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryThreads_IsCurrentThread --
 *
 *      Returns true if the thread state passed in refers to the current thread.
 *
 * Results:
 *      
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool 
FoundryThreads_IsCurrentThread(struct FoundryWorkerThread *threadState)     // IN
{
   if (NULL == threadState) {
      return FALSE;
   }
#ifdef _WIN32
   return (GetCurrentThreadId() == threadState->threadId);
#else
   return pthread_equal(pthread_self(), threadState->threadInfo);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryThreadWrapperProc --
 *
 *       This is a Windows-specific wrapper for a foundry thread procedure.
 *       It calls the platform-independent thread procedure.
 *
 * Results:
 *       Ignored; a thread termination status.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

#if _WIN32
DWORD WINAPI 
FoundryThreadWrapperProc(LPVOID threadParameter)     // IN
#else // Linux
static void *
FoundryThreadWrapperProc(void *threadParameter)      // IN
#endif
{
   FoundryWorkerThread *threadState;

   threadState = (FoundryWorkerThread *) threadParameter;
   if (NULL == threadState) {
      ASSERT(0);
      goto abort;
   }

   if (NULL != threadState->threadProc) {
      (*(threadState->threadProc))(threadState);
   }

abort:

#if _WIN32
   return 0;
#else // Linux
   return NULL;
#endif
}





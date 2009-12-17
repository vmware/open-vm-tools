/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * vthreadUL.c --
 *
 *	Thread management without actually having threads
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__GLIBC__)
#include <pthread.h>
#endif

#include "vmware.h"
#include "vthreadBase.h"
#include "str.h"


/*
 * Local data
 *
 * Initialize thread ID and name so VThread_Init() is optional.
 */

static VThreadID vthreadCurID = VTHREAD_OTHER_ID;
static uintptr_t vthreadHostThreadID = 0;
static Bool vthreadIsInSignal = FALSE;

#if VTHREAD_OTHER_ID != 3
#error "VTHREAD_OTHER_ID is not 3"
#endif

static char vthreadNames[VTHREAD_MAX_THREADS][32] = {
   "",
   "",
   "",
   "app"
};

/*
 * Symbol that needs to be available for lib/lock.
 */

VThreadID vthreadMaxVCPUID = VTHREAD_VCPU0_ID;

/*
 * Local functions
 */

// nothing

/*
 *-----------------------------------------------------------------------------
 *
 * VThreadHostThreadID --
 *
 *      Determine some sort of OS-specific unique threadID.
 *
 *      Nominally, lib/nothread is single-threaded; HostThreadID
 *      would help us detect multithreaded violators.
 *
 * Results:
 *      A per-thread unique uintptr_t.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static uintptr_t
VThreadHostThreadID(void)
{
#if defined(_WIN32)
   return GetCurrentThreadId();
#elif defined(__GLIBC__)
   return pthread_self();
#elif defined(__APPLE__)
   return (uintptr_t)(void*)pthread_self();
#else
   return -1;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_Init --
 *
 *      Module and main thread initialization.
 *
 *      This should be called by the main thread early.
 *
 *      See VThread_InitThread.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Set initial state.
 *
 *-----------------------------------------------------------------------------
 */

void
VThread_Init(VThreadID id,      // IN: this thread's ID
             const char *name)  // IN: this thread's name
{
   if (id == VTHREAD_INVALID_ID) {
      id = VTHREAD_OTHER_ID;
   }
   ASSERT(id >= 0 && id < VTHREAD_VCPU0_ID);

   vthreadCurID = id;

   if (vthreadHostThreadID == 0) {
      vthreadHostThreadID = VThreadHostThreadID();
   }

   ASSERT(name != NULL);
   ASSERT(vthreadNames[id][ARRAYSIZE(vthreadNames[id]) - 1] == '\0');
   strncpy(vthreadNames[id], name, ARRAYSIZE(vthreadNames[id]) - 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_InitThread --
 *
 *      Initialize a thread.
 *
 *      This should be called by threads started outside our control.
 *      Threads started by VThread_CreateThread need to do nothing.
 *
 * Results:
 *      The thread ID.
 *
 * Side effects:
 *      Set initial state.
 *
 *-----------------------------------------------------------------------------
 */

VThreadID
VThread_InitThread(VThreadID id,      // IN: this thread's ID
                   const char *name)  // IN: this thread's name
{
   if (id == VTHREAD_INVALID_ID) {
      /*
       * XXX This emulates some old, broken expectations of callers
       * of Thread_Init(VTHREAD_OTHER_ID) in third-party threads
       * that can also link with either lib/thread or lib/nothread.
       * The calls have become VThread_InitThread(VTHREAD_INVALID_ID),
       * and should behave in the same broken way here and correctly
       * in lib/thread.
       */

      id = VTHREAD_OTHER_ID;
   } else {
      ASSERT(id >= VTHREAD_ALLOCSTART_ID && id < VTHREAD_MAX_THREADS);
   }

   vthreadCurID = id;

   if (vthreadHostThreadID == 0) {
      vthreadHostThreadID = VThreadHostThreadID();
   }

   if (name == NULL) {
      Str_Snprintf(vthreadNames[id], ARRAYSIZE(vthreadNames[id]),
		   "vthread-%d", id);
   } else {
      ASSERT(vthreadNames[id][ARRAYSIZE(vthreadNames[id]) - 1] == '\0');
      strncpy(vthreadNames[id], name, ARRAYSIZE(vthreadNames[id]) - 1);
   }

   return id;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_CurID --
 *
 *      Get the current thread ID.
 *
 * Results:
 *      Thread ID.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VThreadID
VThread_CurID(void)
{
   ASSERT(vthreadCurID < VTHREAD_MAX_THREADS);

   return vthreadCurID;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_CurName --
 *
 *      Get the current thread name.
 *
 * Results:
 *      The current thread name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

const char *
VThread_CurName(void)
{
   uintptr_t hostTID = VThreadHostThreadID();

   if (LIKELY(hostTID == vthreadHostThreadID)) {
      return vthreadNames[vthreadCurID];
   } else {
      static char name[48];

      /*
       * There were multiple threads.  Do our best,
       * but use a static buffer because clients shouldn't be
       * using us anyway if they are multithreaded.
       *
       * XXX: in the future, we should wean people off
       * lib/nothread by putting an ASSERT here!
       */

      snprintf(name, sizeof name, "%s-%"FMTPD"u",
               vthreadNames[vthreadCurID], hostTID);
      name[sizeof name - 1] = '\0';

      return name;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_ExitThread --
 *
 *      Exit.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Bye bye.
 *
 *-----------------------------------------------------------------------------
 */

void
VThread_ExitThread(Bool clean)  // IN:
{
   /*
    * On Linux, we can't possibly have threads, since we're not
    * supposed to link with libpthread.
    * So plain exit() will (and has to) do.
    *
    * On Windows, it's unclear what we should do here.
    * There may or may not be threads, but this module doesn't know
    * either way.  It depends on the caller's intent.
    *
    * The very first caller of this function was an old
    * WS UI.  Since that was a process on Linux
    * and a thread on Windows, we acted accordingly.
    * It still seems to be a good idea.
    *
    * -- edward
    */

#if defined(_WIN32)
   ExitThread(clean ? 0 : 1);
#else
   exit(clean ? 0 : 1);
#endif

   NOT_REACHED();
}


/*
 *----------------------------------------------------------------------------
 *
 * VThread_SetIsInSignal --
 *
 *      Set the 'is in signal' state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Changes internal state.
 *
 *----------------------------------------------------------------------------
 */

void
VThread_SetIsInSignal(VThreadID tid,    // IN:
                      Bool isInSignal)  // IN:
{
   vthreadIsInSignal = isInSignal;
}


#if !defined(_WIN32)
/*
 *----------------------------------------------------------------------------
 *
 * VThread_IsInSignal --
 *
 *      Return the 'is in signal' state.
 *
 * Results:
 *      Returns last value passed to VThread_SetIsInSignal.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VThread_IsInSignal(void)
{
   return vthreadIsInSignal;
}
#endif

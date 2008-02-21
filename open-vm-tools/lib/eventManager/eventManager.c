/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * eventManager.c --
 *
 *    Multi-timer manager
 *
 */

#ifndef VMX86_DEVEL

#endif 

#ifdef __KERNEL__
#   include "kernelStubs.h"
#else
#   ifndef _WIN32
#      include <stdlib.h>
#      include <sys/time.h>
#      include <unistd.h>
#   endif
#   if defined(_WIN32) && defined(_MSC_VER)
#      include <windows.h>
#      include <malloc.h>
#   endif
#   include "debug.h"
#   include "system.h"
#endif

#include "vm_assert.h"
#include "dbllnklst.h"
#include "eventManager.h"


/*
 * The event object
 */

struct Event {
   DblLnkLst_Links l;

   uint64 time;
   EventManager_EventHandler handler;
   void *handlerData;
};


/*
 * Events are stored in a flat doubly linked list of Event to fire,
 * sorted by increasing time.
 */


/*
 * EventManager_Init --
 *
 *    Initialize the event manager module
 *
 * Return value:
 *    Returns a pointer to the event queue on success and NULL on failure.
 *
 * Side effects:
 *    None
 *
 */

DblLnkLst_Links *
EventManager_Init(void)
{
   DblLnkLst_Links *eventQueue;

   eventQueue = malloc(sizeof *eventQueue);
   if (!eventQueue) {
      /* Not enough memory. */
      return NULL;
   }

   DblLnkLst_Init(eventQueue);
   return eventQueue;
}


/*
 * EventManager_Add --
 *
 *    Add an event (that is going to fire, i.e. call the handler with the
 *    handlerData in period hundredth of seconds) to the event queue
 *
 * Return value:
 *    The event on success (it is only valid between the time it is returned,
 *    and the time its associated handler is called)
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 */

Event *
EventManager_Add(DblLnkLst_Links *eventQueue,       // IN:
                 uint32 period,                     // IN: in units of .01s
                 EventManager_EventHandler handler, // IN
                 void *handlerData)                 // IN
{
   Event *e;
   uint64 currentTime;
   DblLnkLst_Links *cur_l;

   ASSERT(eventQueue);
   ASSERT(eventQueue->next);
   ASSERT(eventQueue->prev);
   
   e = malloc(sizeof(*e));
   if (e == NULL) {
      /*
       * Not enough memory
       */

      return FALSE;
   }

   DblLnkLst_Init(&e->l);

   /*
    * Most of the event managers I have studied rely on the system time to fire
    * an event in the future. This is bad, because the system time can be
    * modified: if you schedule an event to fire in 5 seconds, and then
    * somebody substract 1 hour (daylight savings?) to the system time, then
    * your event will actually fire in 1 hour + 5 seconds, which is probably
    * not what you want... Instead, I use the system uptime, which can not be
    * modified.
    *
    *   --hpreg
    */

   currentTime = System_Uptime();
   if (currentTime == -1) {
      /*
       * Unable to retrieve the uptime
       */

      free(e);

      return NULL;
   }
   
   e->time = currentTime + period;
   e->handler = handler;
   e->handlerData = handlerData;

   /*
    * Insert the new event in the sorted list
    */

   for (cur_l = eventQueue->next; cur_l != eventQueue; cur_l = cur_l->next) {
      Event *cur;

      cur = DblLnkLst_Container(cur_l, Event, l);
      if (e->time < cur->time) {
         break;
      }
   }
   DblLnkLst_Link(&e->l, cur_l);

#if 0
   Debug("EventManager_Add: handler-%u, data-%u, period-%u, time-%"FMT64"u\n",
         (uint32) handler, (uint32) handlerData, period, e->time);
#endif

   return e;
}


/*
 * EventManager_Remove --
 *
 *    Remove an event that has not fired yet. The event object is destroyed
 *    after this call.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 */

void
EventManager_Remove(Event *e)                     // IN
{
#if 0
   Debug("EventManager_Remove: handler-%u, handlerData-%u\n",
         (uint32) e->handler, (uint32) e->handlerData);
#endif

   DblLnkLst_Unlink1(&e->l);
   free(e);
}


/*
 * EventManager_ProcessNext --
 *
 *    Process the next event (if any) & return the amount of time the
 *    caller should sleep for before the following event can be
 *    processed.
 *
 * Return value:
 *    -1 on failure
 *     0 if there was no event to process
 *     1 if there are more events to process (*sleepUsecs is set to 0
 *       if we just processed an event, or the duration to sleep for
 *       if it's not time for us to process the next event yet)
 *
 * Side effects:
 *    Lots, depending on events handlers
 *
 */

int
EventManager_ProcessNext(DblLnkLst_Links *eventQueue, // IN:
                         uint64 *sleepUsecs)          // OUT: number of usecs to sleep
{
   uint64 currentTime;
   Event *next;
   int64 delta;
   Bool status;

   ASSERT(eventQueue);
   ASSERT(sleepUsecs);
   
   if (eventQueue->next == eventQueue) {
      /*
       * No event to process
       */

      return 0;
   }
    
   currentTime = System_Uptime();
   if (currentTime == -1) {
      /*
       * Unable to retrieve the uptime
       */

      return -1;
   }

   next = DblLnkLst_Container(eventQueue->next, Event, l);

   delta = next->time - currentTime;
   if (delta > 0) {
      *sleepUsecs = delta * 10000;
      return 1;
   }

   DblLnkLst_Unlink1(&next->l);
   
#if 0
   Debug("EventManager_ProcessNext: currentTime-%"FMT64"u, next->time=%"FMT64
         "u, delta=%"FMT64"u, handler=%u, handlerData=%u\n",
         currentTime, next->time, delta, (uint32) next->handler,
         (uint32) next->handlerData);
#endif
   
   status = (*next->handler)(next->handlerData);
   free(next);
   
   if (status == FALSE) {
      return -1;
   } else {
      *sleepUsecs = 0;
      return 1;
   }
}


/*
 * EventManager_Destroy --
 *
 *    Removes all scheduled events from manager.
 *    Do not call any other EventManager function after this,
 *    until you'll reinit EventManager with EventManager_Init.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 */

void
EventManager_Destroy(DblLnkLst_Links *eventQueue)   // IN:
{
   int cnt = 0;

   ASSERT(eventQueue);

   while (eventQueue->next != eventQueue) {
      Event* next = DblLnkLst_Container(eventQueue->next, Event, l);
      EventManager_Remove(next);
      cnt++;
   }
   if (cnt) {
      Debug("EventManager_Destroy: destroyed %u events\n", cnt);
   }

   free(eventQueue);
}


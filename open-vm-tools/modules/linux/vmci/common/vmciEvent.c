/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmciEvent.c --
 *
 *     VMCI Event code for host and guests.
 */

#include "vmci_kernel_if.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciEvent.h"
#include "vmciKernelAPI.h"
#if defined(_WIN32)
#  include "kernelStubsSal.h"
#endif
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#  include "vmciDriver.h"
#else
#  include "vmciDriver.h"
#endif

#define LGPFX "VMCIEvent: "

#define EVENT_MAGIC 0xEABE0000

typedef struct VMCISubscription {
   VMCIId         id;
   int            refCount;
   Bool           runDelayed;
   VMCIEvent      destroyEvent;
   VMCI_Event     event;
   VMCI_EventCB   callback;
   void           *callbackData;
   VMCIListItem   subscriberListItem;
} VMCISubscription;


static VMCISubscription *VMCIEventFind(VMCIId subID);
static int VMCIEventDeliver(VMCIEventMsg *eventMsg);
static int VMCIEventRegisterSubscription(VMCISubscription *sub,
                                         VMCI_Event event,
                                         uint32 flags,
                                         VMCI_EventCB callback,
                                         void *callbackData);
static VMCISubscription *VMCIEventUnregisterSubscription(VMCIId subID);

static VMCIList subscriberArray[VMCI_EVENT_MAX];
static VMCILock subscriberLock;

typedef struct VMCIDelayedEventInfo {
   VMCISubscription *sub;
   uint8 eventPayload[sizeof(VMCIEventData_Max)];
} VMCIDelayedEventInfo;

typedef struct VMCIEventRef {
   VMCISubscription *sub;
   VMCIListItem   listItem;
} VMCIEventRef;

/*
 *----------------------------------------------------------------------
 *
 * VMCIEvent_Init --
 *
 *      General init code.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIEvent_Init(void)
{
   int i;

   for (i = 0; i < VMCI_EVENT_MAX; i++) {
      VMCIList_Init(&subscriberArray[i]);
   }

   return VMCI_InitLock(&subscriberLock, "VMCIEventSubscriberLock",
                        VMCI_LOCK_RANK_EVENT);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIEvent_Exit --
 *
 *      General exit code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIEvent_Exit(void)
{
   VMCIListItem *iter, *iter2;
   VMCI_Event e;

   /* We free all memory at exit. */
   for (e = 0; e < VMCI_EVENT_MAX; e++) {
      VMCIList_ScanSafe(iter, iter2, &subscriberArray[e]) {
         VMCISubscription *cur;

         /*
          * We should never get here because all events should have been
          * unregistered before we try to unload the driver module.
          * Also, delayed callbacks could still be firing so this cleanup
          * would not be safe.
          * Still it is better to free the memory than not ... so we
          * leave this code in just in case....
          *
          */
         ASSERT(FALSE);

         cur = VMCIList_Entry(iter, VMCISubscription, subscriberListItem);
         VMCI_FreeKernelMem(cur, sizeof *cur);
      }
   }
   VMCI_CleanupLock(&subscriberLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIEvent_Sync --
 *
 *      Use this as a synchronization point when setting globals, for example,
 *      during device shutdown.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIEvent_Sync(void)
{
   VMCILockFlags lockFlags;
   VMCI_GrabLock_BH(&subscriberLock, &lockFlags);
   VMCI_ReleaseLock_BH(&subscriberLock, lockFlags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIEvent_CheckHostCapabilities --
 *
 *      Verify that the host supports the hypercalls we need. If it does not,
 *      try to find fallback hypercalls and use those instead.
 *
 * Results:
 *      TRUE if required hypercalls (or fallback hypercalls) are
 *      supported by the host, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCIEvent_CheckHostCapabilities(void)
{
   /* VMCIEvent does not require any hypercalls. */
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIEventGet --
 *
 *      Gets a reference to the given VMCISubscription.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIEventGet(VMCISubscription *entry)  // IN
{
   ASSERT(entry);

   entry->refCount++;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIEventRelease --
 *
 *      Releases the given VMCISubscription.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Fires the destroy event if the reference count has gone to zero.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIEventRelease(VMCISubscription *entry)  // IN
{
   ASSERT(entry);
   ASSERT(entry->refCount > 0);

   entry->refCount--;
   if (entry->refCount == 0) {
      VMCI_SignalEvent(&entry->destroyEvent);
   }
}


 /*
 *------------------------------------------------------------------------------
 *
 *  EventReleaseCB --
 *
 *     Callback to release the event entry reference. It is called by the
 *     VMCI_WaitOnEvent function before it blocks.
 *
 *  Result:
 *     None.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
EventReleaseCB(void *clientData) // IN
{
   VMCILockFlags flags;
   VMCISubscription *sub = (VMCISubscription *)clientData;

   ASSERT(sub);

   VMCI_GrabLock_BH(&subscriberLock, &flags);
   VMCIEventRelease(sub);
   VMCI_ReleaseLock_BH(&subscriberLock, flags);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIEventFind --
 *
 *      Find entry. Assumes lock is held.
 *
 * Results:
 *      Entry if found, NULL if not.
 *
 * Side effects:
 *      Increments the VMCISubscription refcount if an entry is found.
 *
 *-----------------------------------------------------------------------------
 */

static VMCISubscription *
VMCIEventFind(VMCIId subID)  // IN
{
   VMCIListItem *iter;
   VMCI_Event e;

   for (e = 0; e < VMCI_EVENT_MAX; e++) {
      VMCIList_Scan(iter, &subscriberArray[e]) {
         VMCISubscription *cur =
            VMCIList_Entry(iter, VMCISubscription, subscriberListItem);
         if (cur->id == subID) {
            VMCIEventGet(cur);
            return cur;
         }
      }
   }
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIEventDelayedDispatchCB --
 *
 *      Calls the specified callback in a delayed context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
VMCIEventDelayedDispatchCB(void *data) // IN
{
   VMCIDelayedEventInfo *eventInfo;
   VMCISubscription *sub;
   VMCI_EventData *ed;
   VMCILockFlags flags;

   eventInfo = (VMCIDelayedEventInfo *)data;

   ASSERT(eventInfo);
   ASSERT(eventInfo->sub);

   sub = eventInfo->sub;
   ed = (VMCI_EventData *)eventInfo->eventPayload;

   sub->callback(sub->id, ed, sub->callbackData);

   VMCI_GrabLock_BH(&subscriberLock, &flags);
   VMCIEventRelease(sub);
   VMCI_ReleaseLock_BH(&subscriberLock, flags);

   VMCI_FreeKernelMem(eventInfo, sizeof *eventInfo);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCIEventDeliver --
 *
 *      Actually delivers the events to the subscribers.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The callback function for each subscriber is invoked.
 *
 *----------------------------------------------------------------------------
 */

static int
VMCIEventDeliver(VMCIEventMsg *eventMsg)  // IN
{
   int err = VMCI_SUCCESS;
   VMCIListItem *iter;
   VMCILockFlags flags;

   VMCIList noDelayList;
   VMCIList_Init(&noDelayList);

   ASSERT(eventMsg);

   VMCI_GrabLock_BH(&subscriberLock, &flags);
   VMCIList_Scan(iter, &subscriberArray[eventMsg->eventData.event]) {
      VMCISubscription *cur = VMCIList_Entry(iter, VMCISubscription,
                                             subscriberListItem);
      ASSERT(cur && cur->event == eventMsg->eventData.event);
#if defined(_WIN32)
      _Analysis_assume_(cur != NULL);
#endif

      if (cur->runDelayed) {
         VMCIDelayedEventInfo *eventInfo;
         if ((eventInfo = VMCI_AllocKernelMem(sizeof *eventInfo,
                                              (VMCI_MEMORY_ATOMIC |
                                               VMCI_MEMORY_NONPAGED))) == NULL) {
            err = VMCI_ERROR_NO_MEM;
            goto out;
         }

         VMCIEventGet(cur);

         memset(eventInfo, 0, sizeof *eventInfo);
         memcpy(eventInfo->eventPayload, VMCI_DG_PAYLOAD(eventMsg),
                (size_t)eventMsg->hdr.payloadSize);
         eventInfo->sub = cur;
         err = VMCI_ScheduleDelayedWork(VMCIEventDelayedDispatchCB,
                                        eventInfo);
         if (err != VMCI_SUCCESS) {
            VMCIEventRelease(cur);
            VMCI_FreeKernelMem(eventInfo, sizeof *eventInfo);
            goto out;
         }

      } else {
         VMCIEventRef *eventRef;

         /*
          * To avoid possible lock rank voilation when holding
          * subscriberLock, we construct a local list of
          * subscribers and release subscriberLock before
          * invokes the callbacks. This is similar to delayed
          * callbacks, but callbacks is invoked right away here.
          */
         if ((eventRef = VMCI_AllocKernelMem(sizeof *eventRef,
                                             (VMCI_MEMORY_ATOMIC |
                                              VMCI_MEMORY_NONPAGED))) == NULL) {
            err = VMCI_ERROR_NO_MEM;
            goto out;
         }

         VMCIEventGet(cur);
         eventRef->sub = cur;
         VMCIList_InitEntry(&eventRef->listItem);
         VMCIList_Insert(&eventRef->listItem, &noDelayList);
      }
   }

out:
   VMCI_ReleaseLock_BH(&subscriberLock, flags);

   if (!VMCIList_Empty(&noDelayList)) {
      VMCI_EventData *ed;
      VMCIListItem *iter2;

/*
 * The below ScanSafe macro makes the analyzer think iter might be NULL and
 * then dereferenced.
 */
#if defined(_WIN32)
#pragma warning(suppress: 28182)
#endif
      VMCIList_ScanSafe(iter, iter2, &noDelayList) {
         VMCIEventRef *eventRef = VMCIList_Entry(iter, VMCIEventRef,
                                                 listItem);
         VMCISubscription *cur = eventRef->sub;
         uint8 eventPayload[sizeof(VMCIEventData_Max)];

         /* We set event data before each callback to ensure isolation. */
         memset(eventPayload, 0, sizeof eventPayload);
         memcpy(eventPayload, VMCI_DG_PAYLOAD(eventMsg),
                (size_t)eventMsg->hdr.payloadSize);
         ed = (VMCI_EventData *)eventPayload;
         cur->callback(cur->id, ed, cur->callbackData);

         VMCI_GrabLock_BH(&subscriberLock, &flags);
         VMCIEventRelease(cur);
         VMCI_ReleaseLock_BH(&subscriberLock, flags);
         VMCI_FreeKernelMem(eventRef, sizeof *eventRef);
      }
   }

   return err;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIEvent_Dispatch --
 *
 *      Dispatcher for the VMCI_EVENT_RECEIVE datagrams. Calls all
 *      subscribers for given event.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIEvent_Dispatch(VMCIDatagram *msg)  // IN
{
   VMCIEventMsg *eventMsg = (VMCIEventMsg *)msg;

   ASSERT(msg &&
          msg->src.context == VMCI_HYPERVISOR_CONTEXT_ID &&
          msg->dst.resource == VMCI_EVENT_HANDLER);

   if (msg->payloadSize < sizeof(VMCI_Event) ||
       msg->payloadSize > sizeof(VMCIEventData_Max)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (!VMCI_EVENT_VALID(eventMsg->eventData.event)) {
      return VMCI_ERROR_EVENT_UNKNOWN;
   }

   VMCIEventDeliver(eventMsg);

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIEventRegisterSubscription --
 *
 *      Initialize and add subscription to subscriber list.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIEventRegisterSubscription(VMCISubscription *sub,   // IN
                              VMCI_Event event,        // IN
                              uint32 flags,            // IN
                              VMCI_EventCB callback,   // IN
                              void *callbackData)      // IN
{
#  define VMCI_EVENT_MAX_ATTEMPTS 10
   static VMCIId subscriptionID = 0;
   VMCILockFlags lockFlags;
   uint32 attempts = 0;
   int result;
   Bool success;

   ASSERT(sub);

   if (!VMCI_EVENT_VALID(event) || callback == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to subscribe to event (type=%d) "
                         "(callback=%p) (data=%p).\n",
                         event, callback, callbackData));
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (vmkernel) {
      /*
       * In the vmkernel we defer delivery of events to a helper world.  This
       * makes the event delivery more consistent across hosts and guests with
       * regard to which locks are held.  Memory access and guest paused events
       * are an exception to this, since clients need to know immediately that
       * the device memory is disabled (if we delay such events, then clients
       * may be notified too late).
       */
      if (VMCI_EVENT_MEM_ACCESS_ON == event ||
          VMCI_EVENT_MEM_ACCESS_OFF == event ||
          VMCI_EVENT_GUEST_PAUSED == event ||
          VMCI_EVENT_GUEST_UNPAUSED == event) {
         /*
          * Client must expect to get such events synchronously, and should
          * perform its locking accordingly.  If it can't handle this, then
          * fail.
          */
         if (flags & VMCI_FLAG_EVENT_DELAYED_CB) {
            return VMCI_ERROR_INVALID_ARGS;
         }
         sub->runDelayed = FALSE;
      } else {
         sub->runDelayed = TRUE;
      }
   } else if (!VMCI_CanScheduleDelayedWork()) {
      /*
       * If the platform doesn't support delayed work callbacks then don't
       * allow registration for them.
       */
      if (flags & VMCI_FLAG_EVENT_DELAYED_CB) {
         return VMCI_ERROR_INVALID_ARGS;
      }
      sub->runDelayed = FALSE;
   } else {
      /*
       * The platform supports delayed work callbacks. Honor the requested
       * flags
       */
      sub->runDelayed = (flags & VMCI_FLAG_EVENT_DELAYED_CB) ? TRUE : FALSE;
   }

   sub->refCount = 1;
   sub->event = event;
   sub->callback = callback;
   sub->callbackData = callbackData;
   VMCIList_InitEntry(&sub->subscriberListItem);

   VMCI_GrabLock_BH(&subscriberLock, &lockFlags);

   /* Check if creation of a new event is allowed. */
   if (!VMCI_CanCreate()) {
      result = VMCI_ERROR_UNAVAILABLE;
      goto exit;
   }

   for (success = FALSE, attempts = 0;
        success == FALSE && attempts < VMCI_EVENT_MAX_ATTEMPTS;
        attempts++) {
      VMCISubscription *existingSub = NULL;

      /*
       * We try to get an id a couple of time before claiming we are out of
       * resources.
       */
      sub->id = ++subscriptionID;

      /* Test for duplicate id. */
      existingSub = VMCIEventFind(sub->id);
      if (existingSub == NULL) {
         /* We succeeded if we didn't find a duplicate. */
         success = TRUE;
      } else {
         VMCIEventRelease(existingSub);
      }
   }

   if (success) {
      VMCI_CreateEvent(&sub->destroyEvent);
      VMCIList_Insert(&sub->subscriberListItem, &subscriberArray[event]);
      result = VMCI_SUCCESS;
   } else {
      result = VMCI_ERROR_NO_RESOURCES;
   }

exit:
   VMCI_ReleaseLock_BH(&subscriberLock, lockFlags);
   return result;
#  undef VMCI_EVENT_MAX_ATTEMPTS
}



/*
 *----------------------------------------------------------------------
 *
 * VMCIEventUnregisterSubscription --
 *
 *      Remove subscription from subscriber list.
 *
 * Results:
 *      VMCISubscription when found, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMCISubscription *
VMCIEventUnregisterSubscription(VMCIId subID)    // IN
{
   VMCILockFlags flags;
   VMCISubscription *s;

   VMCI_GrabLock_BH(&subscriberLock, &flags);
   s = VMCIEventFind(subID);
   if (s != NULL) {
      VMCIEventRelease(s);
      VMCIList_Remove(&s->subscriberListItem);
   }
   VMCI_ReleaseLock_BH(&subscriberLock, flags);

   if (s != NULL) {
      VMCI_WaitOnEvent(&s->destroyEvent, EventReleaseCB, s);
      VMCI_DestroyEvent(&s->destroyEvent);
   }

   return s;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_event_subscribe --
 *
 *      Subscribe to given event. The callback specified can be fired
 *      in different contexts depending on what flag is specified while
 *      registering. If flags contains VMCI_FLAG_EVENT_NONE then the
 *      callback is fired with the subscriber lock held (and BH context
 *      on the guest). If flags contain VMCI_FLAG_EVENT_DELAYED_CB then
 *      the callback is fired with no locks held in thread context.
 *      This is useful because other VMCIEvent functions can be called,
 *      but it also increases the chances that an event will be dropped.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_event_subscribe)
int
vmci_event_subscribe(VMCI_Event event,        // IN
#if !defined(__linux__) || defined(VMKERNEL)
                     uint32 flags,            // IN
#endif // !linux || VMKERNEL
                     VMCI_EventCB callback,   // IN
                     void *callbackData,      // IN
                     VMCIId *subscriptionID)  // OUT
{
   int retval;
#if defined(__linux__) && !defined(VMKERNEL)
   uint32 flags = VMCI_FLAG_EVENT_NONE;
#endif // linux && !VMKERNEL
   VMCISubscription *s = NULL;

   if (subscriptionID == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid subscription (NULL).\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   s = VMCI_AllocKernelMem(sizeof *s, VMCI_MEMORY_NONPAGED);
   if (s == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   retval = VMCIEventRegisterSubscription(s, event, flags,
                                          callback, callbackData);
   if (retval < VMCI_SUCCESS) {
      VMCI_FreeKernelMem(s, sizeof *s);
      return retval;
   }

   *subscriptionID = s->id;
   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_event_unsubscribe --
 *
 *      Unsubscribe to given event. Removes it from list and frees it.
 *      Will return callbackData if requested by caller.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_event_unsubscribe)
int
vmci_event_unsubscribe(VMCIId subID)   // IN
{
   VMCISubscription *s;

   /*
    * Return subscription. At this point we know noone else is accessing
    * the subscription so we can free it.
    */
   s = VMCIEventUnregisterSubscription(subID);
   if (s == NULL) {
      return VMCI_ERROR_NOT_FOUND;

   }
   VMCI_FreeKernelMem(s, sizeof *s);

   return VMCI_SUCCESS;
}

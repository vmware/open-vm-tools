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
 * rpcin.c --
 *
 *    Remote Procedure Call between VMware and guest applications
 *    C implementation.
 *
 *    This module implements the guest=>host direction only.
 *    The in and out modules are separate since some applications (e.g.
 *    drivers that want to do RPC-based logging) only want/need/can have the
 *    out direction (the in direction is more complicated).
 */

#ifdef __KERNEL__
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <string.h>
#   include <stdlib.h>
#   include <stdarg.h>
#   if defined(_WIN32) && defined(_MSC_VER)
#      include <windows.h>
#   endif
#   include "debug.h"
#   include "str.h"
#   include "strutil.h"
#endif

#if defined(VMTOOLS_USE_GLIB)
#  include "vmware/tools/guestrpc.h"
#  include "vmware/tools/utils.h"
#endif

#include "vmware.h"
#include "message.h"
#include "rpcin.h"

#if defined(VMTOOLS_USE_GLIB)

#define RPCIN_SCHED_EVENT(in, src) do {                                       \
   (in)->nextEvent = src;                                                     \
   g_source_set_callback((in)->nextEvent, RpcInLoop, (in), NULL);             \
   g_source_attach((in)->nextEvent, (in)->mainCtx);                           \
} while (0)

#else /* VMTOOLS_USE_GLIB */

#include "eventManager.h"

/* Which event queue should RPC events be added to? */
static DblLnkLst_Links *gTimerEventQueue;

/*
 * The RpcIn object
 */

/* The list of TCLO command callbacks we support */
typedef struct RpcInCallbackList {
   const char *name;
   size_t length; /* Length of name so we don't have to strlen a lot */
   RpcIn_Callback callback;
   struct RpcInCallbackList *next;
   void *clientData;
} RpcInCallbackList;

#endif /* VMTOOLS_USE_GLIB */

struct RpcIn {
#if defined(VMTOOLS_USE_GLIB)
   GSource *nextEvent;
   GMainContext *mainCtx;
   RpcIn_Callback dispatch;
   gpointer clientData;
#else
   RpcInCallbackList *callbacks;
   Event *nextEvent;
#endif

   Message_Channel *channel;
   unsigned int delay;   /* The delay of the previous iteration of RpcInLoop */
   unsigned int maxDelay;  /* The maximum delay to schedule in RpcInLoop */
   RpcIn_ErrorFunc *errorFunc;
   void *errorData;

   /*
    * State of the result associated to the last TCLO request we received
    */

   /* Should we send the result back? */
   Bool mustSend;

   /* The result itself */
   char *last_result;

   /* The size of the result */
   size_t last_resultLen;

   /*
    * It's possible for a callback dispatched by RpcInLoop to call RpcIn_stop.
    * When this happens, we corrupt the state of the RpcIn struct, resulting in
    * a crash the next time RpcInLoop is called. To prevent corruption of the
    * RpcIn struct, we check inLoop when RpcIn_stop is called, and if it is
    * true, we set shouldStop to TRUE instead of actually stopping the
    * channel. When RpcInLoop exits, it will stop the channel if shouldStop is
    * TRUE.
    */
   Bool inLoop;     // RpcInLoop is running.
   Bool shouldStop; // Stop the channel the next time RpcInLoop exits.
};


/*
 * The following functions are only needed in the non-glib version of the
 * library. The glib version of the library only deals with the transport
 * aspects of the code - RPC dispatching and other RPC-layer concerns are
 * handled by the rpcChannel abstraction library, or by the application.
 */

#if !defined(VMTOOLS_USE_GLIB)

/*
 *-----------------------------------------------------------------------------
 *
 * RpcInPingCallback --
 *
 *      Replies to a ping message from the VMX.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInPingCallback(char const **result,     // OUT
                  size_t *resultLen,       // OUT
                  const char *name,        // IN
                  const char *args,        // IN
                  size_t argsSize,         // IN
                  void *clientData)        // IN
{
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Construct --
 *
 *      Constructor for the RpcIn object.
 *
 * Results:
 *      New RpcIn object.
 *
 * Side effects:
 *      Sets the current timer event queue, allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

RpcIn *
RpcIn_Construct(DblLnkLst_Links *eventQueue)
{
   RpcIn *result;
   result = (RpcIn *)calloc(1, sizeof(RpcIn));

   gTimerEventQueue = result? eventQueue: NULL;
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInLookupCallback --
 *
 *      Lookup a callback struct in our list.
 *
 * Results:
 *      The callback if found
 *      NULL if not found
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static RpcInCallbackList *
RpcInLookupCallback(RpcIn *in,        // IN
                    const char *name) // IN
{
   RpcInCallbackList *p;

   ASSERT(in);
   ASSERT(name);

   for (p = in->callbacks; p; p = p->next) {
      if (strcmp(name, p->name) == 0) {
         return p;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_RegisterCallback --
 *
 *      Register an old-style callback to happen when a TCLO message is
 *      received. When a TCLO message beginning with 'name' is
 *      sent, the callback will be called with: the cmd name, the args
 *      (starting with the char directly after the cmd name; that's why
 *      it's helpful to add a space to the name if arguments are expected),
 *      and a pointer to the result.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_RegisterCallback(RpcIn *in,               // IN
                       const char *name,        // IN
                       RpcIn_Callback cb,       // IN
                       void *clientData)        // IN
{
   RpcInCallbackList *p;

   Debug("Registering callback '%s'\n", name);

   ASSERT(in);
   ASSERT(name);
   ASSERT(cb);
   ASSERT(RpcInLookupCallback(in, name) == NULL); // not there yet

   p = (RpcInCallbackList *) malloc(sizeof(RpcInCallbackList));
   ASSERT_NOT_IMPLEMENTED(p);

   p->length = strlen(name);
   p->name = strdup(name);
   p->callback = cb;
   p->clientData = clientData;

   p->next = in->callbacks;

   in->callbacks = p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_UnregisterCallback --
 *
 *      Unregisters an RpcIn callback by name.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_UnregisterCallback(RpcIn *in,               // IN
                         const char *name)        // IN
{
   RpcInCallbackList *cur, *prev;

   ASSERT(in);
   ASSERT(name);

   Debug("Unregistering callback '%s'\n", name);

   for (cur = in->callbacks, prev = NULL; cur && strcmp(cur->name, name);
        prev = cur, cur = cur->next);

   /*
    * If we called UnregisterCallback on a name that doesn't exist, we
    * have a problem.
    */
   ASSERT(cur != NULL);

   if (prev == NULL) {
      in->callbacks = cur->next;
   } else {
      prev->next = cur->next;
   }
   free((void *)cur->name);
   free(cur);
}


#else /* VMTOOLS_USE_GLIB */


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Construct --
 *
 *      Constructor for the RpcIn object. Ties the RpcIn loop to the given
 *      glib main loop, and uses the given callback to dispatch incoming
 *      RPC messages.
 *
 *      The dispatch callback receives data in a slightly different way than
 *      the regular RPC callbacks. Basically, the raw data from the backdoor
 *      is provided in the "args" field of the RpcInData struct, and "name"
 *      is NULL. So the dispatch function is responsible for parsing the RPC
 *      message, and preparing the RpcInData instance for proper use by the
 *      final consumer.
 *
 * Results:
 *      New RpcIn object.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

RpcIn *
RpcIn_Construct(GMainContext *mainCtx,    // IN
                RpcIn_Callback dispatch,  // IN
                gpointer clientData)      // IN
{
   RpcIn *result;

   ASSERT(mainCtx != NULL);
   ASSERT(dispatch != NULL);

   result = calloc(1, sizeof *result);
   if (result != NULL) {
      result->mainCtx = mainCtx;
      result->clientData = clientData;
      result->dispatch = dispatch;
   }
   return result;
}

#endif /* VMTOOLS_USE_GLIB */


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Destruct --
 *
 *      Destructor for the RpcIn object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees all memory associated with the RpcIn object, resets the global
 *      timer event queue.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_Destruct(RpcIn *in) // IN
{
   ASSERT(in);
   ASSERT(in->channel == NULL);
   ASSERT(in->nextEvent == NULL);
   ASSERT(in->mustSend == FALSE);

#if !defined(VMTOOLS_USE_GLIB)
   while (in->callbacks) {
      RpcInCallbackList *p;

      p = in->callbacks->next;
      free((void *) in->callbacks->name);
      free(in->callbacks);
      in->callbacks = p;
   }

   gTimerEventQueue = NULL;
#endif

   free(in);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInSend --
 *
 *      Send the last result back to VMware
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side-effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInSend(RpcIn *in) // IN
{
   Bool status;

   ASSERT(in);
   ASSERT(in->channel);
   ASSERT(in->mustSend);

   status = Message_Send(in->channel, (unsigned char *)in->last_result,
                         in->last_resultLen);
   if (status == FALSE) {
      Debug("RpcIn: couldn't send back the last result\n");
   }

   free(in->last_result);
   in->last_result = NULL;
   in->last_resultLen = 0;
   in->mustSend = FALSE;

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInStop --
 *
 *      Stop the RPC channel.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Sends the last result back to the host.
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInStop(RpcIn *in) // IN
{
   ASSERT(in);
   if (in->nextEvent) {
      /* The loop is started. Stop it */
#if defined(VMTOOLS_USE_GLIB)
      if (!in->inLoop) {
         g_source_destroy(in->nextEvent);
      }

      g_source_unref(in->nextEvent);
#else
      EventManager_Remove(in->nextEvent);
#endif
      in->nextEvent = NULL;
   }

   if (in->channel) {
      /* The channel is open */
      if (in->mustSend) {
         /* There is a final result to send back. Try to send it */
         RpcInSend(in);
         ASSERT(in->mustSend == FALSE);
      }

      /* Try to close the channel */
      if (Message_Close(in->channel) == FALSE) {
         Debug("RpcIn: couldn't close channel\n");
      }

      in->channel = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_stop --
 *
 *      Stop the RPC channel.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      Sends the last result to the host, if one exists.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_stop(RpcIn *in) // IN
{
   if (in->inLoop) {
      in->shouldStop = TRUE;
   } else {
      RpcInStop(in);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInLoop --
 *
 *      Receives an RPC from the host.
 *
 * Result:
 *      For the Event Manager implementation, always TRUE.
 *
 *      For the glib implementation, returns FALSE if the timer was rescheduled
 *      so that g_main_loop will unregister the old timer, or TRUE otherwise.
 *
 * Side-effects:
 *      Stops the RPC channel on error.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
static gboolean
#else
static Bool
#endif
RpcInLoop(void *clientData) // IN
{
   RpcIn *in;
   char const *errmsg;
   char const *reply;
   size_t repLen;

#if defined(VMTOOLS_USE_GLIB)
   unsigned int current;
   Bool resched = FALSE;
#endif

   in = (RpcIn *)clientData;
   ASSERT(in);
   ASSERT(in->nextEvent);
   ASSERT(in->channel);
   ASSERT(in->mustSend);

#if defined(VMTOOLS_USE_GLIB)
   current = in->delay;
#else
   /*
    * The event has fired: it is no longer valid. Note that this is
    * not true in the glib case!
    */
   in->nextEvent = NULL;
#endif

   in->inLoop = TRUE;

   /*
    * Workaround for bug 780404. Remove if we ever figure out the root cause.
    * Note that the ASSERT above catches this on non-release builds.
    */
   if (in->channel == NULL) {
      errmsg = "RpcIn: Channel is not active";
      goto error;
   }

   /*
    * This is very important: this is the only way to signal the existence of
    * this guest application to VMware.
    */
   if (RpcInSend(in) == FALSE) {
      errmsg = "RpcIn: Unable to send";
      goto error;
   }

   if (Message_Receive(in->channel, (unsigned char **)&reply, &repLen) == FALSE) {
      errmsg = "RpcIn: Unable to receive";
      goto error;
   }

   if (repLen) {
      unsigned int status;
      char const *statusStr;
      unsigned int statusLen;
      char *result;
      size_t resultLen;
      Bool freeResult = FALSE;

      /*
       * Execute the RPC
       */

#if defined(VMTOOLS_USE_GLIB)
      RpcInData data = { NULL, reply, repLen, NULL, 0, FALSE, NULL, in->clientData };

      status = in->dispatch(&data);
      result = data.result;
      resultLen = data.resultLen;
      freeResult = data.freeResult;
#else
      char *cmd;
      unsigned int index = 0;
      RpcInCallbackList *cb = NULL;

      cmd = StrUtil_GetNextToken(&index, reply, " ");
      if (cmd != NULL) {
         cb = RpcInLookupCallback(in, cmd);
         free(cmd);
         if (cb) {
            result = NULL;
            status = cb->callback((char const **) &result, &resultLen, cb->name,
                                  reply + cb->length, repLen - cb->length,
                                  cb->clientData);
            ASSERT(result);
         } else {
            status = FALSE;
            result = "Unknown Command";
            resultLen = strlen(result);
         }
      } else {
         status = FALSE;
         result = "Bad command";
         resultLen = strlen(result);
      }
#endif

      if (status) {
         statusStr = "OK ";
         statusLen = 3;
      } else {
         statusStr = "ERROR ";
         statusLen = 6;
      }

      in->last_result = (char *)malloc(statusLen + resultLen);
      if (in->last_result == NULL) {
         errmsg = "RpcIn: Not enough memory";
         goto error;
      }
      memcpy(in->last_result, statusStr, statusLen);
      memcpy(in->last_result + statusLen, result, resultLen);
      in->last_resultLen = statusLen + resultLen;

      if (freeResult) {
         free(result);
      }

      /*
       * Run the event pump (in case VMware sends a long sequence of RPCs and
       * perfoms a time-consuming job) and continue to loop immediately
       */
      in->delay = 0;
   } else {
      /*
       * Nothing to execute
       */

      /* No request -> No result */
      ASSERT(in->last_result == NULL);
      ASSERT(in->last_resultLen == 0);

      /*
       * Continue to loop in a while. Use an exponential back-off, doubling
       * the time to wait each time there isn't a new message, up to the max
       * delay.
       */

      if (in->delay < in->maxDelay) {
         if (in->delay > 0) {
            /*
             * Catch overflow.
             */
            in->delay = ((in->delay * 2) > in->delay) ? (in->delay * 2) : in->maxDelay;
         } else {
            in->delay = 1;
         }
         in->delay = MIN(in->delay, in->maxDelay);
      }
   }

   ASSERT(in->mustSend == FALSE);
   in->mustSend = TRUE;

   if (!in->shouldStop) {
#if defined(VMTOOLS_USE_GLIB)
      if (in->delay != current) {
         resched = TRUE;
         g_source_unref(in->nextEvent);
         RPCIN_SCHED_EVENT(in, VMTools_CreateTimer(in->delay * 10));
      }
#else
      in->nextEvent = EventManager_Add(gTimerEventQueue, in->delay, RpcInLoop, in);
#endif
      if (in->nextEvent == NULL) {
         errmsg = "RpcIn: Unable to run the loop";
         goto error;
      }
   }

exit:
   if (in->shouldStop) {
      RpcInStop(in);
      in->shouldStop = FALSE;
#if defined(VMTOOLS_USE_GLIB)
      /* Force the GMainContext to unref the GSource that runs the RpcIn loop. */
      resched = TRUE;
#endif
   }

   in->inLoop = FALSE;

#if defined(VMTOOLS_USE_GLIB)
   return !resched;
#else
   return TRUE;
#endif

error:
   /* Call the error routine */
   (*in->errorFunc)(in->errorData, errmsg);
   in->shouldStop = TRUE;
   goto exit;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_start --
 *
 *    Start the background loop that receives RPC from VMware
 *
 * Result
 *    TRUE on success
 *    FALSE on failure
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
Bool
RpcIn_start(RpcIn *in,                    // IN
            unsigned int delay,           // IN
            RpcIn_ErrorFunc *errorFunc,   // IN
            void *errorData)              // IN
#else
Bool
RpcIn_start(RpcIn *in,                    // IN
            unsigned int delay,           // IN
            RpcIn_Callback resetCallback, // IN
            void *resetClientData,        // IN
            RpcIn_ErrorFunc *errorFunc,   // IN
            void *errorData)              // IN
#endif
{
   ASSERT(in);

   in->delay = 0;
   in->maxDelay = delay;
   in->errorFunc = errorFunc;
   in->errorData = errorData;

   ASSERT(in->channel == NULL);
   in->channel = Message_Open(0x4f4c4354);
   if (in->channel == NULL) {
      Debug("RpcIn_start: couldn't open channel with TCLO protocol\n");
      goto error;
   }

   /* No initial result */
   ASSERT(in->last_result == NULL);
   ASSERT(in->last_resultLen == 0);
   ASSERT(in->mustSend == FALSE);
   in->mustSend = TRUE;

   ASSERT(in->nextEvent == NULL);
#if defined(VMTOOLS_USE_GLIB)
   RPCIN_SCHED_EVENT(in, VMTools_CreateTimer(in->delay * 10));
#else
   in->nextEvent = EventManager_Add(gTimerEventQueue, 0, RpcInLoop, in);
   if (in->nextEvent == NULL) {
      Debug("RpcIn_start: couldn't start the loop\n");
      goto error;
   }
#endif

#if !defined(VMTOOLS_USE_GLIB)
   /* Register the 'reset' handler */
   if (resetCallback) {
      RpcIn_RegisterCallback(in, "reset", resetCallback, resetClientData);
   }

   RpcIn_RegisterCallback(in, "ping", RpcInPingCallback, NULL);
#endif

   return TRUE;

error:
   RpcInStop(in);
   return FALSE;
}


#if !defined(VMTOOLS_USE_GLIB)
/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_SetRetVals --
 *
 *      Utility method to set the return values of a tclo command.
 *      Example:
 *          return RpcIn_SetRetVals(result, resultLen,
 *                                  "Message", FALSE);
 *
 * Results:
 *      retVal
 *
 * Side effects:
 *	Sets *result to resultVal & resultLen to strlen(*result).
 *
 *-----------------------------------------------------------------------------
 */

unsigned int
RpcIn_SetRetVals(char const **result,   // OUT
                 size_t *resultLen,     // OUT
                 const char *resultVal, // IN
                 Bool retVal)           // IN
{
   ASSERT(result);
   ASSERT(resultLen);
   ASSERT(resultVal);

   *result = resultVal;
   *resultLen = strlen(*result);

   return retVal;
}
#endif

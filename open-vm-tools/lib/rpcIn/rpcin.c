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



#include "vmware.h"
#include "message.h"
#include "eventManager.h"
#include "rpcin.h"
#include "dbllnklst.h"

/* Which event queue should RPC events be added to? */
static DblLnkLst_Links *gTimerEventQueue;

/*
 * The RpcIn object
 */

typedef enum {
    RPCIN_CB_OLD,
    RPCIN_CB_NEW
} RpcInCallbackType;


/* The list of TCLO command callbacks we support */
typedef struct RpcInCallbackList {
   const char *name;
   size_t length; /* Length of name so we don't have to strlen a lot */
   RpcInCallbackType type;
   union {
      RpcIn_CallbackOld oldCb;
      RpcIn_Callback newCb;
   } callback;
   struct RpcInCallbackList *next;
   void *clientData;
} RpcInCallbackList;

struct RpcIn {
   RpcInCallbackList *callbacks;

   Message_Channel *channel;
   unsigned int delay;   /* The delay of the previous iteration of RpcInLoop */
   unsigned int maxDelay;  /* The maximum delay to schedule in RpcInLoop */
   RpcIn_ErrorFunc *errorFunc;
   void *errorData;
   Event *nextEvent;

   /*
    * State of the result associated to the last TCLO request we received
    */

   /* Should we send the result back? */
   Bool mustSend;

   /* The result itself */
   char *last_result;

   /* The size of the result */
   size_t last_resultLen;
};


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
RpcInPingCallback(RpcInData *data)  // IN
{
   return RPCIN_SETRETVALS(data, "", TRUE);
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

   while (in->callbacks) {
      RpcInCallbackList *p;

      p = in->callbacks->next;
      free((void *) in->callbacks->name);
      free(in->callbacks);
      in->callbacks = p;
   }

   gTimerEventQueue = NULL;
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
 * RpcIn_stop --
 *
 *      Stop the background loop that receives RPC from VMware
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side-effects:
 *      Try to send the last result and to close the channel
 *
 *-----------------------------------------------------------------------------
 */

Bool
RpcIn_stop(RpcIn *in) // IN
{
   Bool status;

   ASSERT(in);

   status = TRUE;

   if (in->nextEvent) {
      /* The loop is started. Stop it */
      EventManager_Remove(in->nextEvent);

      in->nextEvent = NULL;
   }

   if (in->channel) {
      /* The channel is open */

      if (in->mustSend) {
         /* There is a final result to send back. Try to send it */
         if (RpcInSend(in) == FALSE) {
            status = FALSE;
         }

         ASSERT(in->mustSend == FALSE);
      }

      /* Try to close the channel */
      if (Message_Close(in->channel) == FALSE) {
         Debug("RpcIn: couldn't close channel\n");
         status = FALSE;
      }

      in->channel = NULL;
   }

   return status;
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

RpcInCallbackList *
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
 * RpcInLoop --
 *
 *    The background loop that receives RPC from VMware
 *
 * Result
 *    TRUE on success
 *    FALSE on failure (never happens in this implementation)
 *
 * Side-effects
 *    May call the error routine in which case the loop is
 *    stopped.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInLoop(void *clientData) // IN
{
   RpcIn *in;
   char const *errmsg;
   char const *reply;
   size_t repLen;

   in = (RpcIn *)clientData;
   ASSERT(in);

   /* The event has fired: it is no longer valid */
   ASSERT(in->nextEvent);
   in->nextEvent = NULL;

   /* This is very important: this is the only way to signal the existence
      of this guest application to VMware */
   ASSERT(in->channel);
   ASSERT(in->mustSend);
   if (RpcInSend(in) == FALSE) {
      errmsg = "RpcIn: Unable to send";
      goto error;
   }

   if (Message_Receive(in->channel, (unsigned char **)(char**)&reply, &repLen)
          == FALSE) {
      errmsg = "RpcIn: Unable to receive";
      goto error;
   }

   if (repLen) {
      unsigned int status;
      char const *statusStr;
      unsigned int statusLen;
      char *result;
      size_t resultLen;
      char *cmd;
      unsigned int index = 0;
      Bool freeResult = FALSE;
      RpcInCallbackList *cb = NULL;

      /*
       * Execute the RPC
       */

      cmd = StrUtil_GetNextToken(&index, reply, " ");
      if (cmd != NULL) {
         cb = RpcInLookupCallback(in, cmd);
         free(cmd);
         if (cb) {
            result = NULL;
            if (cb->type == RPCIN_CB_OLD) {
               status = cb->callback.oldCb((char const **) &result, &resultLen, cb->name,
                                           reply + cb->length, repLen - cb->length,
                                           cb->clientData);
            } else {
               RpcInData data = { cb->name,
                                  reply + cb->length,
                                  repLen - cb->length,
                                  NULL,
                                  0,
                                  FALSE,
                                  cb->clientData };
               status = cb->callback.newCb(&data);
               result = data.result;
               resultLen = data.resultLen;
               freeResult = data.freeResult;
            }

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

#if 0 /* Costly in non-debug cases --hpreg */
      if (strlen(reply) <= 128) {
         Debug("Tclo: Done executing '%s'; result='%s'\n", reply, result);
      } else {
         Debug("Tclo: reply string too long to display\n");
      }
#endif

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

   in->nextEvent = EventManager_Add(gTimerEventQueue, in->delay, RpcInLoop, in);
   if (in->nextEvent == NULL) {
      errmsg = "RpcIn: Unable to run the loop";
      goto error;
   }

   return TRUE;

error:
   RpcIn_stop(in);

   /* Call the error routine */
   (*in->errorFunc)(in->errorData, errmsg);

   return TRUE;
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

Bool
RpcIn_start(RpcIn *in,                    // IN
            unsigned int delay,           // IN
            RpcIn_Callback resetCallback, // IN
            void *resetClientData,        // IN
            RpcIn_ErrorFunc *errorFunc,   // IN
            void *errorData)              // IN
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
   in->nextEvent = EventManager_Add(gTimerEventQueue, 0, RpcInLoop, in);
   if (in->nextEvent == NULL) {
      Debug("RpcIn_start: couldn't start the loop\n");
      goto error;
   }

   /* Register the 'reset' handler */
   if (resetCallback) {
      RpcIn_RegisterCallbackEx(in, "reset", resetCallback, resetClientData);
   }

   RpcIn_RegisterCallbackEx(in, "ping", RpcInPingCallback, NULL);

   return TRUE;

error:
   RpcIn_stop(in);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_restart --
 *
 *    Stops/starts the background loop that receives RPC from VMware.
 *    Keeps already registered callbacks. Regardless of the value returned,
 *    callers are still expected to call RpcIn_stop() when done using rpcin,
 *    to properly release used resources.
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

Bool
RpcIn_restart(RpcIn *in)  // IN
{
   ASSERT(in);

   if (RpcIn_stop(in) == FALSE) {
      return FALSE;
   }

   ASSERT(in->channel == NULL);
   in->channel = Message_Open(0x4f4c4354);
   if (in->channel == NULL) {
      Debug("RpcIn_restart: couldn't open channel with TCLO protocol\n");
      return FALSE;
   }

   if (in->last_result) {
      free(in->last_result);
      in->last_result = NULL;
   }
   in->last_resultLen = 0;
   in->mustSend = TRUE;

   ASSERT(in->nextEvent == NULL);
   in->nextEvent = EventManager_Add(gTimerEventQueue, 0, RpcInLoop, in);
   if (in->nextEvent == NULL) {
      Debug("RpcIn_restart: couldn't start the loop\n");
      return FALSE;
   }

   return TRUE;
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
                       RpcIn_CallbackOld cb,    // IN
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
   p->type = RPCIN_CB_OLD;
   p->callback.oldCb = cb;
   p->clientData = clientData;

   p->next = in->callbacks;

   in->callbacks = p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_RegisterCallbackEx --
 *
 *      Register a callback to happen when a TCLO message is
 *      received. When a TCLO message beginning with 'name' is
 *      sent, the callback will be called with an instance of
 *      "RpcInData" with the information from the request.
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
RpcIn_RegisterCallbackEx(RpcIn *in,          // IN
                         const char *name,   // IN
                         RpcIn_Callback cb,  // IN
                         void *clientData)   // IN
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
   p->type = RPCIN_CB_NEW;
   p->callback.newCb = cb;
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


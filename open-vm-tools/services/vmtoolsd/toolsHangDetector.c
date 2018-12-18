/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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

/**
 * @file toolsHangDetector.c
 *
 *    Implementation of the tools hang detection and reporting
 */

#include <string.h>
#include <glib.h>

#include "vmware.h"
#include "vmware/tools/log.h"
#include "vmware/tools/threadPool.h"
#include "toolsHangDetector.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/guestrpc.h"

#define SLEEP_INTERVAL 1         /* approximately 1 second */
#define CHECKIN_INTERVAL 1       /* approximately 1 second */
#define COUNTER_RESET_VALUE 5    /* approximately 5 seconds */
#define STARVE_THRESHOLD 1.5

typedef enum {
   NORMAL,
   HUNG
} DetectedMode;

typedef struct HangDetectorState {
   /* 'mutex' and 'cond' protect concurrent accesses to 'terminate' flag */
   GMutex mutex;
   GCond cond;
   gboolean terminate;

   gint atomic;
   DetectedMode mode;
   GSource *checkinTimer;
   /*
    * Each slot records the time when the matched counter value is seen.
    * This helps figure out whether the detector thread was running under
    * a resource contention.
    */
   gint64 timeSeq[COUNTER_RESET_VALUE+1];
   gboolean vmxRejectedHealthUpdate;
} HangDetectorState;

static HangDetectorState gDetectorState;


/*
 ******************************************************************************
 * DetectorInit --                                                       */ /**
 *
 * Initialization
 *
 * Note: No need to call g_mutex_init, g_mutex_clear, g_cond_init,
 *       and g_cond_clear on statically allocated GMutex and GCond.
 *       Directly use them according to glib documentation.
 *
 ******************************************************************************
 */

static void
DetectorInit(void)
{
   HangDetectorState *state = &gDetectorState;

   state->terminate = FALSE;
   state->mode = NORMAL;
   state->vmxRejectedHealthUpdate = FALSE;
   g_atomic_int_set(&state->atomic, COUNTER_RESET_VALUE);
}


/*
 ******************************************************************************
 * DetectorFree --                                                       */ /**
 *
 * Resets any global state and frees up memory
 *
 * @param[in] args   Currently unused
 *
 ******************************************************************************
 */

static void
DetectorFree(UNUSED_PARAM(gpointer args))
{
   HangDetectorState *state = &gDetectorState;

   if (state->checkinTimer) {
      g_source_destroy(state->checkinTimer);
      g_source_unref(state->checkinTimer);
      state->checkinTimer = NULL;
   }
}


/*
 ******************************************************************************
 * DetectorTerminate --                                                  */ /**
 *
 * Signals the detector thread to exit.
 *
 * @param[in] ctx    Application context
 * @param[in] args   Currently unused
 *
 ******************************************************************************
 */

static void
DetectorTerminate(ToolsAppCtx *ctx,
                  UNUSED_PARAM(gpointer args))
{
   HangDetectorState *state = &gDetectorState;

   g_mutex_lock(&state->mutex);

   state->terminate = TRUE;
   g_cond_signal(&state->cond);

   g_mutex_unlock(&state->mutex);
}


/*
 ******************************************************************************
 * UpdateVmx --                                                         */ /**
 *
 * Notify the VMX about a tools service hang/recover event
 *
 * @param[in] event   event to put into the notification RPCI command.
 *
 ******************************************************************************
 */

static void
UpdateVmx(const char *event)
{
   HangDetectorState *state = &gDetectorState;
   RpcChannel *chan;
   gchar *msg;

   if (state->vmxRejectedHealthUpdate) {
      return;
   }

   chan = BackdoorChannel_New();
   if (NULL == chan) {
      g_warning("Failed to create a RPCI channel to send tools health event.\n");
      return;
   }

   if (!RpcChannel_Start(chan)) {
      g_warning("Failed to start a RPCI channel to send tools health event.\n");
      RpcChannel_Destroy(chan);
      return;
   }

   msg = g_strdup_printf("%s %s", UPDATE_TOOLS_HEALTH_CMD, event);
   ASSERT(NULL != msg);

   if(!RpcChannel_Send(chan, msg, strlen(msg), NULL, NULL)) {
      g_warning("Failed to send RPCI message: %s\n", msg);
      state->vmxRejectedHealthUpdate = TRUE;
   }

   g_free(msg);
   RpcChannel_Destroy(chan);
}


/*
 ******************************************************************************
 * GetTimeSeqString --                                                   */ /**
 *
 * The time sequence string is a sequence of elapsed time how long the detector
 * waited to run in the past.
 *
 * This is mainly for debugging right now.
 *
 * @return    the time sequence string in a statically allocated location.
 *            Do not free.
 *
 ******************************************************************************
 */

static const gchar *
GetTimeSeqString(void) {
   static gchar TimeSequence[COUNTER_RESET_VALUE * 8];
   gulong used = 0;
   gint i;
   HangDetectorState *state = &gDetectorState;

   for (i = 0; i < COUNTER_RESET_VALUE; ++i) {
      double elapsed = (state->timeSeq[i] - state->timeSeq[i+1])
         / (double)G_TIME_SPAN_SECOND;
      gulong left =  sizeof TimeSequence - used;
      gint ret;

      if (i == 0) {
         ret = g_snprintf(&TimeSequence[used], left, "%.2fs", elapsed);
      } else {
         ret = g_snprintf(&TimeSequence[used], left, ", %.2fs", elapsed);
      }

      if (ret < 0 || ret >= left) {
         TimeSequence[used] = '\0';
         break;
      }

      used += ret;
   }

   return TimeSequence;
}


/*
 ******************************************************************************
 * UpdateStateToHung --                                                  */ /**
 *
 * Update the current state to HUNG, and notify VMX
 *
 ******************************************************************************
 */

static void
UpdateStateToHung(void)
{
   HangDetectorState *state = &gDetectorState;
   double elapsed;

   state->mode = HUNG;

   elapsed = (state->timeSeq[0] - state->timeSeq[COUNTER_RESET_VALUE]) /
      (double)G_TIME_SPAN_SECOND;

   g_info("tools hang detector time sequence %s.", GetTimeSeqString());

   if (elapsed > SLEEP_INTERVAL * COUNTER_RESET_VALUE * STARVE_THRESHOLD) {
      g_info("tools service was slow for the last %.2f seconds.", elapsed);

      UpdateVmx(TOOLS_HEALTH_GUEST_SLOW_KEY);
   } else {
      g_info("tools service hung.");

      UpdateVmx(TOOLS_HEALTH_HUNG_KEY);
   }
}


/*
 ******************************************************************************
 * UpdateStateToNormal --                                                */ /**
 *
 * Update the current state to NORMAL, and notify VMX
 *
 ******************************************************************************
 */

static void
UpdateStateToNormal(void)
{
   HangDetectorState *state = &gDetectorState;

   state->mode = NORMAL;

   g_info("tools service recovered from a hang.");

   UpdateVmx(TOOLS_HEALTH_NORMAL_KEY);
}


/*
 ******************************************************************************
 * DetectorUpdate --                                                     */ /**
 *
 * Check the counter value and send proper updates to VMX
 *
 * @param[in] value   Counter value
 * @param[in] now     The current time stamp
 *
 ******************************************************************************
 */

static void
DetectorUpdate(gint value,
               gint64 now)
{
   HangDetectorState *state = &gDetectorState;

   if (value >= 0 && value <= COUNTER_RESET_VALUE) {
      state->timeSeq[value] = now;
   }

   if (state->mode == NORMAL) {
      if (value <= 0) {
         UpdateStateToHung();
      }
   } else {
      if (value > 0) {
         UpdateStateToNormal();
      }
   }
}


/*
 ******************************************************************************
 * SleepToExit --                                                        */ /**
 *
 * Sleep until the time specified, or return if the caller should terminate.
 *
 * @param[in] endTime  The end time stamp to sleep up to.
 *
 * @return TRUE if the caller should terminate, e.g. signaled by a terminator
 *         FALSE otherwise
 *
 ******************************************************************************
 */

static gboolean
SleepToExit(gint64 endTime)
{
   HangDetectorState *state = &gDetectorState;
   gboolean ret;

   g_mutex_lock(&state->mutex);

   while (!state->terminate) {
      if (!g_cond_wait_until(&state->cond, &state->mutex, endTime)) {
         /* endTime passed */
         ret = FALSE;
         goto exit;
      }
   }

   ret = TRUE;

exit:

   g_mutex_unlock(&state->mutex);

   return ret;
}


/*
 ******************************************************************************
 * DetectorThread --                                                     */ /**
 *
 * Detector thread entry function
 *
 * @param[in] ctx    Application context
 * @param[in] args   Currently unused
 *
 ******************************************************************************
 */

static void
DetectorThread(ToolsAppCtx *ctx,
               UNUSED_PARAM(gpointer args))
{
   HangDetectorState *state = &gDetectorState;

   while (1) {
      gint old = g_atomic_int_add(&state->atomic, -1);
      gint64 now = g_get_monotonic_time();
      gint64 endTime;

      DetectorUpdate(old, now);

      endTime = now + SLEEP_INTERVAL * G_TIME_SPAN_SECOND;
      if (SleepToExit(endTime)) {
         break;
      }
   }
}


/*
 ******************************************************************************
 * DetectorCheckin --                                                    */ /**
 *
 * Check in with the detector by resetting the counter
 *
 * @param[in] args   Currently unused
 *
 * @return TRUE   always, otherwise the event source is removed by glib
 *
 ******************************************************************************
 */

static gboolean
DetectorCheckin(UNUSED_PARAM(gpointer args))
{
   HangDetectorState *state = &gDetectorState;

   g_atomic_int_set(&state->atomic, COUNTER_RESET_VALUE);

   return TRUE;
}


/*
 ******************************************************************************
 * ScheduleCheckinTimer --                                               */ /**
 *
 * Schedule the periodic checkin timer with the main loop
 *
 * @param[in] ctx    Application Context
 *
 * @return TRUE iff timer is successfully scheduled
 *
 ******************************************************************************
 */

static gboolean
ScheduleCheckinTimer(ToolsAppCtx *ctx)
{
   HangDetectorState *state = &gDetectorState;
   GMainContext *mainCtx = g_main_loop_get_context(ctx->mainLoop);
   GSource *eventSource;

   ASSERT(NULL == state->checkinTimer);
   ASSERT(NULL != mainCtx);

   eventSource = VMTools_CreateTimer(CHECKIN_INTERVAL * 1000);

   if (NULL == eventSource) {
      return FALSE;
   }

   g_source_set_callback(eventSource, DetectorCheckin, NULL, NULL);
   g_source_attach(eventSource, mainCtx);

   state->checkinTimer = eventSource;

   return TRUE;
}


/*
 ******************************************************************************
 * ToolsCoreHangDetector_Start --                                        */ /**
 *
 * Register the checkin function to the tools main loop as a timer handler.
 * Start the detector thread to watch for the tools hang.
 *
 * @param[in] ctx    Application context.
 *
 * @return TRUE iff the hang detector is successfully started
 *
 ******************************************************************************
 */

gboolean
ToolsCoreHangDetector_Start(ToolsAppCtx *ctx)
{
   gboolean ret;
   GKeyFile *cfg = ctx->config;
   gboolean disabled;

   ASSERT(NULL != cfg);
   disabled = g_key_file_get_boolean(cfg, VMTOOLS_GUEST_SERVICE,
                                     "toolsHangDetectorDisabled",
                                     NULL);
   if (disabled) {
      g_info("tools hang detector is disabled");
      ret = FALSE;
      goto exit;
   }

   DetectorInit();

   ret = ScheduleCheckinTimer(ctx);
   if (!ret) {
      g_info("Unable to schedule hang detector checkin timer on the main loop");
      goto exit;
   }

   ret = ToolsCorePool_StartThread(ctx,
                                   "HangDetector",
                                   DetectorThread,
                                   DetectorTerminate,
                                   NULL,
                                   DetectorFree);
   if (!ret) {
      g_info("Unable to start the detector thread");
      DetectorFree(NULL);
   }

exit:

   return ret;
}


/*
 ******************************************************************************
 * ToolsCoreHangDetector_RpcReset --                                     */ /**
 *
 * Rpc Reset Handler for the tools hang detector module.
 * Just reset the vmxRejectedHealthUpdate boolean flag in case
 * the VM is migrated to a newer version of host that now supports the
 * health update.
 *
 ******************************************************************************
 */

void
ToolsCoreHangDetector_RpcReset(void)
{
   HangDetectorState *state = &gDetectorState;

   state->vmxRejectedHealthUpdate = FALSE;
}

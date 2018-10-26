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

#include <glib.h>

#include "vmware.h"
#include "vmware/tools/log.h"
#include "vmware/tools/threadPool.h"
#include "toolsHangDetector.h"


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
} HangDetectorState;

static HangDetectorState gDetectorState;

#define SLEEP_INTERVAL 1         /* approximately 1 second */
#define CHECKIN_INTERVAL 1       /* approximately 1 second */
#define COUNTER_RESET_VALUE 5    /* approximately 5 seconds */


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
   /* TBD */
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

   state->mode = HUNG;

   g_info("tools service hung.");

   UpdateVmx("hang");
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

   UpdateVmx("recover");
}


/*
 ******************************************************************************
 * DetectorUpdate --                                                     */ /**
 *
 * Check the counter value and send proper updates to VMX
 *
 * @param[in] value   Counter value
 *
 ******************************************************************************
 */

static void
DetectorUpdate(gint value)
{
   HangDetectorState *state = &gDetectorState;

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
 * Sleep for the time specified, or return if the caller should terminate.
 *
 * @param[in] time    Time in seconds to sleep
 *
 * @return TRUE if the caller should terminate, e.g. signaled by a terminator
 *         FALSE otherwise
 *
 ******************************************************************************
 */

static gboolean
SleepToExit(gint time)
{
   HangDetectorState *state = &gDetectorState;
   gint64 endTime = g_get_monotonic_time() + time * G_TIME_SPAN_SECOND;
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
      DetectorUpdate(old);

      if (SleepToExit(SLEEP_INTERVAL)) {
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

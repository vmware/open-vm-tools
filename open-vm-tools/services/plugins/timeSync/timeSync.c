/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file timeSync.c
 *
 * Plugin to handle time synchronization between the guest and host.
 */

#define G_LOG_DOMAIN "timeSync"

/* sync the time once a minute */
#define TIME_SYNC_TIME     60
/* only PERCENT_CORRECTION percent is corrected everytime */
#define PERCENT_CORRECTION   50


#include "backdoor.h"
#include "backdoor_def.h"
#include "conf.h"
#include "msg.h"
#include "strutil.h"
#include "system.h"
#include "vm_app.h"
#include "vmtools.h"
#include "vmtoolsApp.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


typedef struct TimeSyncData {
   gboolean    slewCorrection;
   uint32      slewPercentCorrection;
   gint        timeSyncState;
   uint32      timeSyncPeriod;         /* In seconds. */
   GSource    *timer;
} TimeSyncData;


/**
 * Set the guest OS time to the host OS time.
 *
 * @param[in]  slewCorrection    Is clock slewing enabled?
 * @param[in]  syncOnce          Is this function called in a loop?
 * @param[in]  allowBackwardSync Can we sync time backwards when doing syncOnce?
 * @param[in]  _data             Time sync data.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncDoSync(Bool slewCorrection,
               Bool syncOnce,
               Bool allowBackwardSync,
               void *_data)
{
   Backdoor_proto bp;
   int64 maxTimeLag;
   int64 interruptLag;
   int64 guestSecs;
   int64 guestUsecs;
   int64 hostSecs;
   int64 hostUsecs;
   int64 diffSecs;
   int64 diffUsecs;
   int64 diff;
   TimeSyncData *data = _data;
   Bool timeLagCall = FALSE;
#ifdef VMX86_DEBUG
   static int64 lastHostSecs = 0;
   int64 secs1, usecs1;
   int64 secs2, usecs2;

   System_GetCurrentTime(&secs1, &usecs1);
#endif

   g_debug("Synchronizing time.\n");

   /*
    * We need 3 things from the host, and there exist 3 different versions of
    * the calls (described further below):
    * 1) host time
    * 2) maximum time lag allowed (config option), which is a
    *    threshold that keeps the tools from being over eager about
    *    resetting the time when it is only a little bit off.
    * 3) interrupt lag
    *
    * First 2 versions of the call add interrupt lag to the maximum allowed
    * time lag, where as in the last call it is returned separately.
    *
    * Three versions of the call:
    *
    * - BDOOR_CMD_GETTIME: suffers from a 136-year overflow problem that
    *   cannot be corrected without breaking backwards compatibility with
    *   older Tools. So, we have the newer BDOOR_CMD_GETTIMEFULL, which is
    *   overflow safe.
    *
    * - BDOOR_CMD_GETTIMEFULL: overcomes the problem above.
    *
    * - BDOOR_CMD_GETTIMEFULL_WITH_LAG: Both BDOOR_CMD_GETTIMEFULL and
    *   BDOOR_CMD_GETTIME returns max lag limit as interrupt lag + the maximum
    *   allowed time lag. BDOOR_CMD_GETTIMEFULL_WITH_LAG separates these two
    *   values. This is helpful when synchronizing time backwards by slewing
    *   the clock.
    *
    * We use BDOOR_CMD_GETTIMEFULL_WITH_LAG first and fall back to
    * BDOOR_CMD_GETTIMEFULL or BDOOR_CMD_GETTIME.
    *
    * Note that BDOOR_CMD_GETTIMEFULL and BDOOR_CMD_GETTIMEFULL_WITH_LAG will
    * not touch EAX when it succeeds. So we check for errors by comparing EAX to
    * BDOOR_MAGIC, which was set by the call to Backdoor() prior to touching the
    * backdoor port.
    */
   bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL_WITH_LAG;
   Backdoor(&bp);
   if (bp.out.ax.word == BDOOR_MAGIC) {
      hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
      interruptLag = bp.out.di.word;
      timeLagCall = TRUE;
      g_debug("Using BDOOR_CMD_GETTIMEFULL_WITH_LAG\n");
   } else {
      g_debug("BDOOR_CMD_GETTIMEFULL_WITH_LAG not supported by current host, "
              "attempting BDOOR_CMD_GETTIMEFULL\n");
      interruptLag = 0;
      bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL;
      Backdoor(&bp);
      if (bp.out.ax.word == BDOOR_MAGIC) {
         hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
      } else {
         g_debug("BDOOR_CMD_GETTIMEFULL not supported by current host, "
                 "attempting BDOOR_CMD_GETTIME\n");
         bp.in.cx.halfs.low = BDOOR_CMD_GETTIME;
         Backdoor(&bp);
         hostSecs = bp.out.ax.word;
      }
   }
   hostUsecs = bp.out.bx.word;
   maxTimeLag = bp.out.cx.word;

   if (hostSecs <= 0) {
      g_warning("Invalid host OS time: %"FMT64"d secs, %"FMT64"d usecs.\n\n",
                hostSecs, hostUsecs);
      return FALSE;
   }

   /* Get the guest OS time */
   if (!System_GetCurrentTime(&guestSecs, &guestUsecs)) {
      g_warning("Unable to retrieve the guest OS time: %s.\n\n", Msg_ErrString());
      return FALSE;
   }

   diffSecs = hostSecs - guestSecs;
   diffUsecs = hostUsecs - guestUsecs;
   if (diffUsecs < 0) {
      diffSecs -= 1;
      diffUsecs += 1000000U;
   }
   diff = diffSecs * 1000000L + diffUsecs;

#ifdef VMX86_DEBUG
   g_debug("Daemon: Guest clock lost %.6f secs; limit=%.2f; "
           "%"FMT64"d secs since last update\n",
           diff / 1000000.0, maxTimeLag / 1000000.0, hostSecs - lastHostSecs);
   g_debug("Daemon: %d, %d, %"FMT64"d, %"FMT64"d, %"FMT64"d.\n",
           syncOnce, slewCorrection, diff, maxTimeLag, interruptLag);
   lastHostSecs = hostSecs;
#endif

   if (syncOnce) {
      /*
       * Non-loop behavior:
       *
       * Perform a step correction if:
       * 1) The guest OS is behind the host OS by more than maxTimeLag + interruptLag.
       * 2) The guest OS is ahead of the host OS.
       */
      if (diff > maxTimeLag + interruptLag || (diff < 0 && allowBackwardSync)) {
         System_DisableTimeSlew();
         if (!System_AddToCurrentTime(diffSecs, diffUsecs)) {
            g_warning("Unable to set the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      }
   } else {

      /*
       * Loop behavior:
       *
       * If guest is behind host by more than maxTimeLag + interruptLag
       * perform a step correction to the guest clock and ask the monitor
       * to drop its accumulated catchup (interruptLag).
       *
       * Otherwise, perform a slew correction.  Adjust the guest's clock
       * rate to be either faster or slower than nominal real time, such
       * that we expect to correct correctionPercent percent of the error
       * during this synchronization cycle.
       */

      if (diff > maxTimeLag + interruptLag) {
         System_DisableTimeSlew();
         if (!System_AddToCurrentTime(diffSecs, diffUsecs)) {
            g_warning("Unable to set the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else if (slewCorrection && timeLagCall) {
         int64 slewDiff;
         int64 timeSyncPeriodUS = data->timeSyncPeriod * 1000000;

         /* Don't consider interruptLag during clock slewing. */
         slewDiff = diff - interruptLag;

         /* Correct only data->slewPercentCorrection percent error. */
         slewDiff = (data->slewPercentCorrection * slewDiff) / 100;

         if (!System_EnableTimeSlew(slewDiff, timeSyncPeriodUS)) {
            g_warning("Unable to slew the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else {
         System_DisableTimeSlew();
      }
   }

#ifdef VMX86_DEBUG
      System_GetCurrentTime(&secs2, &usecs2);

      g_debug("Time changed from %"FMT64"d.%"FMT64"d -> %"FMT64"d.%"FMT64"d\n",
              secs1, usecs1, secs2, usecs2);
#endif

   /*
    * If we have stepped the time, ask TimeTracker to reset to normal the rate
    * of timer interrupts it forwards from the host to the guest.
    */
   if (!System_IsTimeSlewEnabled()) {
      bp.in.cx.halfs.low = BDOOR_CMD_STOPCATCHUP;
      Backdoor(&bp);
   }

   return TRUE;
}


/**
 * Run the "time synchronization" loop.
 *
 * @param[in]  _data    Time sync data.
 *
 * @return TRUE on success.
 */

static gboolean
ToolsDaemonTimeSyncLoop(gpointer _data)
{
   TimeSyncData *data = _data;

   g_assert(data != NULL);

   if (!TimeSyncDoSync(data->slewCorrection, FALSE, FALSE, data)) {
      g_warning("Unable to synchronize time.\n");
      if (data->timer != NULL) {
         g_source_unref(data->timer);
         data->timer = NULL;
      }
      return FALSE;
   }

   return TRUE;
}


#if defined(_WIN32)
/**
 * Try to disable the Windows Time Daemon.
 *
 * @return TRUE on success.
 */

static Bool
TimeSyncDisableWinTimeDaemon(void)
{
   DWORD timeAdjustment;
   DWORD timeIncrement;
   DWORD error;
   BOOL timeAdjustmentDisabled;
   BOOL success = FALSE;

   /*
    * We need the SE_SYSTEMTIME_NAME privilege to make the change; get
    * the privilege now (or bail if we can't).
    */
   success = System_SetProcessPrivilege(SE_SYSTEMTIME_NAME, TRUE);
   if (!success) {
      return FALSE;
   }

   /* Actually try to stop the time daemon. */
   if (GetSystemTimeAdjustment(&timeAdjustment, &timeIncrement,
                               &timeAdjustmentDisabled)) {
      g_debug("GetSystemTimeAdjustment() succeeded: timeAdjustment %d,"
              "timeIncrement %d, timeAdjustmentDisabled %s\n",
              timeAdjustment, timeIncrement,
              timeAdjustmentDisabled ? "TRUE" : "FALSE");
      /*
       * timeAdjustmentDisabled means the opposite of what you'd think;
       * if it's TRUE, that means the system may be adjusting the time
       * on its own using the time daemon. Read MSDN for the details,
       * and see Bug 24173 for more discussion on this.
       */

      if (timeAdjustmentDisabled) {
         /*
          * MSDN is a bit vague on the semantics of this function, but it
          * would appear that the timeAdjustment value here is simply the
          * total amount that the system will add to the clock on each
          * timer tick, i.e. if you set it to zero the system clock will
          * not progress at all (and indeed, attempting to set it to zero
          * results in an ERROR_INVALID_PARAMETER). In order to have time
          * proceed at the normal rate, this needs to be set to the value
          * of timeIncrement retrieved from GetSystemTimeAdjustment().
          */
         if (!SetSystemTimeAdjustment(timeIncrement, FALSE)) {
            error = GetLastError();
            g_debug("SetSystemTimeAdjustment failed: %d\n", error);
            goto exit;
         }
      }
   } else {
      error = GetLastError();
      g_debug("GetSystemTimeAdjustment failed: %d\n", error);
      goto exit;
   }

   success = TRUE;

  exit:
   g_debug("Stopping time daemon %s.\n", success ? "succeeded" : "failed");
   System_SetProcessPrivilege(SE_SYSTEMTIME_NAME, FALSE);
   return success;
}
#endif


/**
 * Start or stop the "time synchronization" loop. Nothing will be done if
 * start==TRUE & it's already running or start==FALSE & it's not running.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  data     Time sync data.
 * @param[in]  start    See above.
 *
 * @return TRUE on success.
 */

static Bool
TimeSyncStartStopLoop(ToolsAppCtx *ctx,
                      TimeSyncData *data,
                      gboolean start)
{
   g_assert(data != NULL);

   if (start && data->timer == NULL) {
      g_debug("Starting time sync loop.\n");
      g_debug("New sync period is %d sec.\n", data->timeSyncPeriod);
      if (!ToolsDaemonTimeSyncLoop(data)) {
         return FALSE;
      }
      data->timer = g_timeout_source_new(data->timeSyncPeriod * 1000);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, data->timer, ToolsDaemonTimeSyncLoop, data, NULL);

#if defined(_WIN32)
      g_debug("Daemon: Attempting to disable Windows Time daemon\n");
      if (!TimeSyncDisableWinTimeDaemon()) {
         g_debug("Daemon: Failed to disable Windows Time daemon\n");
      }
#endif

      return TRUE;
   } else if (!start && data->timer != NULL) {
      g_debug("Stopping time sync loop.\n");
      System_DisableTimeSlew();
      g_source_destroy(data->timer);
      data->timer = NULL;

      return TRUE;
   }

   /*
    * No need to start time sync b/c it's already running or no
    * need to stop it b/c it's not running.
    */
   return TRUE;
}


/**
 * Sync the guest's time with the host's.
 *
 * @param[in]  data     RPC request data.
 *
 * @return TRUE on success.
 */

static Bool
TimeSyncTcloHandler(RpcInData *data)
{
   Bool backwardSync = !strcmp(data->args, "1");
   TimeSyncData *syncData = data->clientData;

   if (!TimeSyncDoSync(syncData->slewCorrection, TRUE, backwardSync, syncData)) {
      return RPCIN_SETRETVALS(data, "Unable to sync time", FALSE);
   } else {
      return RPCIN_SETRETVALS(data, "", TRUE);
   }
}


/**
 * Parses boolean option string.
 *
 * @param[in]  string     Option string to be parsed.
 * @param[out] gboolean   Value of the option.
 *
 * @return TRUE on success.
 */

static gboolean
ParseBoolOption(const gchar *string,
                gboolean *value)
{
      if (strcmp(string, "1") == 0) {
         *value = TRUE;
      } else if (strcmp(string, "0") == 0) {
         *value = FALSE;
      } else {
         return FALSE;
      }
      return TRUE;
}

/**
 * Handles a "Set_Option" callback. Processes the time sync related options.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  option   Option being set.
 * @param[in]  value    Option value.
 * @param[in]  plugin   Plugin registration data.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncSetOption(gpointer src,
                  ToolsAppCtx *ctx,
                  const gchar *option,
                  const gchar *value,
                  ToolsPluginData *plugin)
{
   static gboolean syncBeforeLoop;
   TimeSyncData *data = plugin->_private;

   if (strcmp(option, TOOLSOPTION_SYNCTIME) == 0) {
      gboolean start;
      if (!ParseBoolOption(value, &start)) {
         return FALSE;
      }

      /*
       * Try the one-shot time sync if time sync transitions from
       * 'off' to 'on' and TOOLSOPTION_SYNCTIME_ENABLE is turned on.
       * Note that during startup we receive TOOLSOPTION_SYNCTIME
       * before receiving TOOLSOPTION_SYNCTIME_ENABLE and so the
       * one-shot sync will not be done here. Nor should it because
       * the startup synchronization behavior is controlled by
       * TOOLSOPTION_SYNCTIME_STARTUP which is handled separately.
       */
      if (data->timeSyncState == 0 && start && syncBeforeLoop) {
            TimeSyncDoSync(data->slewCorrection, TRUE, TRUE, data);
      }

      /* Now start/stop the loop. */
      if (!TimeSyncStartStopLoop(ctx, data, start)) {
         return FALSE;
      }

      data->timeSyncState = start;

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_SLEWCORRECTION) == 0) {
      data->slewCorrection = strcmp(value, "0");
      g_debug("Daemon: Setting slewCorrection, %d.\n", data->slewCorrection);

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERCENTCORRECTION) == 0) {
      int32 percent;

      g_debug("Daemon: Setting slewPercentCorrection to %s.\n", value);
      if (!StrUtil_StrToInt(&percent, value)) {
         return FALSE;
      }
      if (percent <= 0 || percent > 100) {
         data->slewPercentCorrection = PERCENT_CORRECTION;
      } else {
         data->slewPercentCorrection = percent;
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERIOD) == 0) {
      uint32 period;

      if (!StrUtil_StrToUint(&period, value)) {
         return FALSE;
      }

      /*
       * If the sync loop is running and the time sync period has changed,
       * restart the loop with the new period value. If the sync loop is
       * not running, just remember the new sync period value.
       */
      if (period != data->timeSyncPeriod) {
         data->timeSyncPeriod = (period > 0) ? period : TIME_SYNC_TIME;

         if (data->timer != NULL) {
            TimeSyncStartStopLoop(ctx, data, FALSE);
            if (!TimeSyncStartStopLoop(ctx, data, TRUE)) {
               g_warning("Unable to change time sync period.\n");
               return FALSE;
            }
         }
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_STARTUP) == 0) {
      static gboolean doneAlready = FALSE;
      gboolean doSync;

      if (!ParseBoolOption(value, &doSync)) {
         return FALSE;
      }

      if (doSync && !doneAlready &&
            !TimeSyncDoSync(data->slewCorrection, TRUE, TRUE, data)) {
         g_warning("Unable to sync time during startup.\n");
         return FALSE;
      }

      doneAlready = TRUE;

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_ENABLE) == 0) {
      if (!ParseBoolOption(value, &syncBeforeLoop)) {
         return FALSE;
      }

   } else {
      return FALSE;
   }

   return TRUE;
}


/**
 * Handles a shutdown callback; cleans up internal plugin state.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 */

static void
TimeSyncShutdown(gpointer src,
                 ToolsAppCtx *ctx,
                 ToolsPluginData *plugin)
{
   TimeSyncData *data = plugin->_private;
   if (data->timer != NULL) {
      g_source_destroy(data->timer);
   }
   g_free(data);
}


/**
 * Plugin entry point. Initializes internal state and returns the registration
 * data.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "timeSync",
      NULL,
      NULL
   };

   TimeSyncData *data = g_malloc(sizeof (TimeSyncData));
   RpcChannelCallback rpcs[] = {
      { "Time_Synchronize", TimeSyncTcloHandler, data, NULL, NULL, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_SET_OPTION, TimeSyncSetOption, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, TimeSyncShutdown, &regData }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   data->slewCorrection = FALSE;
   data->slewPercentCorrection = PERCENT_CORRECTION;
   data->timeSyncState = -1;
   data->timeSyncPeriod = TIME_SYNC_TIME;
   data->timer = NULL;

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   regData._private = data;

   return &regData;
}


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

#include "timeSync.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "conf.h"
#include "msg.h"
#include "strutil.h"
#include "system.h"
#include "vmware/guestrpc/timesync.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


/* Sync the time once a minute. */
#define TIMESYNC_TIME 60
/* Correct PERCENT_CORRECTION percent of the error each period. */
#define TIMESYNC_PERCENT_CORRECTION 50

/* When measuring the difference between time on the host and time in the
 * guest we try up to TIMESYNC_MAX_SAMPLES times to read a sample
 * where the two host reads are within TIMESYNC_GOOD_SAMPLE_THRESHOLD
 * microseconds. */
#define TIMESYNC_MAX_SAMPLES 4
#define TIMESYNC_GOOD_SAMPLE_THRESHOLD 2000

typedef enum TimeSyncState {
   TIMESYNC_INITIALIZING,
   TIMESYNC_STOPPED,
   TIMESYNC_RUNNING,
} TimeSyncState;

typedef struct TimeSyncData {
   gboolean       slewCorrection;
   uint32         slewPercentCorrection;
   uint32         timeSyncPeriod;         /* In seconds. */
   TimeSyncState  state;
   GSource        *timer;
} TimeSyncData;


/**
 * Read the time reported by the Host OS.
 *
 * @param[out]  host                Time on the Host.
 * @param[out]  apparentError       Apparent time error = apparent - real.
 * @param[out]  apparentErrorValid  Did the platform inform us of apparentError.
 * @param[out]  maxTimeError        Maximum amount of error than can go.
 *                                  uncorrected.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncReadHost(int64 *host, int64 *apparentError, Bool *apparentErrorValid,
                 int64 *maxTimeError)
{
   Backdoor_proto bp;
   int64 maxTimeLag;
   int64 interruptLag;
   int64 hostSecs;
   int64 hostUsecs;
   Bool timeLagCall;

   /*
    * We need 3 things from the host, and there exist 3 different versions of
    * the calls (described further below):
    * 1) host time
    * 2) maximum time lag allowed (config option), which is a
    *    threshold that keeps the tools from being over eager about
    *    resetting the time when it is only a little bit off.
    * 3) interrupt lag (the amount that apparent time lags real time)
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
      timeLagCall = FALSE;
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

   *host = hostSecs * US_PER_SEC + hostUsecs;
   *apparentError = -interruptLag;
   *apparentErrorValid = timeLagCall;
   *maxTimeError = maxTimeLag;

   if (hostSecs <= 0) {
      g_warning("Invalid host OS time: %"FMT64"d secs, %"FMT64"d usecs.\n\n",
                hostSecs, hostUsecs);
      return FALSE;
   }

   return TRUE;
}


/**
 * Read the Guest OS time and the Host OS time.
 *
 * There are three time domains that are revelant here:
 * 1. Guest time     - the time reported by the guest
 * 2. Apparent time  - the time reported by the virtualization layer
 * 3. Host time      - the time reported by the host operating system.
 *
 * This function reports the host time, the guest time and the difference
 * between apparent time and host time (apparentError).  The host and
 * guest time may be sampled multiple times to ensure an accurate reading.
 *
 * @param[out]  host                Time on the Host.
 * @param[out]  guest               Time in the Guest.
 * @param[out]  apparentError       Apparent time error = apparent - real.
 * @param[out]  apparentErrorValid  Did the platform inform us of apparentError.
 * @param[out]  maxTimeError        Maximum amount of error than can go.
 *                                  uncorrected.
 *
 * @return TRUE on success.
 */

static gboolean
TimeSyncReadHostAndGuest(int64 *host, int64 *guest, 
                         int64 *apparentError, Bool *apparentErrorValid,
                         int64 *maxTimeError)
{
   int64 host1, host2, hostDiff;
   int64 tmpGuest, tmpApparentError, tmpMaxTimeError;
   Bool tmpApparentErrorValid;
   int64 bestHostDiff = MAX_INT64;
   int iter = 0;
   DEBUG_ONLY(static int64 lastHost = 0);

   *apparentErrorValid = FALSE;
   *host = *guest = *apparentError = *maxTimeError = 0;

   if (!TimeSyncReadHost(&host2, &tmpApparentError, 
                         &tmpApparentErrorValid, &tmpMaxTimeError)) {
      return FALSE;
   }

   do {
      iter++;
      host1 = host2;

      if (!TimeSync_GetCurrentTime(&tmpGuest)) {
         g_warning("Unable to retrieve the guest OS time: %s.\n\n", 
                   Msg_ErrString());
         return FALSE;
      }
      
      if (!TimeSyncReadHost(&host2, &tmpApparentError, 
                            &tmpApparentErrorValid, &tmpMaxTimeError)) {
         return FALSE;
      }
      
      if (host1 < host2) {
         hostDiff = host2 - host1;
      } else {
         hostDiff = 0;
      }

      if (hostDiff <= bestHostDiff) {
         bestHostDiff = hostDiff;
         *host = host1 + hostDiff / 2;
         *guest = tmpGuest;
         *apparentError = tmpApparentError;
         *apparentErrorValid = tmpApparentErrorValid;
         *maxTimeError = tmpMaxTimeError;
      }
   } while (iter < TIMESYNC_MAX_SAMPLES && 
            bestHostDiff > TIMESYNC_GOOD_SAMPLE_THRESHOLD);

   ASSERT(*host != 0 && *guest != 0);

#ifdef VMX86_DEBUG
   g_debug("Daemon: Guest vs host error %.6fs; guest vs apparent error %.6fs; "
           "limit=%.2fs; apparentError %.6fs; iter=%d error=%.6fs; "
           "%.6f secs since last update\n",
           (*guest - *host) / 1000000.0, 
           (*guest - *host - *apparentError) / 1000000.0, 
           *maxTimeError / 1000000.0, *apparentError / 1000000.0,
           iter, bestHostDiff / 1000000.0,
           (*host - lastHost) / 1000000.0);
   lastHost = *host;
#endif

   return TRUE;
}


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
   int64 guest, host;
   int64 gosError, apparentError, maxTimeError;
   Bool apparentErrorValid;
   TimeSyncData *data = _data;

#ifdef VMX86_DEBUG
   int64 before;
   int64 after;
   TimeSync_GetCurrentTime(&before);
#endif

   g_debug("Synchronizing time: "
           "syncOnce %d, slewCorrection %d, allowBackwardSync %d.\n",
           syncOnce, slewCorrection, allowBackwardSync);

   if (!TimeSyncReadHostAndGuest(&host, &guest, &apparentError, 
                                 &apparentErrorValid, &maxTimeError)) {
      return FALSE;
   }

   gosError = guest - host - apparentError;

   if (syncOnce) {
      /*
       * Non-loop behavior:
       *
       * Perform a step correction if:
       * 1) The guest OS error is behind by more than maxTimeError.
       * 2) The guest OS is ahead of the host OS.
       */
      if (gosError < -maxTimeError || 
          (gosError + apparentError > 0 && allowBackwardSync)) {
         TimeSync_DisableTimeSlew();
         if (!TimeSync_AddToCurrentTime(-gosError + -apparentError)) {
            g_warning("Unable to set the guest OS time: %s.\n\n", 
                      Msg_ErrString());
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

      if (gosError < -maxTimeError) {
         TimeSync_DisableTimeSlew();
         if (!TimeSync_AddToCurrentTime(-gosError + -apparentError)) {
            g_warning("Unable to set the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else if (slewCorrection && apparentErrorValid) {
         int64 timeSyncPeriodUS = data->timeSyncPeriod * US_PER_SEC;
         int64 slewDiff = (-gosError * data->slewPercentCorrection) / 100;

         if (!TimeSync_EnableTimeSlew(slewDiff, timeSyncPeriodUS)) {
            g_warning("Unable to slew the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else {
         TimeSync_DisableTimeSlew();
      }
   }

#ifdef VMX86_DEBUG
      TimeSync_GetCurrentTime(&after);

      g_debug("Time changed from %"FMT64"d.%06"FMT64"d -> "
              "%"FMT64"d.%06"FMT64"d\n",
              before / US_PER_SEC, before % US_PER_SEC, 
              after / US_PER_SEC, after % US_PER_SEC);
#endif

   /*
    * If we have stepped the time, ask TimeTracker to reset to normal the rate
    * of timer interrupts it forwards from the host to the guest.
    */
   if (!TimeSync_IsTimeSlewEnabled()) {
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
 * @return always TRUE.
 */

static gboolean
ToolsDaemonTimeSyncLoop(gpointer _data)
{
   TimeSyncData *data = _data;

   ASSERT(data != NULL);

   if (!TimeSyncDoSync(data->slewCorrection, FALSE, FALSE, data)) {
      g_warning("Unable to synchronize time.\n");
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
 * Start the "time synchronization" loop.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  data     Time sync data.
 *
 * @return TRUE on success.
 */

static Bool
TimeSyncStartLoop(ToolsAppCtx *ctx,
                  TimeSyncData *data)
{
   ASSERT(data != NULL);
   ASSERT(data->state != TIMESYNC_RUNNING);
   ASSERT(data->timer == NULL);

   g_debug("Starting time sync loop.\n");

#if defined(_WIN32)
   g_debug("Daemon: Attempting to disable Windows Time daemon\n");
   if (!TimeSyncDisableWinTimeDaemon()) {
      g_warning("Daemon: Failed to disable Windows Time daemon\n");
   }
#endif

   g_debug("New sync period is %d sec.\n", data->timeSyncPeriod);

   if (!TimeSyncDoSync(data->slewCorrection, FALSE, FALSE, data)) {
      g_warning("Unable to synchronize time when starting time loop.\n");
   }

   data->timer = g_timeout_source_new(data->timeSyncPeriod * 1000);
   VMTOOLSAPP_ATTACH_SOURCE(ctx, data->timer, ToolsDaemonTimeSyncLoop, data, NULL);

   data->state = TIMESYNC_RUNNING;
   return TRUE;
}


/**
 * Stop the "time synchronization" loop.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  data     Time sync data.
 */

static void
TimeSyncStopLoop(ToolsAppCtx *ctx,
                 TimeSyncData *data)
{
   ASSERT(data != NULL);
   ASSERT(data->state == TIMESYNC_RUNNING);
   ASSERT(data->timer != NULL);

   g_debug("Stopping time sync loop.\n");

   TimeSync_DisableTimeSlew();

   g_source_destroy(data->timer);
   data->timer = NULL;

   data->state = TIMESYNC_STOPPED;
}


/**
 * Sync the guest's time with the host's.
 *
 * @param[in]  data     RPC request data.
 *
 * @return TRUE on success.
 */

static gboolean
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

      if (start && data->state != TIMESYNC_RUNNING) {
         /*
          * Try the one-shot time sync if time sync transitions from
          * 'off' to 'on' and TOOLSOPTION_SYNCTIME_ENABLE is turned on.
          * Note that during startup we receive TOOLSOPTION_SYNCTIME
          * before receiving TOOLSOPTION_SYNCTIME_ENABLE and so the
          * one-shot sync will not be done here. Nor should it because
          * the startup synchronization behavior is controlled by
          * TOOLSOPTION_SYNCTIME_STARTUP which is handled separately.
          */
         if (data->state == TIMESYNC_STOPPED && syncBeforeLoop) {
            TimeSyncDoSync(data->slewCorrection, TRUE, TRUE, data);
         }

         if (!TimeSyncStartLoop(ctx, data)) {
            g_warning("Unable to change time sync period.\n");
            return FALSE;
         }

      } else if (!start && data->state == TIMESYNC_RUNNING) {
         TimeSyncStopLoop(ctx, data);
      }

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
         data->slewPercentCorrection = TIMESYNC_PERCENT_CORRECTION;
      } else {
         data->slewPercentCorrection = percent;
      }

   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERIOD) == 0) {
      uint32 period;

      if (!StrUtil_StrToUint(&period, value)) {
         return FALSE;
      }

      if (period <= 0)
         period = TIMESYNC_TIME;

      /*
       * If the sync loop is running and the time sync period has changed,
       * restart the loop with the new period value. If the sync loop is
       * not running, just remember the new sync period value.
       */
      if (period != data->timeSyncPeriod) {
         data->timeSyncPeriod = period;

         if (data->state == TIMESYNC_RUNNING) {
            TimeSyncStopLoop(ctx, data);
            if (!TimeSyncStartLoop(ctx, data)) {
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

   if (data->state == TIMESYNC_RUNNING) {
      TimeSyncStopLoop(ctx, data);
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
      { TIMESYNC_SYNCHRONIZE, TimeSyncTcloHandler, data, NULL, NULL, 0 }
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
   data->slewPercentCorrection = TIMESYNC_PERCENT_CORRECTION;
   data->state = TIMESYNC_INITIALIZING;
   data->timeSyncPeriod = TIMESYNC_TIME;
   data->timer = NULL;

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   regData._private = data;

   return &regData;
}


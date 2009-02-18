/*********************************************************
 * Copyright (C) 2001 VMware, Inc. All rights reserved.
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
 * toolsDaemon.c --
 *
 *    Platform independent methods used by the tools daemon.
 *    The tools daemon does the following:
 *      -starts automatically with the guest
 *      -syncs the guest time to the host
 *      -executes scripts on state change requests from the VMX
 *      -listens for other TCLO cmds through the backdoor
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef _WIN32
#   include <windows.h>
#   include "win32u.h"
#   include "hgfsUsabilityLib.h"
#   include "rescaps.h"
#   include "ServiceHelpers.h"
#endif


#include "vmware.h"
#include "toolsDaemon.h"
#include "vm_version.h"
#include "vm_app.h"
#include "message.h"
#include "eventManager.h"
#include "debug.h"
#include "guestApp.h"
#include "rpcout.h"
#include "rpcin.h"
#include "hostinfo.h"
#include "strutil.h"
#include "str.h"
#include "msg.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "system.h"
#include "netutil.h"
#include "hgfsServerManager.h"
#include "conf.h"
#include "foundryToolsDaemon.h"
#include "util.h"
#include "vmcheck.h"

#ifndef N_PLAT_NLM
#include "vm_atomic.h"
#include "hostinfo.h"
#include "guestInfoServer.h"
#include "syncDriver.h"
#endif // #ifndef N_PLAT_NLM

#if !defined(__FreeBSD__) && !defined(sun) && !defined(N_PLAT_NLM)
#include "deployPkg.h"
#endif

#ifdef TOOLSDAEMON_HAS_RESOLUTION
#   include "resolution.h"
#endif

/* in 1/100 of a second */
#define RPCIN_POLL_TIME      10
/* sync the time once a minute */
#define TIME_SYNC_TIME     6000
/* only PERCENT_CORRECTION percent is corrected everytime */
#define PERCENT_CORRECTION   50

/*
 * Table mapping state changes to their conf file names.
 */
/*
 * Bug 294328:  Mac OS guests do not (yet) support the state change RPCs.
 */
#ifndef __APPLE__
static const char *stateChgConfNames[] = {
   NULL,                     /* NONE */
   CONFNAME_POWEROFFSCRIPT,  /* HALT */
   CONFNAME_POWEROFFSCRIPT,  /* REBOOT */
   CONFNAME_POWERONSCRIPT,   /* POWERON */
   CONFNAME_RESUMESCRIPT,    /* RESUME */
   CONFNAME_SUSPENDSCRIPT,   /* SUSPEND */
};
#endif

DblLnkLst_Links *ToolsDaemonEventQueue = NULL;  // main loop event queue
static char *guestTempDirectory = NULL;

void ToolsDaemon_InitializeForeignVM(ToolsDaemon_Data *toolsDaemonData);
void ToolsDaemon_ShutdownForeignVM(void);


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_SyncTime --
 *
 *    Set the guest OS time to the host OS time
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemon_SyncTime(Bool slewCorrection,  // IN: Is clock slewing enabled?
                     Bool syncOnce,        // IN: Is this function called in a loop?
                     void *toolsData)      // IN: Opaque data
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
   ToolsDaemon_Data *data = (ToolsDaemon_Data *) toolsData;
   Bool timeLagCall = FALSE;
#ifdef VMX86_DEBUG
   static int64 lastHostSecs = 0;
   int64 secs1, usecs1;
   int64 secs2, usecs2;

   System_GetCurrentTime(&secs1, &usecs1);
#endif

   Debug("Daemon: Synchronizing time\n");

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
      Debug("Using BDOOR_CMD_GETTIMEFULL_WITH_LAG\n");
   } else {
      Debug("BDOOR_CMD_GETTIMEFULL_WITH_LAG not supported by current host, attempting "
            "BDOOR_CMD_GETTIMEFULL\n");
      interruptLag = 0;
      bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL;
      Backdoor(&bp);
      if (bp.out.ax.word == BDOOR_MAGIC) {
         hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
      } else {
         Debug("BDOOR_CMD_GETTIMEFULL not supported by current host, attempting "
               "BDOOR_CMD_GETTIME\n");
         bp.in.cx.halfs.low = BDOOR_CMD_GETTIME;
         Backdoor(&bp);
         hostSecs = bp.out.ax.word;
      }
   }
   hostUsecs = bp.out.bx.word;
   maxTimeLag = bp.out.cx.word;

   if (hostSecs <= 0) {
      Warning("Invalid host OS time: %"FMT64"d secs, %"FMT64"d usecs.\n\n",
              hostSecs, hostUsecs);

      return FALSE;
   }

   /* Get the guest OS time */
   if (!System_GetCurrentTime(&guestSecs, &guestUsecs)) {
      Warning("Unable to retrieve the guest OS time: %s.\n\n", Msg_ErrString());
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
   Debug("Daemon: Guest clock lost %.6f secs; limit=%.2f; "
         "%"FMT64"d secs since last update\n",
         diff / 1000000.0, maxTimeLag / 1000000.0, hostSecs - lastHostSecs);
   Debug("Daemon: %d, %d, %"FMT64"d, %"FMT64"d, %"FMT64"d.\n",
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
      if (diff > maxTimeLag + interruptLag) {
         System_DisableTimeSlew();
         if (!System_AddToCurrentTime(diffSecs, diffUsecs)) {
            Warning("Unable to set the guest OS time: %s.\n\n", Msg_ErrString());
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
            Warning("Unable to set the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else if (slewCorrection && timeLagCall) {
         int64 slewDiff;

         /* Don't consider interruptLag during clock slewing. */
         slewDiff = diff - interruptLag;

         /* Correct only data->slewPercentCorrection percent error. */
         slewDiff = (data->slewPercentCorrection * slewDiff) / 100;

         if (!System_EnableTimeSlew(slewDiff, data->timeSyncPeriod)) {
            Warning("Unable to slew the guest OS time: %s.\n\n", Msg_ErrString());
            return FALSE;
         }
      } else {
         System_DisableTimeSlew();
      }
   }

#ifdef VMX86_DEBUG
      System_GetCurrentTime(&secs2, &usecs2);

      Debug("Time changed from %"FMT64"d.%"FMT64"d -> %"FMT64"d.%"FMT64"d\n",
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


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonConfFileLoop --
 *
 *    Run the "conf file reload" loop
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonConfFileLoop(void *clientData) // IN
{
   GuestApp_Dict **pConfDict = (GuestApp_Dict **) clientData;

   ASSERT(pConfDict);

   /*
    * With the addition of the Sync Driver we can get into a state
    * where the system drive is frozen, preventing the completion of
    * any disk-based I/O. The event that periodically reloads the conf
    * file then gets blocked, which blocks the main daemon thread and
    * prevents any further GuestRPC messages from getting
    * processed. This effectively deadlocks the tools daemon and among
    * other things makes it impossible to thaw disk I/O once it's been
    * frozen.
    *
    * So, we keep track of when the disks are frozen and skip doing disk
    * I/O during that time.
    */
#if !defined(N_PLAT_NLM)
   if (!SyncDriver_DrivesAreFrozen()) {
      if (Conf_ReloadFile(pConfDict)) {
         GuestInfoServer_DisableDiskInfoQuery(
            GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_DISABLEQUERYDISKINFO));

         Debug_Set(GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_LOG),
                   DEBUG_PREFIX);
         Debug_EnableToFile(GuestApp_GetDictEntry(*pConfDict, CONFNAME_LOGFILE),
                            FALSE);
      }
   }
#else
   if (Conf_ReloadFile(pConfDict)) {
      Debug_Set(GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_LOG),
                DEBUG_PREFIX);
      Debug_EnableToFile(GuestApp_GetDictEntry(*pConfDict, CONFNAME_LOGFILE),
                         FALSE);
   }
#endif

   EventManager_Add(ToolsDaemonEventQueue, CONF_POLL_TIME, ToolsDaemonConfFileLoop,
                    pConfDict);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTimeSyncLoop --
 *
 *    Run the "time synchronization" loop
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTimeSyncLoop(void *clientData) // IN
{
   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);

   /* The event has fired: it is no longer valid */
   data->timeSyncEvent = NULL;

   if (!data->timeSyncPeriod) {
      data->timeSyncPeriod = TIME_SYNC_TIME;
   }
   if (!ToolsDaemon_SyncTime(data->slewCorrection, FALSE, clientData)) {
      Warning("Unable to synchronize time.\n\n");
      return FALSE;
   }

   data->timeSyncEvent = EventManager_Add(ToolsDaemonEventQueue, data->timeSyncPeriod,
                                          ToolsDaemonTimeSyncLoop, data);
   if (data->timeSyncEvent == NULL) {
      Warning("Unable to run the \"time synchronization\" loop.\n\n");

      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonDisableWinTimeDaemon --
 *
 *      Try to disable the Windows Time Daemon.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(_WIN32)
static Bool
ToolsDaemonDisableWinTimeDaemon(void)
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
      Debug("GetSystemTimeAdjustment() succeeded: timeAdjustment %d,"
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
            Debug("Daemon: SetSystemTimeAdjustment failed: %d\n", error);
            goto exit;
         }
      }
   } else {
      error = GetLastError();
      Debug("Daemon: GetSystemTimeAdjustment failed: %d\n", error);
      goto exit;
   }

   success = TRUE;

  exit:
   Debug("Stopping time daemon %s.\n", success ? "succeeded" : "failed");
   System_SetProcessPrivilege(SE_SYSTEMTIME_NAME, FALSE);
   return success;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonStartStopTimeSyncLoop --
 *
 *    Start or stop the "time synchronization" loop. Nothing will be
 *    done if start==TRUE & it's already running or start=FALSE & it's
 *    not running.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonStartStopTimeSyncLoop(ToolsDaemon_Data *data, // IN
                                 Bool start)             // IN
{
   ASSERT(data);

   if (start && data->timeSyncEvent == NULL) {
      Debug("Daemon: Starting time sync loop\n");
      Debug("Daemon: New sync period is %d sec\n", data->timeSyncPeriod);
      if (!ToolsDaemonTimeSyncLoop(data)) {
         return FALSE;
      }

#if defined(_WIN32)
      Debug("Daemon: Attempting to disable Windows Time daemon\n");
      if (!ToolsDaemonDisableWinTimeDaemon()) {
         Debug("Daemon: Failed to disable Windows Time daemon\n");
      }
#endif

      return TRUE;
   } else if (!start && data->timeSyncEvent != NULL) {
      Debug("Daemon: Stopping time sync loop\n");
      System_DisableTimeSlew();
      EventManager_Remove(data->timeSyncEvent);
      data->timeSyncEvent = NULL;

      return TRUE;
   } else {
      /*
       * No need to start time sync b/c it's already running or no
       * need to stop it b/c it's not running.
       */
      return TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonOldUpdateOptions --
 *
 *    Get the latest value of the tools options from VMware, and update
 *    guestd's behavior according to this new value
 *    (Legacy from before the unified TCLO loop)
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonOldUpdateOptions(ToolsDaemon_Data *data) // IN
{
   uint32 toolsOptions;
   Bool syncTime;
   Bool copyPaste;
   Bool autoHide;

   ASSERT(data);

   toolsOptions = GuestApp_OldGetOptions();

   syncTime = (toolsOptions & VMWARE_GUI_SYNC_TIME) != 0;
   GuestApp_SetDictEntry(data->optionsDict, TOOLSOPTION_SYNCTIME,
                           syncTime ? "1" : "0");
   copyPaste = (toolsOptions & VMWARE_GUI_EXCHANGE_SELECTIONS) != 0;
   GuestApp_SetDictEntry(data->optionsDict, TOOLSOPTION_COPYPASTE,
                           copyPaste ? "1" : "0");
   autoHide = (toolsOptions & VMWARE_GUI_WARP_CURSOR_ON_UNGRAB) != 0;
   GuestApp_SetDictEntry(data->optionsDict, TOOLSOPTION_AUTOHIDE,
                           autoHide ? "1" : "0");

   if (ToolsDaemonStartStopTimeSyncLoop(data, syncTime) == FALSE) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonOldUpdateOptionsLoop --
 *
 *    Run the "update options" loop
 *    (Legacy from before the unified TCLO loop)
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonOldUpdateOptionsLoop(void *clientData) // IN
{
   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);

   if (ToolsDaemonOldUpdateOptions(data) == FALSE) {
      return FALSE;
   }

   data->oldOptionsLoop = EventManager_Add(ToolsDaemonEventQueue, 100,
                                           ToolsDaemonOldUpdateOptionsLoop, data);
   if (data->oldOptionsLoop == NULL) {
      Warning("Unable to run the \"update options\" loop.\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonStartStopOldUpdateOptionsLoop --
 *
 *      Start or stop the old upate options loop depending
 *      on whether vmware is unified loop capable.
 *      It won't be started again it's already running & it won't be
 *      stopped if it's not running.
 *
 * Results:
 *      TRUE on success
 *      FALSE if all attempts to get options have failed.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonStartStopOldUpdateOptionsLoop(ToolsDaemon_Data *data) // IN
{
   Bool unifiedLoopCap;

   ASSERT(data);

   /*
    * Start the old options loop if it's not running & the unified loop is no
    * supported; stop it if it is running & the unified loop is supported.
    */
   unifiedLoopCap = GuestApp_GetUnifiedLoopCap(TOOLS_DAEMON_NAME);
   if (!unifiedLoopCap && data->oldOptionsLoop == NULL) {
      Debug("Daemon: No unified loop cap; starting old poll loop.\n");

      if (!ToolsDaemonOldUpdateOptionsLoop(data)) {
         return FALSE;
      }
   } else if (unifiedLoopCap && data->oldOptionsLoop != NULL) {
      Debug("Daemon: Unified loop cap found; stopping old poll loop.\n");

      EventManager_Remove(data->oldOptionsLoop);
      data->oldOptionsLoop = NULL;
   } else {
      /*
       * No need to start the loop b/c it's already running or no
       * need to stop it b/c it's not running.
       */
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonResetSent --
 *
 *      Called after we've sent the reset TCLO completion to vmware.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *	Set our version in vmware & start/stop the old options loop.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonResetSent(void *clientData) // IN
{
   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);

#if !defined(N_PLAT_NLM)
   GuestInfoServer_VMResumedNotify();
#endif

   GuestApp_Log("Version: " BUILD_NUMBER "\n");

   if (!ToolsDaemonStartStopOldUpdateOptionsLoop(data)) {
      /* We aren't much use if we can't get the options */
      Panic("Unable to get options from %s\n", PRODUCT_LINE_NAME);
   }

   if (data->resetCB) {
      data->resetCB(data->resetCBData);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloReset --
 *
 *      'reset' tclo cmd handler. MUST be the first tclo message sent
 *      by VMware when it recognizes that a toolbox app has opened
 *      a tclo channel.
 *
 * Results:
 *      TRUE on success (*result is empty)
 *      FALSE on failure (*result contains the error)
 *
 * Side effects:
 *	May start or stop the old update options loop.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloReset(RpcInData *data)  // IN/OUT
{
   /*
    * Mandatory reset RPC
    */

   Debug("----------Daemon: Received 'reset' from vmware\n");

   /*
    * Schedule the post-reset actions to happen a little after one cycle of the
    * RpcIn loop. This will give vmware a chance to receive the ATR &
    * reinitialize the channel if appropriate. [greg]
    */
   EventManager_Add(ToolsDaemonEventQueue, (int) (RPCIN_POLL_TIME * 1.5),
                    ToolsDaemonResetSent, data->clientData);

   return RPCIN_SETRETVALS(data, "ATR " TOOLS_DAEMON_NAME, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonStateChangeDone --
 *
 *      Called when a state change script is done running.
 *      Sends the state change status with the script exit value.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	May halt/reboot the VM. Also VMware may suspend the VM upon
 *      receipt of a positive status.
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsDaemonStateChangeDone(Bool status,  // IN
                           void *cbData) // IN
{
   ToolsDaemon_Data *data = (ToolsDaemon_Data *) cbData;

   ASSERT(data);
   ASSERT(data->rebootCB);
   ASSERT(data->haltCB);

   Debug("Daemon: state change callback called\n");

   /*
    * We execute the requested action if the script succeeded, or if the
    * same action was tried before but didn't finish due to a script failure.
    * See bug 168568 for discussion.
    */
   if (status || data->lastFailedStateChg == data->stateChgInProgress) {
      status = TRUE;
#ifdef _WIN32
      if (data->stateChgInProgress == GUESTOS_STATECHANGE_REBOOT || data->stateChgInProgress == GUESTOS_STATECHANGE_HALT) {
         if (Hostinfo_GetOSType() >= OS_VISTA) {
            DISABLE_RES_CAPS();
         }
      }
#endif
      if (data->stateChgInProgress == GUESTOS_STATECHANGE_REBOOT) {
         Debug("Initiating reboot\n");
         status = data->rebootCB(data->rebootCBData);
      } else if (data->stateChgInProgress == GUESTOS_STATECHANGE_HALT) {
         Debug("Initiating halt\n");
         status = data->haltCB(data->haltCBData);
      }
      data->lastFailedStateChg = GUESTOS_STATECHANGE_NONE;
   }

   if (!status) {
      data->lastFailedStateChg = data->stateChgInProgress;
   }

   if (!ToolsDaemon_SetOsPhase(status, data->stateChgInProgress)) {
      Warning("Unable to send the status RPCI");
   }

   data->stateChgInProgress = GUESTOS_STATECHANGE_NONE;

   /* Unless the process couldn't be spawned, we need to free it */
   if (data->asyncProc) {
      free(data->asyncProc);
      data->asyncProc = NULL;
   }
}


/*
 * Bug 294328:  Mac OS guests do not (yet) support the state change RPCs.
 */
#ifndef __APPLE__
/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloStateChange --
 *
 *      Tclo cmd handler for commands which invoke state change scripts.
 *
 * Results:
 *      TRUE on success (*result is empty)
 *      FALSE on failure (*result contains the error)
 *
 * Side effects:
 *	Scripts are invoked in the guest.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloStateChange(char const **result,     // OUT
                           size_t *resultLen,       // OUT
                           const char *name,        // IN
                           const char *args,        // IN
                           size_t argsSize,         // Ignored
                           void *clientData)        // IN
{
   int i;
   ProcMgr_ProcArgs procArgs;
   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);
   ASSERT(data->pConfDict);
   ASSERT(*data->pConfDict);

   Debug("Got state change message\n");

   if (data->asyncProc != NULL) {
      Debug("State change already in progress\n");
      return RpcIn_SetRetVals(result, resultLen,
                              "State change already in progress", FALSE);
   }

   for (i = 0; i < ARRAYSIZE(stateChangeCmdTable); i++) {
      if (strcmp(name, stateChangeCmdTable[i].tcloCmd) == 0) {
         const char *script;
         char *scriptCmd;
         unsigned int stateId;

         stateId = stateChangeCmdTable[i].id;
         data->stateChgInProgress = (GuestOsState)stateId;

         /* Check for the toolScripts option. */
         if (!data->toolScriptOption[stateId]) {
            ToolsDaemonStateChangeDone(TRUE, data);
            Debug("Script for %s not configured to run\n", stateChangeCmdTable[i].tcloCmd);
            return RpcIn_SetRetVals(result, resultLen, "", TRUE);
         }

         script = GuestApp_GetDictEntry(*data->pConfDict,
                                        stateChgConfNames[stateId]);
         ASSERT(script);
         if (strlen(script) == 0) {
            ToolsDaemonStateChangeDone(TRUE, data);
            Debug("No script to run\n");
            return RpcIn_SetRetVals(result, resultLen, "", TRUE);
         }
#ifdef N_PLAT_NLM
         procArgs = NULL;
         scriptCmd = Str_Asprintf(NULL, "%s", script);
#elif !defined(_WIN32)
         procArgs = NULL;
         ASSERT(data->execLogPath);
         scriptCmd = Str_Asprintf(NULL, "(%s) 2>&1 >> %s",
                                  script, data->execLogPath);
#else
         /*
          * Pass the CREATE_NO_WINDOW flag to CreateProcess so that the
          * cmd.exe window will not be visible to the user in the guest.
          */
         memset(&procArgs, 0, sizeof procArgs);
         procArgs.bInheritHandles = TRUE;
         procArgs.dwCreationFlags = CREATE_NO_WINDOW;

         {
            char systemDir[1024 * 3];
            Win32U_GetSystemDirectory(systemDir, sizeof systemDir);
            scriptCmd = Str_Asprintf(NULL, "%s\\cmd.exe /c \"%s\"", systemDir, script);
         }
#endif

         if (scriptCmd == NULL) {
            Debug("Could not format the cmd to run scripts\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "Could not format cmd to run scritps",
                                    FALSE);
         }
         data->asyncProc = ProcMgr_ExecAsync(scriptCmd, &procArgs);

         if (data->asyncProc) {
            data->asyncProcCb = ToolsDaemonStateChangeDone;
            data->asyncProcCbData = data;
         } else {
            ToolsDaemonStateChangeDone(FALSE, data);
            goto startError;
         }

         free(scriptCmd);
         return RpcIn_SetRetVals(result, resultLen, "", TRUE);

      startError:
         free(scriptCmd);
         Debug("Error starting script\n");   
         return RpcIn_SetRetVals(result, resultLen, "Error starting script",
                                 FALSE);
      }
   }

   Debug("Invalid state change command\n");   
   return RpcIn_SetRetVals(result, resultLen, "Invalid state change command",
                           FALSE);
}
#endif // ifndef __APPLE__


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloCapReg --
 *
 *    Register our capabilities with the VMX & request
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloCapReg(char const **result,     // OUT
                      size_t *resultLen,       // OUT
                      const char *name,        // IN
                      const char *args,        // IN
                      size_t argsSize,         // Ignored
                      void *clientData)        // IN
{
#ifdef _WIN32
   unsigned int minResolutionWidth;
   unsigned int minResolutionHeight;
#endif
   ToolsDaemon_Data *data;

   data = (ToolsDaemon_Data *)clientData;
   ASSERT(data);

#ifdef _WIN32
   /*
    * Inform the VMX that we support setting the guest
    * resolution and display topology. Currently, this only
    * applies on windows.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 1")) {
      Debug("ToolsDaemonTcloCapReg: Unable to register resolution set capability\n");
   }
   /* Tell the VMX to send resolution updates to the tools daemon */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 1",
		       TOOLS_DAEMON_NAME)) {
      Debug("ToolsDaemonTcloCapReg: Unable to register resolution server capability\n");
   }
   /*
    * Bug 149541: Windows 2000 does not currently support multimon.
    *
    * In addition, NT will never support multimon.  9x guests have
    * frozen tools, and will report this capability set to 1, which
    * current UIs will treat as unsupported.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set %s",
                       Hostinfo_GetOSType() >= OS_WINXP ? "2" : "0")) {
      Debug("ToolsDaemonTcloCapReg: Unable to register display topology set "
            "capability\n");
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 1")) {
      Debug("ToolsDaemonTcloCapReg: Unable to register display global offset "
            "capability\n");
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.color_depth_set 1")) {
      Debug("ToolsDaemonTcloCapReg: Unable to register color depth set "
            "capability\n");
   }

   /*
    * Report to the VMX any minimum guest resolution below which we
    * can't resize the guest. See bug 58681.
    */
   ToolsDaemon_GetMinResolution(*data->pConfDict, &minResolutionWidth,
                                &minResolutionHeight);

   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_min %u %u",
                       minResolutionWidth, minResolutionHeight)) {
      Debug("ToolsDaemonTcloCapReg: Unable to register minimum resolution of %ux%u\n",
            minResolutionWidth, minResolutionHeight);
   }
#endif

#ifdef TOOLSDAEMON_HAS_RESOLUTION
   Resolution_RegisterCaps();
#endif

   /*
    * Bug 294328:  Mac OS guests do not (yet) support the state change RPCs.
    */
#ifndef __APPLE__
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.statechange")) {
      Debug("ToolsDaemonTcloCapReg: VMware doesn't support tools.capability.statechange. "
            "Trying .haltreboot\n");
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.haltreboot")) {
         return RpcIn_SetRetVals(result, resultLen,
                                 "Unable to register capabilities", FALSE);
      }
   }

   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.softpowerop_retry")) {
      Debug("ToolsDaemonTcloCapReg: VMX doesn't support "
            "tools.capability.softpowerop_retry.");
   }
#endif  // ifndef __APPLE__

/*
 * This is a _WIN32 || linux check, with the additional check since linux is
 * defined when you build the NetWare Tools.
 */
#if (defined(_WIN32) || defined(linux)) && !defined(N_PLAT_NLM)
   {
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.auto_upgrade 2")) {
         Debug("ToolsDaemonTcloCapReg: Unable to register "
               "auto-upgrading capability.\n");
      }

      if (guestTempDirectory == NULL) {
#ifdef _WIN32
         guestTempDirectory = File_GetTmpDir(FALSE);
#else
         guestTempDirectory = Util_GetSafeTmpDir(FALSE);
#endif
      }

      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.guest_temp_directory 1 %s",
                          guestTempDirectory)) {
         Debug("ToolsDaemonTcloCapReg: Unable to register guest temp "
               "directory capability.\n");
      }
   }
#endif

#if !defined(N_PLAT_NLM)
   {
      char *confPath = GuestApp_GetConfPath();
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.guest_conf_directory %s",
                          confPath ? confPath : "")) {
         Debug("ToolsDaemonTcloCapReg: Unable to register guest conf "
               "directory capability.\n");
      }
      free(confPath);
   }

   /* 
    * Send the uptime here so that the VMX can detect soft resets. This must be
    * sent before the Tools version RPC since the version RPC handler uses the
    * uptime to detect soft resets.
    */
   if (!GuestInfoServer_SendUptime()) {
      Debug("Daemon: Error setting guest uptime during 'reset' request.\n");
   }
#endif

   /*
    * Send the monolithic Tools version. Using a configuration option, users
    * can override the Tools version such that the VMX treats the Tools as not
    * to be managed by the VMware platform.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.set.version %u",
                       GuestApp_GetDictEntryBool(*data->pConfDict,
                                                 CONFNAME_DISABLETOOLSVERSION) ?
                       TOOLS_VERSION_UNMANAGED : TOOLS_VERSION_CURRENT)) {
      Debug("Daemon: Error setting tools version during 'Capabilities_Register'"
            "request.\n");
   }

#if !defined(N_PLAT_NLM) && !defined(sun)
   if (!HgfsServerManager_CapReg(TOOLS_DAEMON_NAME, TRUE)) {
      Debug("ToolsDaemonTcloCapReg: Failed to register HGFS server capability.\n");
   }
#endif

#if defined(WIN32)
   HgfsUsability_RegisterServiceCaps();
   if (Hostinfo_GetOSType() >= OS_VISTA) {
      ServiceHelpers_SendResolutionCaps();
   }
#endif

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloTimeSync --
 *
 *    Sync the guest's time with the host's.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloTimeSync(char const **result,     // OUT
                        size_t *resultLen,       // OUT
                        const char *name,        // IN
                        const char *args,        // IN
                        size_t argsSize,         // Ignored
                        void *clientData)        // Ignored
{
   Bool slewCorrection = !strcmp(args, "1");

   if (!ToolsDaemon_SyncTime(slewCorrection, TRUE, clientData)) {
      return RpcIn_SetRetVals(result, resultLen,
                              "Unable to sync time", FALSE);
   } else {
      return RpcIn_SetRetVals(result, resultLen, "", TRUE);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloSetOption --
 *
 *    Parse a "Set_Option" TCLO cmd from the VMX & update the local
 *    value of the option.
 *
 * Return value:
 *    TRUE if the set option command was executed
 *    FALSE if something failed (detail displayed)
 *
 * Side effects:
 *    Start or stop processes (like time syncing) that could be affected
 *    by option's new value.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ToolsDaemonTcloSetOption(char const **result,     // OUT
                         size_t *resultLen,       // OUT
                         const char *name,        // IN
                         const char *args,        // IN
                         size_t argsSize,         // Ignored
                         void *clientData)        // IN
{
   Bool retVal = FALSE;
   char *option;
   char *value;
   unsigned int index = 0;
   static Bool timeSyncStartup = TRUE;
   static int oldTimeSyncValue = -1;

   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);

   /* parse the option & value string */
   option = StrUtil_GetNextToken(&index, args, " ");
   index++; // ignore leading space before value
   value = StrUtil_GetNextToken(&index, args, "");
   if (option == NULL || value == NULL || strlen(value) == 0) {
      goto invalid_option;
   }

   /* Validate the option name & value */
   if (strcmp(option, TOOLSOPTION_SYNCTIME) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_SLEWCORRECTION) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERCENTCORRECTION) == 0) {
      int32 percent;
      if (!StrUtil_StrToInt(&percent, value) || percent == 0 || percent > 100) {
         goto invalid_value;
      }
      Debug("Daemon: update the slew correction percent.\n");
   } else if (strcmp(option, TOOLSOPTION_COPYPASTE) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_AUTOHIDE) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_BROADCASTIP) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERIOD) == 0) {
      Debug("Daemon: update the time sync period.\n");
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_ENABLE) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_STARTUP) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_LINK_ROOT_HGFS_SHARE) == 0) {
      /*
       * Check to make sure that we actually support creating the link
       * on this platform.
       */
      if (!data->linkHgfsCB || !data->unlinkHgfsCB) {
	 goto invalid_option;
      }

      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_POWERON) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_POWEROFF) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_SUSPEND) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_RESUME) == 0) {
      if (strcmp(value, "1") != 0 && strcmp(value, "0") != 0) {
         goto invalid_value;
      }
   } else {
      goto invalid_option;
   }

   Debug("Daemon: Setting option '%s' to '%s'\n", option, value);
   GuestApp_SetDictEntry(data->optionsDict, option, value);

   /* Take action that may be necessary given the new value */
   if (strcmp(option, TOOLSOPTION_SYNCTIME) == 0) {
      int start = (strcmp(value, "1") == 0);

      /* 
       * Try the one-shot time sync if time sync transitions from
       * 'off' to 'on'.
       */
      if (oldTimeSyncValue == 0 && start &&
          GuestApp_GetDictEntry(data->optionsDict, TOOLSOPTION_SYNCTIME_ENABLE)) {
         ToolsDaemon_SyncTime(data->slewCorrection, TRUE, clientData);
      }
      oldTimeSyncValue = start;
      
      /* Now start/stop the loop. */
      if (!ToolsDaemonStartStopTimeSyncLoop(data, start)) {
         RpcIn_SetRetVals(result, resultLen,
                          "Unable to start/stop time sync loop",
                          retVal = FALSE);
         goto exit;
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_SLEWCORRECTION) == 0) {
      data->slewCorrection = strcmp(value, "0");
      Debug("Daemon: Setting slewCorrection, %d.\n", data->slewCorrection);
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERCENTCORRECTION) == 0) {
      int32 percent;
      if (StrUtil_StrToInt(&percent, value)) {
         data->slewPercentCorrection = percent;
      }
   } else if (strcmp(option, TOOLSOPTION_BROADCASTIP) == 0 &&
              strcmp(value, "1") == 0) {
      char *ip;

      ip = NetUtil_GetPrimaryIP();

      if (ip == NULL) {
         RpcIn_SetRetVals(result, resultLen, "Error getting IP address of guest",
                          retVal = FALSE);
         goto exit;
      }

      RpcOut_sendOne(NULL, NULL, "info-set guestinfo.ip %s", ip);
      free(ip);
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_PERIOD) == 0) {
      uint32 period = atoi(value);

      /*
       * If the sync loop is running and
       * the time sync period has changed,
       * restart the loop with the new period value.
       * If the sync loop is not running,
       * just remember the new sync period value.
       */
      if (period != data->timeSyncPeriod) {
         data->timeSyncPeriod = period * 100;

         if (data->timeSyncEvent != NULL) {
            Bool status;

            /* Stop the loop. */
            status = ToolsDaemonStartStopTimeSyncLoop(data, FALSE);

            /* Start the loop with the new period value. */
            status = ToolsDaemonStartStopTimeSyncLoop(data, TRUE);

            if (!status) {
               RpcIn_SetRetVals(result, resultLen,
                                "Unable to change time sync period value",
                                retVal = FALSE);
               goto exit;
            }
         }
      }
   } else if (strcmp(option, TOOLSOPTION_SYNCTIME_STARTUP) == 0) {
      uint32 syncStartupOk = atoi(value);

      if (timeSyncStartup) {
         timeSyncStartup = FALSE;

         if (syncStartupOk) {
            if (!ToolsDaemon_SyncTime(TRUE, TRUE, clientData)) {
               RpcIn_SetRetVals(result, resultLen,
                                "Unable to sync time during startup",
                                retVal = FALSE);
               goto exit;
            }
         }
      }
   } else if (strcmp(option, TOOLSOPTION_LINK_ROOT_HGFS_SHARE) == 0) {
      if (strcmp(value, "1") == 0) {
	 /* Validated that data->linkHgfsCB existed above. */
	 retVal = data->linkHgfsCB(data->linkHgfsCBData);
      } else if (strcmp(value, "0") == 0) {
	 /* Validated that data->unlinkHgfsCB existed above. */
	 retVal = data->unlinkHgfsCB(data->unlinkHgfsCBData);
      }

      if (!retVal) {
	 RpcIn_SetRetVals(result, resultLen,
			  "Could not link/unlink root share.",
			  retVal = FALSE);
	 goto exit;
      }
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_POWERON) == 0) {
      data->toolScriptOption[GUESTOS_STATECHANGE_POWERON] =
                                strcmp(value, "0") ? TRUE : FALSE;
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_POWEROFF) == 0) {
      data->toolScriptOption[GUESTOS_STATECHANGE_HALT] =
      data->toolScriptOption[GUESTOS_STATECHANGE_REBOOT] =
                                strcmp(value, "0") ? TRUE : FALSE;
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_SUSPEND) == 0) {
      data->toolScriptOption[GUESTOS_STATECHANGE_SUSPEND] =
                                strcmp(value, "0") ? TRUE : FALSE;
   } else if (strcmp(option, TOOLSOPTION_SCRIPTS_RESUME) == 0) {
      data->toolScriptOption[GUESTOS_STATECHANGE_RESUME] =
                                strcmp(value, "0") ? TRUE : FALSE;
   }

   /* success! */
   RpcIn_SetRetVals(result, resultLen, "", retVal = TRUE);
   goto exit;

 invalid_option:
   RpcIn_SetRetVals(result, resultLen, "Unknown option", retVal = FALSE);
   goto exit;

 invalid_value:
   RpcIn_SetRetVals(result, resultLen, "Invalid option value",
                    retVal = FALSE);
   goto exit;

 exit:
   free(option);
   free(value);
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemonTcloError --
 *
 *    Callback called when an error occurred in the receive loop
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsDaemonTcloError(void *clientData,   // IN
                     char const *status) // IN
{
   ToolsDaemon_Data *data = (ToolsDaemon_Data *)clientData;

   ASSERT(data);

   Warning("Error in the RPC receive loop: %s.\n\n", status);
   data->inError = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_Init_Backdoor --
 *
 *    Initializes the backdoor to the VMX.
 *
 * Return value:
 *    TRUE if successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemon_Init_Backdoor(ToolsDaemon_Data * data) // IN/OUT
{
   data->in = RpcIn_Construct(ToolsDaemonEventQueue);
   if (data->in == NULL) {
      Warning("Unable to create the RpcIn object.\n\n");
      return FALSE;
   }

   /*
    * Initialize 'inError' before starting the loop -- clients should 
    * only read this flag.
    */
   data->inError = FALSE;


   /* Start the TCLO receive loop */
   if (RpcIn_start(data->in, RPCIN_POLL_TIME,
                   ToolsDaemonTcloReset, data,
                   ToolsDaemonTcloError, data) == FALSE) {
      RpcIn_Destruct(data->in);
      data->in = NULL;
      Warning("Unable to start the receive loop.\n\n");
      return FALSE;
   }

   RpcIn_RegisterCallback(data->in, "Time_Synchronize",
                          ToolsDaemonTcloTimeSync, NULL);
   RpcIn_RegisterCallback(data->in, "Capabilities_Register",
                          ToolsDaemonTcloCapReg, data);
   RpcIn_RegisterCallback(data->in, "Set_Option",
                          ToolsDaemonTcloSetOption, data);

   /*
    * Bug 294328:  Mac OS guests do not (yet) support the state change RPCs.
    */
#ifndef __APPLE__
   {
      int i;
      for (i = 0; i < ARRAYSIZE(stateChangeCmdTable); i++) {
         RpcIn_RegisterCallback(data->in, stateChangeCmdTable[i].tcloCmd,
                                ToolsDaemonTcloStateChange, data);
      }
   }
#endif // ifndef __APPLE__

#if !defined(N_PLAT_NLM)
   FoundryToolsDaemon_RegisterRoutines(data->in,
                                       data->pConfDict, 
                                       ToolsDaemonEventQueue,
                                       TRUE);
   if (!HgfsServerManager_Register(data->in, TOOLS_DAEMON_NAME)) {
      RpcIn_stop(data->in);
      RpcIn_Destruct(data->in);
      data->in = NULL;
      Warning("Could not initialize HGFS server\n");
      return FALSE;
   }
#endif

#ifdef TOOLSDAEMON_HAS_RESOLUTION
   Resolution_InitBackdoor(data->in);
#endif

#if !defined(__FreeBSD__) && !defined(sun) && !defined(N_PLAT_NLM)
   DeployPkg_Register(data->in);
#endif

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_Init --
 *
 *    Setup a TCLO channel with VMware, and start it's event loop
 *
 * Return value:
 *    the created RpcIn struct or
 *    NULL if something failed (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

ToolsDaemon_Data *
ToolsDaemon_Init(GuestApp_Dict **pConfDict,         // IN
                 const char *execLogPath,           // IN
                 ToolsDaemon_Callback haltCB,       // IN
                 void *haltCBData,                  // IN
                 ToolsDaemon_Callback rebootCB,     // IN
                 void *rebootCBData,                // IN
                 ToolsDaemon_Callback resetCB,      // IN
                 void *resetCBData,                 // IN
                 ToolsDaemon_Callback linkHgfsCB,   // IN
		 void *linkHgfsCBData,              // IN
                 ToolsDaemon_Callback unlinkHgfsCB, // IN
		 void *unlinkHgfsCBData)            // IN
{
   ToolsDaemon_Data *data;
   int i;

#ifndef N_PLAT_NLM
   Atomic_Init();
#endif // #ifndef N_PLAT_NLM

   ASSERT(pConfDict);
   ASSERT(*pConfDict);
   ASSERT(haltCB != NULL);
   ASSERT(rebootCB != NULL);

   data = (ToolsDaemon_Data *) calloc(1, sizeof(ToolsDaemon_Data));
   ASSERT_MEM_ALLOC(data);

   data->pConfDict = pConfDict;
   data->execLogPath = execLogPath;
   data->inError = FALSE;
   data->haltCB = haltCB;
   data->haltCBData = haltCBData;
   data->rebootCB = rebootCB;
   data->rebootCBData = rebootCBData;
   data->stateChgInProgress = GUESTOS_STATECHANGE_NONE;
   data->lastFailedStateChg = GUESTOS_STATECHANGE_NONE;
   data->resetCB = resetCB;
   data->resetCBData = resetCBData;
   data->linkHgfsCB = linkHgfsCB;
   data->linkHgfsCBData = linkHgfsCBData;
   data->unlinkHgfsCB = unlinkHgfsCB;
   data->unlinkHgfsCBData = unlinkHgfsCBData;
   data->timeSyncPeriod = 0;
   data->slewPercentCorrection = PERCENT_CORRECTION;
   data->slewCorrection = TRUE;

   for (i = 0; i < GUESTOS_STATECHANGE_LAST; i++) {
      data->toolScriptOption[i] = TRUE;
   }

#if ALLOW_TOOLS_IN_FOREIGN_VM
   if (!VmCheck_IsVirtualWorld()) {
      ToolsDaemon_InitializeForeignVM(data);
   }
#endif

#if defined(VMX86_DEBUG) && !defined(__APPLE__)
   {
      /* Make sure the confDict has all the confs we need */
      for (i = 0; i < ARRAYSIZE(stateChangeCmdTable); i++) {
         const char *confName;

         confName = stateChgConfNames[stateChangeCmdTable[i].id];
         ASSERT(GuestApp_GetDictEntry(*pConfDict, confName));
      }
   }
#endif

   ToolsDaemonEventQueue = EventManager_Init();
   if(!ToolsDaemonEventQueue) {
      Warning("Unable to create the event queue.\n\n");
      goto error;
   }

#ifdef TOOLSDAEMON_HAS_RESOLUTION
   if (!Resolution_Init(TOOLS_DAEMON_NAME, NULL)) {
      Debug("%s: Unable to initialize Guest Fit feature\n", __func__);
   }
#endif

   /*
    * Load the conf file, then setup a periodic check and reload.
    */
   Debug_Set(GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_LOG), DEBUG_PREFIX);

   /*
    * All components except vmware-user will be logged to same file. Everytime after
    * reboot, tools daemon should rename existing log file and start logging to a new
    * one. In all other cases the backup flag for Debug_EnableToFile should be set to
    * FALSE.
    */
   Debug_EnableToFile(GuestApp_GetDictEntry(*pConfDict, CONFNAME_LOGFILE), TRUE);

   EventManager_Add(ToolsDaemonEventQueue, CONF_POLL_TIME, ToolsDaemonConfFileLoop,
                    data->pConfDict);

   if (!ToolsDaemon_Init_Backdoor(data)) {
      goto error;
   }

   data->optionsDict = GuestApp_ConstructDict(NULL);

   return data;

 error:
   if (ToolsDaemonEventQueue) {
      EventManager_Destroy(ToolsDaemonEventQueue);
      ToolsDaemonEventQueue = NULL;
   }
   free(data);
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_Cleanup_Backdoor --
 *
 *    Closes the backdoor to the VMX.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsDaemon_Cleanup_Backdoor(ToolsDaemon_Data *data) // IN/OUT
{
   ASSERT(data);
   if (data->in) {
#if !defined(N_PLAT_NLM)
      HgfsServerManager_Unregister(data->in, TOOLS_DAEMON_NAME);
#endif
      RpcIn_stop(data->in);
      RpcIn_Destruct(data->in);
      data->in = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_Cleanup --
 *
 *    Cleanup the RpcIn channel if it hasn't been destructed yet& free the
 *    local options.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsDaemon_Cleanup(ToolsDaemon_Data *data) // IN
{
#ifdef _WIN32
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 0")) {
      Debug("%s: Unable to unregister resolution set capability\n",
	    __FUNCTION__);
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 0",
		       TOOLS_DAEMON_NAME)) {
      Debug("%s: Unable to unregister resolution server capability\n",
	    __FUNCTION__);
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 0")) {
      Debug("%s: Unable to unregister display topology set capability\n",
	    __FUNCTION__);
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 0")) {
      Debug("%s: Unable to unregister display global offset capability\n",
	    __FUNCTION__);
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.color_depth_set 0")) {
      Debug("%s: Unable to unregister color depth set capability\n",
	    __FUNCTION__);
   }

   /*
    * Clear the minimum resolution limitation.
    */
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_min 0 0")) {
      Debug("%s: Unable to clear minimum resolution\n", __FUNCTION__);
   }

   HgfsUsability_UnregisterServiceCaps();
#endif

#ifdef TOOLSDAEMON_HAS_RESOLUTION
   Resolution_Cleanup();
#endif

#if defined(_WIN32) || defined(linux)
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.auto_upgrade 0")) {
      Debug("%s: Unable to clear auto-upgrading capability.\n", __FUNCTION__);
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.guest_temp_directory 0")) {
      Debug("%s: Unable to clear guest temp directory capability.\n",
	    __FUNCTION__);
   }
#endif

#if !defined(N_PLAT_NLM)
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.guest_conf_directory 0")) {
      Debug("%s: Unable to clear guest conf directory capability.\n",
	    __FUNCTION__);
   }
#endif

#if ALLOW_TOOLS_IN_FOREIGN_VM
   if (runningInForeignVM) {
      ToolsDaemon_ShutdownForeignVM();
   }
#endif

   ToolsDaemon_Cleanup_Backdoor(data);

   GuestApp_FreeDict(data->optionsDict);

   if (data->asyncProc) {
      ProcMgr_Kill(data->asyncProc);
      ToolsDaemonStateChangeDone(FALSE, data);
   }

   EventManager_Destroy(ToolsDaemonEventQueue);
   free(data);
   free(guestTempDirectory);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_CheckReset --
 *
 *    Can/should be called in an app's main run loop before calling the
 *    'sleep' function, to check and potentially reset the rpc layer.
 *
 * Return value:
 *    TRUE  if no errors were encountered, or rpc re-initialization is in
 *          progress and we haven't exceeded the maximum number of consecutive
 *          recovery attempts
 *    FALSE rpc can't be re-initialized, or we exhausted our attempts quota
 *
 * Side effects:
 *    Changes 'data'.
 *
 *-----------------------------------------------------------------------------
 */
                                                                                
Bool
ToolsDaemon_CheckReset(ToolsDaemon_Data *data,  // IN/OUT
                       uint64 *sleepUsecs)      // IN/OUT
{
   static int channelTimeoutAttempts = -1;
   char *tmp = NULL;
                                                                                
   ASSERT(data);
                                                                                
   if (channelTimeoutAttempts < 0) {
      Debug("Attempting to retrieve number of channel timeout attempts "
            "from vmx\n");
      /*
       * Currenly, we still use the 'guestinfo' alias. When the main branches
       * are synced up and the 'guestvars' code becomes stable, we'll move to
       * using the un-prefixed key.
       */
      if (RpcOut_sendOne(&tmp, NULL,
                         "info-get guestinfo.guest_rpc.tclo.timeout") && tmp) {
         Debug("Retrieved channel timeout attempts from vmx: %s\n", tmp);
         channelTimeoutAttempts = atoi(tmp);
      }
      free(tmp);
      /* Safe-guard attempts against negative and too high-values. */
      if (channelTimeoutAttempts <= 0) {
         channelTimeoutAttempts = 60;
         Debug("Assuming %d channel timeout attempts\n",
               channelTimeoutAttempts);
      } else if (channelTimeoutAttempts > 180) {
         channelTimeoutAttempts = 180;
         Debug("Limiting to %d channel timeout attempts\n",
               channelTimeoutAttempts);
      }
      /*
       * Double it.  This handles the case where the host is heavily loaded and
       * host (real) and guest (virtual) times diverge to the point where the
       * guest process timeouts before the VMX can reset the channel.  This
       * makes the guest process wait sufficiently long.  Note that since the
       * max above is 180 attempts, it is possible to wait 360 * sleepUsecs,
       * which by default is 360 seconds.
       */
      channelTimeoutAttempts *= 2;
      Debug("Backdoor resetting will be attemped at most %d times\n",
            channelTimeoutAttempts);
   }
                                                                                
   if (data->inError) {
      if (++(data->errorCount) > channelTimeoutAttempts) {
         Warning("Failed to reset backdoor after %d attempts\n",
                 data->errorCount - 1);
         return FALSE;
      }
                                                                                
      Debug("Resetting backdoor [%d]\n", data->errorCount);
      if (RpcIn_restart(data->in) == FALSE) {
         Warning("Backdoor reset failed [%d]\n", data->errorCount);
         return FALSE;
      }
      data->inError = FALSE;
                                                                                
      *sleepUsecs = (uint64)1000000;
   } else {
      if ( *sleepUsecs > 0 && data->errorCount > 0) {
         Debug("Backdoor was reset successfully\n");
         data->errorCount = 0;
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_SetOsPhase --
 *
 *    Set the guest OS phase in the VMX
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
ToolsDaemon_SetOsPhase(Bool stateChangeSucceeded, // IN
                       unsigned int cmdId)        // IN
{
   return RpcOut_sendOne(NULL, NULL, "tools.os.statechange.status %d %d",
                         stateChangeSucceeded, cmdId);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_GetMinResolution --
 *
 *      Get the minimum resolution (height and width) that we support
 *      setting this guest to.
 *
 *      This was originally added for bug 58681.
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
ToolsDaemon_GetMinResolution(GuestApp_Dict *dict,    // IN
                             unsigned int *width,    // OUT
                             unsigned int *height)   // OUT
{
   ASSERT(width);
   ASSERT(height);

   /*
    * This code is no longer used for Win9x platforms, and it's assumed that
    * all other platforms don't have a minimum.
    */
   *width = 0;
   *height = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_GetGuestTempDirectory --
 *
 *      Return the guest temp directory.
 *
 * Results:
 *      The guest temp directory
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
ToolsDaemon_GetGuestTempDirectory(void)
{
   return guestTempDirectory;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_InitializeForeignVM --
 *
 *      This is called when the tools are not running in a VM in VMware.
 *      Register appropriate backdoor procedures, and open the foreign tools
 *      listener socket.
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
ToolsDaemon_InitializeForeignVM(ToolsDaemon_Data *toolsDaemonData)   // IN
{
   Bool success;

   runningInForeignVM = TRUE;

   MessageStub_RegisterTransport();

   success = ForeignTools_Initialize(toolsDaemonData->optionsDict);
} // ToolsDaemon_InitializeForeignVM


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsDaemon_ShutdownForeignVM --
 *
 *      This is called when the tools are not running in a VM in VMware.
 *      Close the foreign tools listener socket.
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
ToolsDaemon_ShutdownForeignVM(void)
{
   ForeignTools_Shutdown();
} // ToolsDaemon_ShutdownForeignVM






#ifdef __cplusplus
}
#endif

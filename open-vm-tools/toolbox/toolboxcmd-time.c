/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * toolboxcmd-time.c --
 *
 *     The time sync operations for toolbox-cmd
 */

#include "toolboxCmdInt.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/guestrpc/timesync.h"
#include "vmware/tools/i18n.h"


/*
 *-----------------------------------------------------------------------------
 *
 * TimeSyncGetOldOptions --
 *
 *    Retrieve the tools options from VMware using the old (deprecated) method.
 *
 * Return value:
 *    The tools options
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static uint32
TimeSyncGetOldOptions(void)
{
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETGUIOPTIONS;
   Backdoor(&bp);
   return bp.out.ax.word;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSyncSet --
 *
 *      Enable/disable time sync
 *
 * Results:
 *      None
 *
 * Side effects:
 *      If time syncing is turned on the system time may be changed
 *      Prints to stderr and exits on errors
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
TimeSyncSet(Bool enable,      // IN:
            char **reply,     // OUT:
            size_t *replyLen) // OUT:
{
   gchar *msg = g_strdup_printf("vmx.set_option %s %s %s",
                                TOOLSOPTION_SYNCTIME,
                                !enable ? "1" : "0",
                                enable ? "1" : "0");
   return ToolsCmd_SendRPC(msg, strlen(msg) + 1, reply, replyLen);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSyncEnable --
 *
 *      Enable time sync.
 *
 * Results:
 *      EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 *
 * Side effects:
 *      Same as TimeSyncSet.
 *
 *-----------------------------------------------------------------------------
 */

static int
TimeSyncEnable(void)
{
   int ret = EXIT_SUCCESS;
   char *reply = NULL;
   size_t replyLen;

   if ((TimeSyncGetOldOptions() & VMWARE_GUI_SYNC_TIME) ||
       TimeSyncSet(TRUE, &reply, &replyLen)) {
      ToolsCmd_Print("%s\n", SU_(option.enabled, "Enabled"));
   } else {
      ToolsCmd_PrintErr(SU_(error.message, "Error: %s\n"), reply);
      ret = EXIT_FAILURE;
   }

   ToolsCmd_FreeRPC(reply);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSyncDisable --
 *
 *      Disable time sync.
 *
 * Results:
 *      EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 *
 * Side effects:
 *      Same as TimeSyncSet.
 *
 *-----------------------------------------------------------------------------
 */

static int
TimeSyncDisable(void)
{
   int ret = EXIT_SUCCESS;
   char *reply = NULL;
   size_t replyLen;

   if (!(TimeSyncGetOldOptions() & VMWARE_GUI_SYNC_TIME) ||
       TimeSyncSet(FALSE, &reply, &replyLen)) {
      ToolsCmd_Print("%s\n", SU_(option.disabled, "Disabled"));
   } else {
      ToolsCmd_PrintErr(SU_(error.message, "Error: %s\n"), reply);
      ret = EXIT_FAILURE;
   }

   ToolsCmd_FreeRPC(reply);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSyncStatus --
 *
 *      Checks the status of time sync in VMX.
 *
 * Results:
 *      EXIT_SUCCESS: time sync is enabled.
 *      EX_UNAVAILABLE: time sync is disabled.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
TimeSyncStatus(void)
{
   Bool status = FALSE;
   if (TimeSyncGetOldOptions() & VMWARE_GUI_SYNC_TIME) {
      status = TRUE;
   }
   if (status) {
      ToolsCmd_Print("%s\n", SU_(option.enabled, "Enabled"));
      return EXIT_SUCCESS;
   } else {
      ToolsCmd_Print("%s\n", SU_(option.disabled, "Disabled"));
      return EX_UNAVAILABLE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * TimeSyncCommand --
 *
 *      Parse and Handle timesync commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit code errors.
 *
 * Side effects:
 *      Might enable time sync, which would change the time in the guest os.
 *
 *-----------------------------------------------------------------------------
 */

int
TimeSync_Command(char **argv,     // IN: command line arguments
                 int argc,        // IN: The length of the command line arguments
                 gboolean quiet)  // IN
{
   if (toolbox_strcmp(argv[optind], "enable") == 0) {
      return TimeSyncEnable();
   } else if (toolbox_strcmp(argv[optind], "disable") == 0) {
      return TimeSyncDisable();
   } else if (toolbox_strcmp(argv[optind], "status") == 0) {
      return TimeSyncStatus();
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * TimeSync_Help --
 *
 *      Prints the help for timesync command.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
TimeSync_Help(const char *progName, // IN: The name of the program obtained from argv[0]
              const char *cmd)      // IN
{
   g_print(SU_(help.timesync, "%s: functions for controlling time synchronization on the guest OS\n"
                              "Usage: %s %s <subcommand>\n\n"
                              "Subcommands:\n"
                              "   enable: enable time synchronization\n"
                              "   disable: disable time synchronization\n"
                              "   status: print the time synchronization status\n"),
           cmd, progName, cmd);
}


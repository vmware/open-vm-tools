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

/*
 * toolboxcmd-time.c --
 *
 *     The time sync operations for toolbox-cmd
 */

#include "toolboxCmdInt.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/guestrpc/timesync.h"


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

static void
TimeSyncSet(Bool enable) // IN: status
{
   GuestApp_SetOptionInVMX(TOOLSOPTION_SYNCTIME,
                           !enable ? "1" : "0", enable ? "1" : "0");
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSync_Enable --
 *
 *      Enable time sync.
 *
 * Results:
 *      EXIT_SUCCESS
 *
 * Side effects:
 *      Same as TimeSyncSet.
 *
 *-----------------------------------------------------------------------------
 */

int
TimeSync_Enable(int quiet_flag) // IN: verbosity flag
{
   TimeSyncSet(TRUE);
   if (!quiet_flag) {
      printf("Enabled\n");
   }
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSync_Disable --
 *
 *      Disable time sync.
 *
 * Results:
 *      EXIT_SUCCESS
 *
 * Side effects:
 *      Same as TimeSyncSet.
 *
 *-----------------------------------------------------------------------------
 */

int
TimeSync_Disable(int quiet_flag) // IN: verbosity flag
{
   TimeSyncSet(FALSE);
   if (!quiet_flag) {
      printf("Disabled\n");
   }
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  TimeSync_Status --
 *
 *      Checks the status of time sync in VMX.
 *
 * Results:
 *      EXIT_SUCCESS
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
TimeSync_Status(void)
{
   Bool status = FALSE;
   if (GuestApp_OldGetOptions() & VMWARE_GUI_SYNC_TIME) {
      status = TRUE;
   }
   printf("%s\n", status ? "Enabled" : "Disabled");
   return EXIT_SUCCESS;
}

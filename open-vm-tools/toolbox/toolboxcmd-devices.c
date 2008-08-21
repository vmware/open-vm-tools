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
 * toolboxcmd-devices.c --
 *
 *     The devices functions for toolbox-cmd
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "toolboxInt.h"
#include "toolboxCmdInt.h"

static int DevicesSetStatus (char *devName, Bool enable, int quiet_flag);


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_ListDevices  --
 *
 *      prints device names and status to stdout.
 *
 * Results:
 *      EXIT_SUCCESS.
 *
 * Side effects:
 *      Prints to stdout.
 *
 *-----------------------------------------------------------------------------
 */

int
Devices_ListDevices(void)
{
   int i;
   for (i = 0; i < MAX_DEVICES; i++) {
      RD_Info info;
      if (GuestApp_GetDeviceInfo(i, &info) && strlen(info.name) > 0) {
         printf ("%s: %s\n", info.name, info.enabled ? "Enabled" : "Disabled");
      }
   }
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_DeviceStatus  --
 *
 *      Prints device names to stdout.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success
 *      Returns EXIT_OSFILE if devName was not found
 *
 * Side effects:
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Devices_DeviceStatus(char *devName)  // IN: Device Name
{
   int i;
   for (i = 0; i < MAX_DEVICES; i++) {
      RD_Info info;
      if (GuestApp_GetDeviceInfo(i, &info)
          && strcmp(info.name, devName) == 0) {
         printf("%s\n", info.enabled ? "Enabled" : "Disabled");
         return EXIT_SUCCESS;
      }
   }
   fprintf(stderr,
            "error fetching interface information: Device not found\n");
   return EX_OSFILE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DevicesSetStatus  --
 *
 *      Sets device status to the value in enable.
 *
 * Results:
 *      EXIT_SUCCESS on success
 *      EXIT_TEMPFAIL on failure to connect/disconnect a device
 *      EXIT_OSFILE if device is not found
 *
 * Side effects:
 *      Possibly connects or disconnects a device.
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
DevicesSetStatus(char *devName,  // IN: device name
                 Bool enable,    // IN: status
                 int quiet_flag) // IN: Verbosity flag
{
   int dev_id;
   for (dev_id = 0; dev_id < MAX_DEVICES; dev_id++) {
      RD_Info info;
      if (GuestApp_GetDeviceInfo(dev_id, &info)
	  && strcmp(info.name, devName) == 0) {
         if (!GuestApp_SetDeviceState(dev_id, enable)) {
            fprintf(stderr, "Unable to %s device %s\n", enable ? "connect"
                    : "disconnect", info.name);
            return EX_TEMPFAIL;
         }
         goto exit;
      }
   }
   fprintf(stderr,
           "error fetching interface information: Device not found\n");
   return EX_OSFILE;
  exit:
   if (!quiet_flag) {
      printf("%s\n", enable ? "Enabled" : "Disabled");
   }
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_EnableDevice  --
 *
 *      Connects a device.
 *
 * Results:
 *      Same as DevicesSetStatus.
 *
 * Side effects:
 *      Possibly connect a device.
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Devices_EnableDevice(char *name,     // IN: device name
                     int quiet_flag) // IN: Verbosity flag
{
   return DevicesSetStatus(name, TRUE, quiet_flag);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Devices_DisableDevice  --
 *
 *      disconnects a device.
 *
 * Results:
 *      Same as DevicesSetStatus.
 *
 * Side effects:
 *      Possibly disconnect a device.
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Devices_DisableDevice(char *name,     // IN: device name
                      int quiet_flag) // IN: Verbosity flag
{
   return DevicesSetStatus(name, FALSE, quiet_flag);
}

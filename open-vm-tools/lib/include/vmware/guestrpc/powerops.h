/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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

#ifndef _POWEROPS_H_
#define _POWEROPS_H_

/**
 * @file powerops.h
 *
 * Definition of commands and information related to VM power operations.
 */


#define TOOLSOPTION_SCRIPTS_POWERON    "toolScripts.afterPowerOn"
#define TOOLSOPTION_SCRIPTS_POWEROFF   "toolScripts.beforePowerOff"
#define TOOLSOPTION_SCRIPTS_RESUME     "toolScripts.afterResume"
#define TOOLSOPTION_SCRIPTS_SUSPEND    "toolScripts.beforeSuspend"


/**
 * The guest OS state changes that the VMX can initiate.
 */
typedef enum {
   /* Must be first. */
   GUESTOS_STATECHANGE_NONE = 0,

   GUESTOS_STATECHANGE_HALT,
   GUESTOS_STATECHANGE_REBOOT,
   GUESTOS_STATECHANGE_POWERON,
   GUESTOS_STATECHANGE_RESUME,
   GUESTOS_STATECHANGE_SUSPEND,

   /* Must be last. */
   GUESTOS_STATECHANGE_LAST,
} GuestOsState;


/**
 * Info regarding a state change command (OS_Halt, OS_Reboot, etc.)
 */
typedef struct GuestOsStateChangeCmd {
   unsigned int id;
   char const *name;
   char const *tcloCmd;
} GuestOsStateChangeCmd;


/**
 * The table of state change cmds corresponding to tclo commands.
 */
static const GuestOsStateChangeCmd stateChangeCmdTable[] = {
   { GUESTOS_STATECHANGE_POWERON, "poweron", "OS_PowerOn" },
   { GUESTOS_STATECHANGE_RESUME,  "resume",  "OS_Resume" },
   { GUESTOS_STATECHANGE_SUSPEND, "suspend", "OS_Suspend" },
   { GUESTOS_STATECHANGE_HALT,    "halt",    "OS_Halt" },
   { GUESTOS_STATECHANGE_REBOOT,  "reboot",  "OS_Reboot" },
};

#endif /* _POWEROPS_H_ */


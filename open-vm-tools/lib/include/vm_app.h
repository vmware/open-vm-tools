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
 * Common data structures used by the helper app that runs on the VM
 * and the user process.
 */

#ifndef _VM_APP_H_
#define _VM_APP_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

/*
 * The guest OS state changes that the VMX can initiate.
 */
typedef enum {
   /* Must be first. --hpreg */
   GUESTOS_STATECHANGE_NONE = 0,

   GUESTOS_STATECHANGE_HALT,
   GUESTOS_STATECHANGE_REBOOT,
   GUESTOS_STATECHANGE_POWERON,
   GUESTOS_STATECHANGE_RESUME,
   GUESTOS_STATECHANGE_SUSPEND,

   /* Must be last. --hpreg */
   GUESTOS_STATECHANGE_LAST,
} GuestOsState;

/*
 * Info regarding a state change command (OS_Halt, OS_Reboot, etc.)
 */
typedef struct GuestOsStateChangeCmd {
   unsigned int id;
   char const *name;
   char const *tcloCmd;
} GuestOsStateChangeCmd;

/*
 * The TCLO channel  names used by the tools daemon (guestd/
 * VMwareService) and tools UI (vmware-toolbox/VMwareControlPanel),
 * respectively.
 */
#define TOOLS_DAEMON_NAME         "toolbox"
#define TOOLS_CTLPANEL_NAME       "toolbox-ui"
#define TOOLS_DND_NAME            "toolbox-dnd"
#define TOOLS_UPGRADER_NAME       "tools-upgrader"
#define TOOLS_SSO_NAME            "tools-sso"
#define TOOLS_HGFS_NAME           "tools-hgfs"

/*
 * Option strings
 */
#define TOOLSOPTION_SYNCTIME      "synctime"
#define TOOLSOPTION_COPYPASTE     "copypaste"
#define TOOLSOPTION_AUTOHIDE      "autohide"
#define TOOLSOPTION_BROADCASTIP   "broadcastIP"
#define TOOLSOPTION_ENABLEDND     "enableDnD"
#define TOOLSOPTION_SYNCTIME_PERIOD "synctime.period"
#define TOOLSOPTION_SYNCTIME_ENABLE "time.synchronize.tools.enable"
#define TOOLSOPTION_SYNCTIME_STARTUP "time.synchronize.tools.startup"
#define TOOLSOPTION_MAP_ROOT_HGFS_SHARE "mapRootHgfsShare"
#define TOOLSOPTION_LINK_ROOT_HGFS_SHARE "linkRootHgfsShare"

/*
 * The max selection buffer length has to be less than the
 * ipc msg max size b/c the selection is transferred from mks -> vmx
 * and then through the backdoor to the tools. Also, leave
 * some room for ipc msg overhead. [greg]
 */
#define MAX_SELECTION_BUFFER_LENGTH             (1 << 16) - 100

#define VMWARE_DONT_EXCHANGE_SELECTIONS		-2
#define VMWARE_SELECTION_NOT_READY		-1

#define VMWARE_GUI_AUTO_GRAB			0x001
#define VMWARE_GUI_AUTO_UNGRAB			0x002
#define VMWARE_GUI_AUTO_SCROLL			0x004
#define VMWARE_GUI_AUTO_RAISE			0x008
#define VMWARE_GUI_EXCHANGE_SELECTIONS		0x010
#define VMWARE_GUI_WARP_CURSOR_ON_UNGRAB	0x020
#define VMWARE_GUI_FULL_SCREEN			0x040

#define VMWARE_GUI_TO_FULL_SCREEN		0x080
#define VMWARE_GUI_TO_WINDOW			0x100

#define VMWARE_GUI_AUTO_RAISE_DISABLED		0x200

#define VMWARE_GUI_SYNC_TIME			0x400

/*
 * When set, toolboxes should not show the cursor options page.
 */

#define VMWARE_DISABLE_CURSOR_OPTIONS		0x800


/*
 * The table of state change cmds corresponding to tclo commands.
 */
static const GuestOsStateChangeCmd stateChangeCmdTable[] = {
   { GUESTOS_STATECHANGE_POWERON, "poweron", "OS_PowerOn" },
   { GUESTOS_STATECHANGE_RESUME,  "resume",  "OS_Resume" },
   { GUESTOS_STATECHANGE_SUSPEND, "suspend", "OS_Suspend" },
   { GUESTOS_STATECHANGE_HALT,    "halt",    "OS_Halt" },
   { GUESTOS_STATECHANGE_REBOOT,  "reboot",  "OS_Reboot" },
};


#endif

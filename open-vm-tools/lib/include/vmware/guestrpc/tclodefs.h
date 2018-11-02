/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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

#ifndef _TCLODEFS_H_
#define _TCLODEFS_H_

/**
 * @file tclodefs.h
 *
 * Miscelaneous GuestRPC definitions. There include generic definitions not
 * particular to any application, and also some application-specific definitions
 * that don't really justify a separate header file.
 */

/** TCLO channel name for the main Tools service. */
#define TOOLS_DAEMON_NAME         "toolbox"
/** TCLO channel name for the Tools configuration UI. */
#define TOOLS_CTLPANEL_NAME       "toolbox-ui"
/** TCLO channel name for the Tools user service. */
#define TOOLS_DND_NAME            "toolbox-dnd"
/** TCLO channel name for the Tools upgraded. */
#define TOOLS_UPGRADER_NAME       "tools-upgrader"
/** TCLO channel name for the VDI single-sign-on service. */
#define TOOLS_SSO_NAME            "tools-sso"
/** TCLO channel name for the HGFS driver. */
#define TOOLS_HGFS_NAME           "tools-hgfs"

/*
 * The TCLO channel names used by the View Agent Service.
 * vdiagent may register RPC callbacks to be notified of certain events not
 * handled by Tools Service.
 */
#define TOOLS_VDIAGENT_NAME       "vdiagent"

/** Reply from host when the command is not recognized. */
#define RPCI_UNKNOWN_COMMAND      "Unknown command"

#define GUESTRPC_TCLO_VSOCK_LISTEN_PORT      975
#define GUESTRPC_RPCI_VSOCK_LISTEN_PORT      976

/*
 * Tools options.
 */

#define TOOLSOPTION_COPYPASTE                     "copypaste"
#define TOOLSOPTION_AUTOHIDE                      "autohide"
#define TOOLSOPTION_BROADCASTIP                   "broadcastIP"
#define TOOLSOPTION_ENABLEDND                     "enableDnD"
#define TOOLSOPTION_MAP_ROOT_HGFS_SHARE           "mapRootHgfsShare"
#define TOOLSOPTION_LINK_ROOT_HGFS_SHARE          "linkRootHgfsShare"
#define TOOLSOPTION_ENABLE_MESSAGE_BUS_TUNNEL     "enableMessageBusTunnel"
#define TOOLSOPTION_GUEST_LOG_LEVEL               "guestLogLevel"

/*
 * Auto-upgrade commands.
 */

#define AUTOUPGRADE_AVAILABLE_CMD   "vmx.capability.tools_is_upgradable"
#define AUTOUPGRADE_START_CMD       "guest.initiateAutoUpgrade"

/* More upgrader commands. */
#define GUEST_UPGRADER_SEND_CMD_LINE_ARGS  "guest.upgrader_send_cmd_line_args"

/*
 * Shrink commands.
 */

#define DISK_SHRINK_CMD             "disk.shrink"

/*
 * Auto-lock commands.
 */

#define DESKTOP_AUTOLOCK_CMD        "Autolock_Desktop"

/*
 * Guest log commands.
 */

#define GUEST_LOG_STATE_CMD "guest.log.state"
#define GUEST_LOG_TEXT_CMD "guest.log.text"

/*
 *  Update tools health command.
 */
#define UPDATE_TOOLS_HEALTH_CMD "update.tools.health"
#define TOOLS_HEALTH_NORMAL_KEY "normal"
#define TOOLS_HEALTH_HUNG_KEY "hung"
#define TOOLS_HEALTH_GUEST_SLOW_KEY "guest_slow"

/*
 * The max selection buffer length has to be less than the
 * ipc msg max size b/c the selection is transferred from mks -> vmx
 * and then through the backdoor to the tools. Also, leave
 * some room for ipc msg overhead.
 */
#define MAX_SELECTION_BUFFER_LENGTH       ((1 << 16) - 100)

#define VMWARE_DONT_EXCHANGE_SELECTIONS	(-2)
#define VMWARE_SELECTION_NOT_READY        (-1)

#define VMWARE_GUI_AUTO_GRAB              0x001
#define VMWARE_GUI_AUTO_UNGRAB            0x002
#define VMWARE_GUI_AUTO_SCROLL            0x004
#define VMWARE_GUI_AUTO_RAISE             0x008
#define VMWARE_GUI_EXCHANGE_SELECTIONS    0x010
#define VMWARE_GUI_WARP_CURSOR_ON_UNGRAB  0x020
#define VMWARE_GUI_FULL_SCREEN            0x040

#define VMWARE_GUI_TO_FULL_SCREEN         0x080
#define VMWARE_GUI_TO_WINDOW              0x100

#define VMWARE_GUI_AUTO_RAISE_DISABLED    0x200

#define VMWARE_GUI_SYNC_TIME              0x400

/* When set, toolboxes should not show the cursor options page. */
#define VMWARE_DISABLE_CURSOR_OPTIONS     0x800

#define  RPCIN_TCLO_PING                 0x1

enum {
   GUESTRPCPKT_TYPE_DATA = 1,
   GUESTRPCPKT_TYPE_PING
};

enum {
   GUESTRPCPKT_FIELD_TYPE = 1,
   GUESTRPCPKT_FIELD_PAYLOAD
};

#endif /* _TCLODEFS_H_ */


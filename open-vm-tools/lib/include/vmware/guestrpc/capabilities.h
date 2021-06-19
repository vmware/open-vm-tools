/*********************************************************
 * Copyright (C) 2008-2017,2020-2021 VMware, Inc. All rights reserved.
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
 * capabilities.h --
 *
 *  Common definitions for the guestCaps system that allows a guest to
 *  register an arbitrary number of boolean capabilites with the vmx.
 */

#ifndef _GUEST_CAPS_H_
#define _GUEST_CAPS_H_

/*
 * Guest capabilities.
 * The guest uses this enum to communicate whether a certain
 * feature is supported by the tools.
 * The guest sends an RPC where it specifies which features
 * are turned off and on, for example
 * "tools.capability.features 0=1 2=1 3=0".
 * In the above example, the guest is capable of showing the
 * start menu and setting the work area, but does not support
 * multiple monitors.
 *
 * NOTE: the order for these has to stay constant for backward compatibility
 * with older Tools versions. New capabilities must be added at the end.
 */

typedef enum {
   UNITY_CAP_START_MENU                 = 0,  // can show the start menu
   UNITY_CAP_VIRTUAL_DESK               = 1,  // supports virtual desktops
   UNITY_CAP_WORK_AREA                  = 2,  // can set the work area
   UNITY_CAP_MULTI_MON                  = 3,  // supports multiple monitors
   GHI_CAP_SHELL_ACTION_BROWSE          = 4,  // supports the "browse" action verb
   GHI_CAP_SHELL_LOCATION_HGFS          = 5,  // supports HGFS location URIs
   GHI_CAP_SHELL_ACTION_RUN             = 6,  // supports the "run" action verb
   GHI_CAP_CMD_SHELL_ACTION             = 7,  // allows "ghi.guest.shell.action" command
   HGFSU_CAP_MIRROR_DESKTOP             = 8,  // supports remapping GOS Desktop to HGFS
   HGFSU_CAP_MIRROR_DOCUMENTS           = 9,  // supports remapping GOS Documents to HGFS
   HGFSU_CAP_MIRROR_MUSIC               = 10, // supports remapping GOS Music to HGFS
   HGFSU_CAP_MIRROR_PICTURES            = 11, // supports remapping GOS Pictures to HGFS
   HGFSU_CAP_DESKTOP_SHORTCUT           = 12, // supports creating HGFS link on GOS Desktop
   HGFSU_CAP_MAP_DRIVE                  = 13, // supports mapping a GOS drive letter to HGFS
   GHI_CAP_SET_HANDLER                  = 14, // supports setting the handler for types/protocols
   UNITY_CAP_STATUS_UNITY_ACTIVE        = 15, // supports GuestRpc bits for Unity Status
   GHI_CAP_SET_OUTLOOK_TEMP_FOLDER      = 16, // supports setting the Outlook temp folder
                                              // 17 is obsolete, do not use
   CAP_SET_TOPO_MODES                   = 18, // supports setting topology modes in video driver
   GHI_CAP_TRAY_ICONS                   = 19, // supports ghi.guest.trayIcon commands
   GHI_CAP_SET_FOCUSED_WINDOW           = 20, // supports ghi.guest.setFocusedWindow
   GHI_CAP_GET_EXEC_INFO_HASH           = 21, // supports ghi.guest.getExecInfoHash
   UNITY_CAP_STICKY_WINDOWS             = 22, // supports unity.window.{un,}stick
   CAP_CHANGE_HOST_3D_AVAILABILITY_HINT = 23, // supports sending 3D support hint to guest
   CAP_AUTOUPGRADE_AT_SHUTDOWN          = 24, // supports auto-upgrading tools at OS shutdown
   GHI_CAP_AUTOLOGON                    = 25, // supports autologon
   CAP_DESKTOP_AUTOLOCK                 = 26, // supports desktop autolock
                                              // 27 is obsolete, do not use
   HGFSU_CAP_MIRROR_DOWNLOADS           = 28, // supports remapping GOS Downloads to HGFS
   HGFSU_CAP_MIRROR_MOVIES              = 29, // supports remapping GOS Movies to HGFS
   GHI_CAP_TOGGLE_START_UI              = 30, // supports showing/hiding the Start UI
   GHI_CAP_SET_DISPLAY_SCALING          = 31, // supports setting the display scaling (DPI)
   UNITY_CAP_DISABLE_MOUSE_BUTTON_SWAPPING     = 32, // supports disabling mouse button swapping
   UNITY_CAP_CARET_POSITION             = 33, // supports sending caret position updates
   CAP_GUESTSTORE_UPGRADE               = 34, // supports tools upgrade from GuestStore
   CAP_DEVICE_HELPER                    = 35, // supports tools device helper for Windows guests
   CAP_VMBACKUP_NVME                    = 36, // supports NVMe for vmbackup
} GuestCapabilities;

typedef struct {
   GuestCapabilities cap;
   const char *vmdbPath;
   const char *vmdbKey;
} GuestCapElem;

/* guest_rpc command to send over the wire. */
#define GUEST_CAP_FEATURES                   "tools.capability.features"

#if defined(VM_NEED_VMDB_GUEST_CAP_MAPPING)

/* VMDB paths prefixes to store various capabilities sent from the guest. */
#define UNITY_CAP_VMDB_PATH                  "guest/caps/unityFeatures"
#define GHI_CAP_VMDB_PATH                    "guest/caps/ghiFeatures"
#define HGFSU_CAP_VMDB_PATH                  "guest/caps/hgfsUsabilityFeatures"
#define CAP_VMDB_PATH                        "guest/caps"

/*
 * If you change these strings, make sure you also change the
 *  vmdb schema, since these strings are used as vmdb keys.
 */
// clang-format off
static GuestCapElem guestCapTable[] = {
   { UNITY_CAP_START_MENU,                 UNITY_CAP_VMDB_PATH, "startmenu" },
   { UNITY_CAP_VIRTUAL_DESK,               UNITY_CAP_VMDB_PATH, "virtualdesk" },
   { UNITY_CAP_WORK_AREA,                  UNITY_CAP_VMDB_PATH, "workarea" },
   { UNITY_CAP_MULTI_MON,                  UNITY_CAP_VMDB_PATH, "multimon" },

   { GHI_CAP_SHELL_ACTION_BROWSE,          GHI_CAP_VMDB_PATH,   "shellActionBrowse" },
   { GHI_CAP_SHELL_LOCATION_HGFS,          GHI_CAP_VMDB_PATH,   "shellLocationHGFS" },
   { GHI_CAP_SHELL_ACTION_RUN,             GHI_CAP_VMDB_PATH,   "shellActionRun" },
   { GHI_CAP_CMD_SHELL_ACTION,             GHI_CAP_VMDB_PATH,   "cmdShellAction" },

   { HGFSU_CAP_MIRROR_DESKTOP,             HGFSU_CAP_VMDB_PATH, "mirrorDesktop" },
   { HGFSU_CAP_MIRROR_DOCUMENTS,           HGFSU_CAP_VMDB_PATH, "mirrorDocuments" },
   { HGFSU_CAP_MIRROR_MUSIC,               HGFSU_CAP_VMDB_PATH, "mirrorMusic" },
   { HGFSU_CAP_MIRROR_PICTURES,            HGFSU_CAP_VMDB_PATH, "mirrorPictures" },
   { HGFSU_CAP_DESKTOP_SHORTCUT,           HGFSU_CAP_VMDB_PATH, "createShortcut" },
   { HGFSU_CAP_MAP_DRIVE,                  HGFSU_CAP_VMDB_PATH, "mapDrive" },
   { GHI_CAP_SET_HANDLER,                  GHI_CAP_VMDB_PATH,   "setHandler" },
   { UNITY_CAP_STATUS_UNITY_ACTIVE,        UNITY_CAP_VMDB_PATH, "unityActive" },
   { GHI_CAP_SET_OUTLOOK_TEMP_FOLDER,      GHI_CAP_VMDB_PATH,   "setOutlookTempFolder" },
   { CAP_SET_TOPO_MODES,                   CAP_VMDB_PATH,       "displayTopologyModesSet" },
   { GHI_CAP_TRAY_ICONS,                   GHI_CAP_VMDB_PATH,   "trayIcons" },
   { GHI_CAP_SET_FOCUSED_WINDOW,           GHI_CAP_VMDB_PATH,   "setFocusedWindow"},
   { GHI_CAP_GET_EXEC_INFO_HASH,           GHI_CAP_VMDB_PATH,   "getExecInfoHash"},
   { UNITY_CAP_STICKY_WINDOWS,             UNITY_CAP_VMDB_PATH, "sticky"},
   { CAP_CHANGE_HOST_3D_AVAILABILITY_HINT, CAP_VMDB_PATH,       "changeHost3DAvailabilityHint" },
   { CAP_AUTOUPGRADE_AT_SHUTDOWN,          CAP_VMDB_PATH,       "autoUpgradeAtShutdown"},
   { GHI_CAP_AUTOLOGON,                    GHI_CAP_VMDB_PATH,   "autologon" },
   { CAP_DESKTOP_AUTOLOCK,                 CAP_VMDB_PATH,       "desktopAutolock" },
   { HGFSU_CAP_MIRROR_DOWNLOADS,           HGFSU_CAP_VMDB_PATH, "mirrorDownloads" },
   { HGFSU_CAP_MIRROR_MOVIES,              HGFSU_CAP_VMDB_PATH, "mirrorMovies" },
   { GHI_CAP_TOGGLE_START_UI,              GHI_CAP_VMDB_PATH,   "toggleStartUI"},
   { GHI_CAP_SET_DISPLAY_SCALING,          GHI_CAP_VMDB_PATH,   "setDisplayScaling"},
   { UNITY_CAP_DISABLE_MOUSE_BUTTON_SWAPPING, UNITY_CAP_VMDB_PATH, "mouseButtonSwapping" },
   { UNITY_CAP_CARET_POSITION,             UNITY_CAP_VMDB_PATH, "getCaretPosition" },
   /*
    * GuestStoreUpgrade is available on ESXi only at this time. Therefore, we
    * don't define VMDB schema for it and don't store it in VMDB.
    */
   { CAP_GUESTSTORE_UPGRADE,               NULL,                NULL },
   { CAP_DEVICE_HELPER,                    NULL,                NULL },
   { CAP_VMBACKUP_NVME,                    NULL,                NULL },
};
// clang-format on

#endif // VM_NEED_VMDB_GUEST_CAP_MAPPING

#endif // _GUEST_CAPS_H_

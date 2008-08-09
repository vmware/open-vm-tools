/*********************************************************
 * Copyright (C) 2007-2008 VMware, Inc. All rights reserved.
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
 * unityCommon.h --
 *
 *    Types and GuestRPC commands that comprise the Unity protocol.
 */

#ifndef _UNITY_COMMON_H_
#define _UNITY_COMMON_H_

#define UNITY_MAX_SETTOP_WINDOW_COUNT 100

/* RPC messages between host and guest that are part of Unity protocol. */

/* Host -> Guest */
#define UNITY_RPC_ENTER                   "unity.enter"
#define UNITY_RPC_GET_UPDATE              "unity.get.update"
#define UNITY_RPC_GET_WINDOW_PATH         "unity.get.window.path"
#define UNITY_RPC_GET_BINARY_INFO         "unity.get.binary.info"
#define UNITY_RPC_GET_BINARY_HANDLERS     "unity.get.binary.handlers"
#define UNITY_RPC_OPEN_LAUNCHMENU         "unity.launchmenu.open"
#define UNITY_RPC_GET_LAUNCHMENU_ITEM     "unity.launchmenu.get.item"
#define UNITY_RPC_CLOSE_LAUNCHMENU        "unity.launchmenu.close"
#define UNITY_RPC_WINDOW_RESTORE          "unity.window.restore"
#define UNITY_RPC_WINDOW_SETTOP           "unity.window.settop"
#define UNITY_RPC_WINDOW_CLOSE            "unity.window.close"
#define UNITY_RPC_GET_WINDOW_CONTENTS     "unity.get.window.contents"
#define UNITY_RPC_GET_ICON_DATA           "unity.get.icon.data"
#define UNITY_RPC_EXIT                    "unity.exit"
#define UNITY_RPC_GET_UPDATE_FULL         "unity.get.update.full"
#define UNITY_RPC_GET_UPDATE_INCREMENTAL  "unity.get.update.incremental"
#define UNITY_RPC_SHELL_OPEN              "unity.shell.open"
#define UNITY_RPC_SHOW_TASKBAR            "unity.show.taskbar"
#define UNITY_RPC_WINDOW_MOVE_RESIZE      "unity.window.move_resize"
#define UNITY_RPC_DESKTOP_WORK_AREA_SET   "unity.desktop.work_area.set"
#define UNITY_RPC_WINDOW_SHOW             "unity.window.show"
#define UNITY_RPC_WINDOW_HIDE             "unity.window.hide"
#define UNITY_RPC_WINDOW_MINIMIZE         "unity.window.minimize"
#define UNITY_RPC_WINDOW_MAXIMIZE         "unity.window.maximize"
#define UNITY_RPC_WINDOW_UNMAXIMIZE       "unity.window.unmaximize"
#define UNITY_RPC_DESKTOP_CONFIG_SET      "unity.desktop.config.set"
#define UNITY_RPC_DESKTOP_ACTIVE_SET      "unity.desktop.active.set"
#define UNITY_RPC_WINDOW_DESKTOP_SET      "unity.window.desktop.set"

#define GHI_RPC_GUEST_SHELL_ACTION                    "ghi.guest.shell.action"
#define GHI_RPC_SET_GUEST_HANDLER                     "ghi.guest.handler.set"
#define GHI_RPC_RESTORE_DEFAULT_GUEST_HANDLER         "ghi.guest.handler.restoreDefault"

/* Guest -> Host */
#define UNITY_RPC_PUSH_UPDATE_CMD         "tools.unity.push.update"
#define UNITY_RPC_VMX_SHOW_TASKBAR        "vmx.unity.show.taskbar"
#define UNITY_RPC_UNITY_CAP               "tools.capability.unity"
#define UNITY_RPC_SHOW_TASKBAR_CAP        "tools.capability.unity.taskbar"
#define GHI_RPC_LAUNCHMENU_CHANGE         "tools.ghi.launchmenu.change"
#define GHI_RPC_PROTOCOL_HANDLER_INFO     "tools.ghi.protocolhandler.info"

#define GHI_RPC_HOST_SHELL_ACTION         "ghi.host.shell.action"

/*
 * Currently we have four possible unity start menu roots.
 *
 * UNITY_START_MENU_LAUNCH_FOLDER is for all guest start menu 'Programs' items
 * plus favorite items from guest start menu folder.
 *
 * UNITY_START_MENU_FIXED_FOLDER is for special items like 'My Computer',
 * 'My Documents', 'Control Panel', etc.
 *
 * UNITY_START_MENU_ALL_HANDLERS_FOLDER is for all the applications that are known
 * by the guest to open files.
 *
 * UNITY_START_MENU_RESOLVED_LAUNCH_FOLDER is the same contents as
 * UNITY_START_MENU_LAUNCH_FOLDER however each item that is a shortcut (link) is resolved
 * into its destination path.
 */

#define UNITY_START_MENU_LAUNCH_FOLDER          "VMGuestLaunchItems"
#define UNITY_START_MENU_FIXED_FOLDER           "VMGuestFixedItems"
#define UNITY_START_MENU_ALL_HANDLERS_FOLDER    "VMGuestAllHandlers"
#define UNITY_START_MENU_RESOLVED_LAUNCH_FOLDER "VMGuestResolvedItems"

typedef uint32 UnityWindowId;

typedef int32 UnityDesktopId;

/*
 * Unity window state tracks the current state of the window (right now we only have
 * minimized, might have more in the future).
 *
 * These are deprecated in favor of window attributes and window types, and are retained
 * for compatibility purposes only.
 */

#define UNITY_WINDOW_STATE_MINIMIZED   (1 << 0)
#define UNITY_WINDOW_STATE_IN_FOCUS    (1 << 1)
#define UNITY_WINDOW_STATE_TOPMOST     (1 << 2)

/*
 * Unity window attributes are boolean flags that can be set in combination on a window.
 * If they are not set by the guest, it is up to the host to decide on a reasonable
 * default.
 *
 * If you add attributes here, please also update unityAttributeNames in
 * unityWindowTracker.c.
 */

typedef enum {
   UNITY_WINDOW_ATTR_BORDERLESS = 0,
   UNITY_WINDOW_ATTR_MINIMIZABLE = 1,
   UNITY_WINDOW_ATTR_MAXIMIZABLE = 2,
   UNITY_WINDOW_ATTR_MAXIMIZED = 3,
   UNITY_WINDOW_ATTR_CLOSABLE = 5,
   UNITY_WINDOW_ATTR_HAS_TITLEBAR = 6,
   UNITY_WINDOW_ATTR_VISIBLE = 7,
   UNITY_WINDOW_ATTR_CHILD_WINDOW = 8,
   UNITY_WINDOW_ATTR_HAS_TOOLBAR_BTN = 9,
   UNITY_WINDOW_ATTR_BELONGS_TO_APP = 10,
   UNITY_WINDOW_ATTR_DROPSHADOWED = 11,
   UNITY_WINDOW_ATTR_ALWAYS_ABOVE = 12,
   UNITY_WINDOW_ATTR_ALWAYS_BELOW = 13,
   UNITY_WINDOW_ATTR_DISABLED = 14,
   UNITY_WINDOW_ATTR_NOACTIVATE = 15,
   UNITY_WINDOW_ATTR_SYSMENU = 16,
   UNITY_WINDOW_ATTR_TOOLWINDOW = 17,
   UNITY_WINDOW_ATTR_APPWINDOW = 18,
   UNITY_WINDOW_ATTR_FULLSCREENABLE = 19,
   UNITY_WINDOW_ATTR_FULLSCREENED = 20,
   UNITY_WINDOW_ATTR_ATTN_WANTED = 21,
   UNITY_WINDOW_ATTR_SHADEABLE = 22,
   UNITY_WINDOW_ATTR_SHADED = 23,
   UNITY_WINDOW_ATTR_STICKABLE = 24,
   UNITY_WINDOW_ATTR_STICKY = 25,
   UNITY_WINDOW_ATTR_MODAL = 26,

   UNITY_MAX_ATTRIBUTES // Not a valid attribute
} UnityWindowAttribute;

typedef enum {
   UNITY_WINDOW_TYPE_NONE   = -1,
   UNITY_WINDOW_TYPE_NORMAL = 0,
   UNITY_WINDOW_TYPE_PANEL,
   UNITY_WINDOW_TYPE_DIALOG,
   UNITY_WINDOW_TYPE_MENU,
   UNITY_WINDOW_TYPE_TOOLTIP,
   UNITY_WINDOW_TYPE_SPLASH,
   UNITY_WINDOW_TYPE_TOOLBAR,
   UNITY_WINDOW_TYPE_DOCK,
   UNITY_WINDOW_TYPE_DESKTOP,
   UNITY_WINDOW_TYPE_COMBOBOX,
   UNITY_WINDOW_TYPE_WIDGET,

   UNITY_MAX_WINDOW_TYPES  // Not a valid window type
} UnityWindowType;

typedef enum {
   UNITY_ICON_TYPE_MAIN = 0,

   UNITY_MAX_ICONS // Not a valid icon type
} UnityIconType;

typedef uint32 UnityIconSize; // Number of pixels on the larger side of the icon (which is usually square anyways).
#define UNITY_MAX_ICON_DATA_CHUNK ((1 << 16) - 100) // 64k, minus space for a few other return values

#define UNITY_DEFAULT_COLOR "#c0c0c0"

#endif

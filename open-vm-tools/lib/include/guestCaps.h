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
 * guestCaps.h --
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
   UNITY_CAP_START_MENU        = 0,  // can show the start menu
   UNITY_CAP_VIRTUAL_DESK      = 1,  // supports virtual desktops
   UNITY_CAP_WORK_AREA         = 2,  // can set the work area
   UNITY_CAP_MULTI_MON         = 3,  // supports multiple monitors
   GHI_CAP_SHELL_ACTION_BROWSE = 4,  // supports the "browse" action verb
   GHI_CAP_SHELL_LOCATION_HGFS = 5,  // supports HGFS location URIs
   GHI_CAP_SHELL_ACTION_RUN    = 6,  // supports the "run" action verb
   GHI_CAP_CMD_SHELL_ACTION    = 7,  // allows "ghi.guest.shell.action" command
   HGFSU_CAP_MIRROR_DESKTOP    = 8,  // supports remapping GOS Desktop to HGFS
   HGFSU_CAP_MIRROR_DOCUMENTS  = 9,  // supports remapping GOS Documents to HGFS
   HGFSU_CAP_MIRROR_MUSIC      = 10, // supports remapping GOS Music to HGFS
   HGFSU_CAP_MIRROR_PICTURES   = 11, // supports remapping GOS Pictures to HGFS
   HGFSU_CAP_DESKTOP_SHORTCUT  = 12, // supports creating HGFS link on GOS Desktop
   HGFSU_CAP_MAP_DRIVE         = 13, // supports mapping a GOS drive letter to HGFS
   GHI_CAP_SET_HANDLER         = 14, // supports setting the handler for types/protocols
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

/*
 * If you change these strings, make sure you also change the
 *  vmdb schema, since these strings are used as vmdb keys.
 */

/*
 * This table must be sorted such that it can be indexed using the 
 * GuestCapabilities enum above. RPC calls pass the value, and the
 * handler code uses it as an index. In other words, the value of the 
 * caps field at index i must be equal to i as well. This is because
 * the code that looks up entries in this table assume as much. It
 * also means we don't need the cap field, or, to justify its existence,
 * the lookup code should be converted to loop through the table and
 * return the entry where cap == the value passed in the RPC call.
 * Moral of the story, new entries always at the bottom of the table
 * and the cap field must be set to the offset in the array (and make
 * sure the enum in GuestCapabilities is also set to that offset).
 */

static GuestCapElem guestCapTable[] = {
   { UNITY_CAP_START_MENU,        UNITY_CAP_VMDB_PATH, "startmenu" },
   { UNITY_CAP_VIRTUAL_DESK,      UNITY_CAP_VMDB_PATH, "virtualdesk" },
   { UNITY_CAP_WORK_AREA,         UNITY_CAP_VMDB_PATH, "workarea" },
   { UNITY_CAP_MULTI_MON,         UNITY_CAP_VMDB_PATH, "multimon" },

   { GHI_CAP_SHELL_ACTION_BROWSE, GHI_CAP_VMDB_PATH,   "shellActionBrowse" },
   { GHI_CAP_SHELL_LOCATION_HGFS, GHI_CAP_VMDB_PATH,   "shellLocationHGFS" },
   { GHI_CAP_SHELL_ACTION_RUN,    GHI_CAP_VMDB_PATH,   "shellActionRun" },
   { GHI_CAP_CMD_SHELL_ACTION,    GHI_CAP_VMDB_PATH,   "cmdShellAction" },

   { HGFSU_CAP_MIRROR_DESKTOP,    HGFSU_CAP_VMDB_PATH, "mirrorDesktop" },
   { HGFSU_CAP_MIRROR_DOCUMENTS,  HGFSU_CAP_VMDB_PATH, "mirrorDocuments" },
   { HGFSU_CAP_MIRROR_MUSIC,      HGFSU_CAP_VMDB_PATH, "mirrorMusic" },
   { HGFSU_CAP_MIRROR_PICTURES,   HGFSU_CAP_VMDB_PATH, "mirrorPictures" },
   { HGFSU_CAP_DESKTOP_SHORTCUT,  HGFSU_CAP_VMDB_PATH, "createShortcut" },
   { HGFSU_CAP_MAP_DRIVE,         HGFSU_CAP_VMDB_PATH, "mapDrive" },
   { GHI_CAP_SET_HANDLER,         GHI_CAP_VMDB_PATH,   "setHandler" },
};

#endif // VM_NEED_VMDB_GUEST_CAP_MAPPING

#endif // _GUEST_CAPS_H_

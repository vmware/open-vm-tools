/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * ghiTrayIcon.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide access to the guest tray icons to the host.
 */

/*
 * Using the opaque type causes rpcgen to emit code that calls the htonl and
 * ntohl functions. On Windows, these functions are defined in winsock2.h, so
 * we need to #include that header in the generated code.
 */
%#ifdef WIN32
%#include "winsock2.h"
%#endif /* WIN32 */

enum GHITrayIconVersion {
   GHI_TRAY_ICON_V1 = 1
};

/* These are the values used in the op member below. */
enum GHITrayIconOp {
   GHI_TRAY_ICON_OP_ADD = 1,
   GHI_TRAY_ICON_OP_MODIFY,
   GHI_TRAY_ICON_OP_DELETE
};

/* These are the flag values used in the flags bitmask below. */
enum GHITrayIconFlags {
   GHI_TRAY_ICON_FLAG_PNGDATA      = 1, /* The pngData is valid. */
   GHI_TRAY_ICON_FLAG_TOOLTIP      = 2, /* The tooltip is valid. */
   GHI_TRAY_ICON_FLAG_BLACKLISTKEY = 4  /* The blacklistKey is valid. */
};

/*
 * These structures are sent from the guest to the host whenver a new tray
 * icon is added, or an existing tray icon is modified. When an icon is
 * modified, the flags member of this structure is used as a bitmask to
 * indicate the changed values.
 *
 * Note that the identifier string assigned to the icon should be treated as
 * an opaque value. The contents of this string may change without notice in
 * new versions of the guest tools. This identifier is not persistent.
 *
 * The blacklist key is used by the host to blacklist tray icons. We can't use
 * iconID for this purpose because iconID must be unique, and the blacklist
 * key is not always unique (see bug 461750).
 */
struct GHITrayIconV1 {
   string iconID<>;       /* opaque identifier for the icon */
   GHITrayIconOp op;      /* action to perform */
   uint32 flags;          /* bitmask of valid items */
   uint32 width;          /* PNG image width */
   uint32 height;         /* PNG image height */
   opaque pngData<>;      /* PNG image data for the icon */
   string tooltip<>;      /* tooltip string for the icon, UTF8 encoded */
   string blacklistKey<>; /* key used for icon blacklisting */
};

union GHITrayIcon switch (GHITrayIconVersion ver) {
case GHI_TRAY_ICON_V1:
   struct GHITrayIconV1 *trayIconV1;
};

enum GHITrayIconEventVersion {
   GHI_TRAY_ICON_EVENT_V1 = 1
};

/*
 * Event messages are sent from the host to the guest when the user interacts
 * with the host representation of the tray icon (e.g. right-clicks on it).
 *
 * XXX The (x, y) co-ordinates in this structure must be relative to the guest
 *     screen origin.
 */
struct GHITrayIconEventV1 {
   string iconID<>; /* identifier of the icon */
   uint32 event;    /* event identifier, see unityCommon.h */
   uint32 x;        /* guest x co-ordinate of the event */
   uint32 y;        /* guest y co-ordinate of the event */
};

union GHITrayIconEvent switch (GHITrayIconEventVersion ver) {
case GHI_TRAY_ICON_EVENT_V1:
   struct GHITrayIconEventV1 *trayIconEventV1;
};

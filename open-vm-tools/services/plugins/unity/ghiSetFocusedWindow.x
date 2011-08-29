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
* ghiSetFocusedWindow.x --
*
*    Definition of the data structures used in the GuestRpc commands to
*    set a window to be focused.
*/

#include "ghiCommonDefines.h"

/*
* Enumerates the different versions of the messages.
*/
enum GHISetFocusedWindowVersion {
  GHI_SET_FOCUSED_WINDOW_V1 = 1
};

/*
* The structure used for version 1 of the message.
*/
struct GHISetFocusedWindowV1 {
  int windowId;
};

/*
 * This defines the protocol for a 'setFocusedWindow' message.
 *
 * The union allows us to introduce new versions of the protocol later by
 * creating new values in the enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail if
 * an unknown version is provided on the wire.
 */
union GHISetFocusedWindow switch (GHISetFocusedWindowVersion ver) {
case GHI_SET_FOCUSED_WINDOW_V1:
   struct GHISetFocusedWindowV1 *setFocusedWindowV1;
};


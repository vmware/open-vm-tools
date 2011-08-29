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
 * ghiSetGuestHandler.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    set or restore the handler for a given protocol or file type in the
 *    guest.
 */

#include "ghiCommonDefines.h"

/*
 * Enumerates the different versions of the messages.
 */
enum GHISetGuestHandlerVersion {
    GHI_SET_GUEST_HANDLER_V1 = 1
};


struct GHISetGuestHandlerAction {
   string actionURI<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
   string targetURI<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
};

struct GHISetGuestHandlerV1 {
   /*
    * The file suffix (including leading period character), or for protocols
    * the handler type ('http', 'ftp' etc).
    */
   string suffix<GHI_HANDLERS_SUFFIX_MAX_LEN>;

   /*
    * A mimetype - if available.
    */
   string mimetype<GHI_HANDLERS_MIMETYPE_MAX_LEN>;

   /*
    * A UTI (universal type identifier) - if available.
    */
   string UTI<GHI_HANDLERS_UTI_MAX_LEN>;

   /*
    * A list of the action URI and target URI's for this handlerType.
    */
   struct GHISetGuestHandlerAction actionURIs<GHI_MAX_NUM_ACTION_URI_PAIRS>;
};

/*
 * This defines the protocol for a 'setGuestHandler' messages. The union allows
 * us to create new versions of the protocol later by creating new values
 * in the GHISetGuestHandlerVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union GHISetGuestHandler switch (GHISetGuestHandlerVersion ver) {
case GHI_SET_GUEST_HANDLER_V1:
   struct GHISetGuestHandlerV1 *guestHandlerV1;
};


struct GHIRestoreDefaultGuestHandlerV1 {
   /*
    * The file suffix (including leading period character), or for protocols
    * the handler type ('http', 'ftp' etc).
    */
   string suffix<GHI_HANDLERS_SUFFIX_MAX_LEN>;

   /*
    * A mimetype - if available.
    */
   string mimetype<GHI_HANDLERS_MIMETYPE_MAX_LEN>;

   /*
    * A UTI (universal type identifier) - if available.
    */
   string UTI<GHI_HANDLERS_UTI_MAX_LEN>;
};

union GHIRestoreDefaultGuestHandler switch (GHISetGuestHandlerVersion ver) {
case GHI_SET_GUEST_HANDLER_V1:
   struct GHIRestoreDefaultGuestHandlerV1 *defaultHandlerV1;
};



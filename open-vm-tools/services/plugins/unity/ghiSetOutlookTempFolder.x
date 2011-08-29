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
 * ghiSetOutlookTempFolder.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    set or restore the temporary folder path used by Microsoft Outlook
 *    to store attachments opened by the user.
 */

#include "ghiCommonDefines.h"

/*
 * Enumerates the different versions of the messages.
 */
enum GHISetOutlookTempFolderVersion {
    GHI_SET_OUTLOOK_TEMP_FOLDER_V1 = 1
};

/*
 * The structure used for version 1 of the message.
 */
struct GHISetOutlookTempFolderV1 {
   string targetURI<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
};

/*
 * This defines the protocol for a 'setOutlookTempFolder' message.
 *
 * The union allows us to introduce new versions of the protocol later by
 * creating new values in the enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail if
 * an unknown version is provided on the wire.
 */
union GHISetOutlookTempFolder switch (GHISetOutlookTempFolderVersion ver) {
case GHI_SET_OUTLOOK_TEMP_FOLDER_V1:
   struct GHISetOutlookTempFolderV1 *setOutlookTempFolderV1;
};


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
 * ghiTrashFolder.x --
 *
 *    Definition of the data structures used in the GuestRpc commands that
 *    manipulate the Trash folder (aka Recycle Bin).
 */

/*
 * Using the opaque type causes rpcgen to emit code that calls the htonl and
 * ntohl functions. On Windows, these functions are defined in winsock2.h, so
 * we need to #include that header in the generated code.
 */
%#ifdef WIN32
%#include "winsock2.h"
%#endif /* WIN32 */

enum GHITrashFolderActionVersion {
   GHI_TRASH_FOLDER_ACTION_V1 = 1
};

enum GHITrashFolderStateVersion {
   GHI_TRASH_FOLDER_STATE_V1 = 1
};

enum GHITrashFolderGetIconVersion {
   GHI_TRASH_FOLDER_GET_ICON_V1 = 1
};

/* The maximum length of the verb to apply to the trash folder. */
const GHI_TRASH_FOLDER_ACTION_MAX_LEN = 64;

/*
 * The maximum size of the trash folder icon, 48x48x4 bytes, plus
 * 1,024 bytes for the PNG metadata.
 */
const GHI_TRASH_FOLDER_MAX_ICON_SIZE  = 10240;

struct GHITrashFolderActionV1 {
   string action<GHI_TRASH_FOLDER_ACTION_MAX_LEN>;
};

struct GHITrashFolderStateV1 {
   Bool empty;
};

struct GHITrashFolderGetIconV1 {
   int    height;
   int    width;
   opaque pngData<GHI_TRASH_FOLDER_MAX_ICON_SIZE>;
};

union GHITrashFolderAction switch (GHITrashFolderActionVersion ver) {
case GHI_TRASH_FOLDER_ACTION_V1:
   struct GHITrashFolderActionV1 *trashFolderActionV1;
};

union GHITrashFolderState switch (GHITrashFolderStateVersion ver) {
case GHI_TRASH_FOLDER_STATE_V1:
   struct GHITrashFolderStateV1 *trashFolderStateV1;
};

union GHITrashFolderGetIcon switch (GHITrashFolderGetIconVersion ver) {
case GHI_TRASH_FOLDER_GET_ICON_V1:
   struct GHITrashFolderGetIconV1 *trashFolderGetIconV1;
};

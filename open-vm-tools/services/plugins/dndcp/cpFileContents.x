/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * cpFileContents.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about file contents for cross platform clipboard.
 */

/*
 * Enumerates the different versions of the messages.
 */
enum CPFileContentsVersion {
   CP_FILE_CONTENTS_V1 = 1
};

struct CPFileItem {
   uint64 validFlags;     /* Valid members. */
   uint32 type;           /* File type */
   uint64 size;           /* File size (in bytes) */
   uint64 createTime;     /* Creation time. Ignored by POSIX */
   uint64 accessTime;     /* Time of last access */
   uint64 writeTime;      /* Time of last write */
   uint64 attrChangeTime; /* Time file attributess were last
                           * changed. Ignored by Windows */
   uint64 permissions;    /* Permissions bits */
   uint64 attributes;     /* File attributes. */
   opaque cpName<>;       /* File name in cross-platform format. */
   opaque content<>;      /* File contents. */
};

struct CPFileContentsList {
   uint64 totalFileSize;
   struct CPFileItem fileItem<>;
};

/*
 * This defines the protocol for cross-platform file contents format. The union
 * allows us to create new versions of the protocol later by creating new values
 * in the CPFileContentsVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union CPFileContents switch (CPFileContentsVersion ver) {
case CP_FILE_CONTENTS_V1:
   struct CPFileContentsList *fileContentsV1;
};


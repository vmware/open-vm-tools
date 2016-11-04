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
 * dndFileContentsUtil.h
 *
 *      Helper functions for format conversion for file contents.
 */

#ifndef _DND_FILE_CONTENTS_UTIL_H_
#define _DND_FILE_CONTENTS_UTIL_H_

#include "guestrpc/cpFileContents.h"

#define CP_FILE_VALID_NONE              0
#define CP_FILE_VALID_TYPE              (1 << 0)
#define CP_FILE_VALID_SIZE              (1 << 1)
#define CP_FILE_VALID_CREATE_TIME       (1 << 2)
#define CP_FILE_VALID_ACCESS_TIME       (1 << 3)
#define CP_FILE_VALID_WRITE_TIME        (1 << 4)
#define CP_FILE_VALID_CHANGE_TIME       (1 << 5)
#define CP_FILE_VALID_PERMS             (1 << 6)
#define CP_FILE_VALID_ATTR              (1 << 7)
#define CP_FILE_VALID_NAME              (1 << 8)
#define CP_FILE_VALID_CONTENT           (1 << 9)

#define CP_FILE_ATTR_NONE               0
#define CP_FILE_ATTR_HIDDEN             (1 << 0)
#define CP_FILE_ATTR_SYSTEM             (1 << 1)
#define CP_FILE_ATTR_ARCHIVE            (1 << 2)
#define CP_FILE_ATTR_HIDDEN_FORCED      (1 << 3)
#define CP_FILE_ATTR_READONLY           (1 << 4)

#define CP_FILE_TYPE_REGULAR            1
#define CP_FILE_TYPE_DIRECTORY          2
#define CP_FILE_TYPE_SYMLINK            3

#ifdef _WIN32

#include <shlobj.h>

Bool DnD_FileDescAToCPFileContents(FILEDESCRIPTORA *fileDescA,
                                   CPFileItem* cpItem);
Bool DnD_FileDescWToCPFileContents(FILEDESCRIPTORW *fileDescW,
                                   CPFileItem* cpItem);
Bool DnD_CPFileContentsToFileDescA(CPFileItem* cpItem,
                                   FILEDESCRIPTORA *fileDescA,
                                   Bool processName);
Bool DnD_CPFileContentsToFileDescW(CPFileItem* cpItem,
                                   FILEDESCRIPTORW *fileDescW);
#endif // _WIN32

#endif // _DND_FILE_CONTENTS_UTIL_H_

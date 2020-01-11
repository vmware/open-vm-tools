/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * dndClipboard.h
 *
 *      This file maintains an interface for the clipboard object. The object
 *      may contain several representations of the same object. For
 *      example, we could have a plain text filename as well as the file's
 *      contents on the clipboard at the same time.
 *
 *      The purpose of this structure is to store cross platform clipboard
 *      data for further processing. The UI is responsible for converting local
 *      clipboard data to a crossplatform format and inserting it into the
 *      cross platform clipboard.
 */

#ifndef _DND_CLIPBOARD_H_
#define _DND_CLIPBOARD_H_

#include "vm_basic_types.h"

#include "dnd.h"
#include "dynbuf.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Make sure each clipboard item is at most 64kb - 100 b. This limitation is
 * copied from the current text copy paste limit.
 */
#define CPCLIPITEM_MAX_SIZE_V1 ((1 << 16) - 100)
#define CPCLIPITEM_MAX_SIZE_V2 ((1 << 16) - 100)
#define CPCLIPITEM_MAX_SIZE_V3 (DNDMSG_MAX_ARGSZ - 100)

/* Cross platform formats */
typedef
#include "vmware_pack_begin.h"
struct CPFileList {
   uint64 fileSize;
   uint32 relPathsLen;
   uint32 fulPathsLen;
   uint8 filelists[1];
}
#include "vmware_pack_end.h"
CPFileList;

#define CPFILELIST_HEADER_SIZE (1* sizeof(uint64) + 2 * sizeof(uint32))

typedef
#include "vmware_pack_begin.h"
struct UriFileList {
   uint64 fileSize;
   uint32 uriPathsLen;
   uint8 filelists[1];
}
#include "vmware_pack_end.h"
UriFileList;

#define URI_FILELIST_HEADER_SIZE (1* sizeof(uint64) + 1 * sizeof(uint32))

typedef
#include "vmware_pack_begin.h"
struct CPFileAttributes {
   // File, Directory, or link. See HgfsFileType.
   uint64 fileType;
   // Read, write, execute permissions. See File_GetFilePermissions().
   uint64 filePermissions;
}
#include "vmware_pack_end.h"
CPFileAttributes;

typedef
#include "vmware_pack_begin.h"
struct CPAttributeList {
   uint32 attributesLen;
   CPFileAttributes attributeList[1];
}
#include "vmware_pack_end.h"
CPAttributeList;

#define URI_ATTRIBUTES_LIST_HEADER_SIZE (1* sizeof(uint32))

/* Types which can be stored on the clipboard. */
/*
 * XXX
 * This is currently in dnd.h, but should be moved over to dndClipboard.h
 * Ling's vmx code reorg should handle this.
 */
/*
typedef enum
{
   CPFORMAT_UNKNOWN = 0
   CPFORMAT_TEXT,
   CPFORMAT_FILELIST,
   CPFORMAT_MAX,
} DND_CPFORMAT;
*/

#define CPFORMAT_MIN CPFORMAT_TEXT

/* CPClipboard */
void CPClipboard_Init(CPClipboard *clip);
void CPClipboard_InitWithSize(CPClipboard *clip, uint32 size);
void CPClipboard_Destroy(CPClipboard *clip);

void CPClipboard_Clear(CPClipboard *clip);
Bool CPClipboard_SetItem(CPClipboard *clip, const DND_CPFORMAT fmt, const void *buf,
                         const size_t size);
void CPClipboard_SetChanged(CPClipboard *clip, Bool changed);
Bool CPClipboard_Changed(const CPClipboard *clip);
Bool CPClipboard_ClearItem(CPClipboard *clip, DND_CPFORMAT fmt);
Bool CPClipboard_GetItem(const CPClipboard *clip, DND_CPFORMAT fmt,
                         void **buf, size_t *size);
Bool CPClipboard_ItemExists(const CPClipboard *clip, DND_CPFORMAT fmt);
Bool CPClipboard_IsEmpty(const CPClipboard *clip);
#if !defined(SWIG)
size_t CPClipboard_GetTotalSize(const CPClipboard *clip);
#endif
Bool CPClipboard_Copy(CPClipboard *dest, const CPClipboard *src);
Bool CPClipboard_Serialize(const CPClipboard *clip, DynBuf *buf);
Bool CPClipboard_Unserialize(CPClipboard *clip, const void *buf, size_t len);
Bool CPClipboard_Strip(CPClipboard *clip, uint32 caps);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _DND_CLIPBOARD_H_

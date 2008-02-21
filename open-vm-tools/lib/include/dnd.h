/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * dnd.h --
 *
 *    Drag and Drop library
 *
 */

#ifndef _DND_H_
#define _DND_H_

#define INCLUDE_ALLOW_USERLEVEL

#ifdef _WIN32
#   include <windows.h>
#   include <shellapi.h>
#endif

#include "includeCheck.h"
#include "vm_basic_types.h"
#include "unicodeTypes.h"

/* Error value returned when data contains illegal characters */
#define DND_ILLEGAL_CHARACTERS  "data contains illegal characters"
/*
 * Use the same maximum path length as Hgfs.
 * XXX: Move HGFS_PATH_MAX to some header file which is more public
 *      and use it here.
 */
#define DND_MAX_PATH        6144

/* Strings used for formatting various types of data */
#define DND_URI_LIST_PRE     "file://"
#define DND_URI_LIST_PRE_KDE "file:"
#define DND_URI_LIST_POST    "\r\n"
#define DND_TEXT_PLAIN_PRE   ""
#define DND_TEXT_PLAIN_POST  ""
#define DND_STRING_PRE       ""
#define DND_STRING_POST      ""
#define FCP_GNOME_LIST_PRE   "file://"
#define FCP_GNOME_LIST_POST  "\n"

typedef enum
{
   CPFORMAT_UNKNOWN = 0,
   CPFORMAT_TEXT,
   CPFORMAT_FILELIST,
} DND_CPFORMAT;
enum DND_DROPEFFECT
{
   DROP_UNKNOWN = 1<<31,
   DROP_NONE = 0,
   DROP_COPY = 1<<0,
   DROP_MOVE = 1<<1,
   DROP_LINK = 1<<2,
};

#ifdef _WIN32
/*
 * Windows-specific functions
 */
EXTERN uint32 DnD_GetClipboardFormatFromName(LPCSTR pFormatName);
EXTERN size_t DnD_GetClipboardFormatName(UINT cf,
                                         char *pFormatName,
                                         DWORD dwBufSize);
EXTERN HGLOBAL  DnD_CopyStringToGlobal(LPSTR pszString);
EXTERN HGLOBAL DnD_CopyDWORDToGlobal(DWORD *pDWORD);
EXTERN HGLOBAL DnD_CreateHDrop(const char *path, const char *fileList);
EXTERN HGLOBAL DnD_CreateHDropForGuest(const char *path,
                                       const char *fileList);
EXTERN DWORD  DnD_CalcDirectorySize(const char *dir);
EXTERN Bool DnD_FakeMouseEvent(DWORD flag);
EXTERN Bool DnD_FakeMouseState(DWORD key, Bool isDown);
EXTERN Bool DnD_FakeEscapeKey(void);
EXTERN Bool DnD_DeleteLocalDirectory(const char *localDir);
EXTERN Bool DnD_SetClipboard(UINT format, char *buffer, int len);
EXTERN Bool DnD_GetFileList(HDROP hDrop,
                            char **remoteFiles,
                            int *remoteLength,
                            char **localFiles,
                            int *localLength,
                            uint64 *totalSize);

#else
/*
 * Posix-specific functions
 */
EXTERN char *DnD_UriListGetNextFile(char const *uriList,
                                    size_t *index,
                                    size_t *length);
#endif

/*
 * Shared functions
 */
ConstUnicode DnD_GetFileRoot(void);
char *DnD_CreateStagingDirectory(void);
Bool DnD_DeleteStagingFiles(const char *fileList, Bool onReboot);
Bool DnD_DataContainsIllegalCharacters(const char *data,
                                       const size_t dataSize);
Bool DnD_PrependFileRoot(const char *fileRoot, char **src, size_t *srcSize);
char *DnD_UTF8Asprintf(unsigned int outBufSize, const char *format, ...);
int DnD_LegacyConvertToCPName(const char *nameIn,
                              size_t bufOutSize,
                              char *bufOut);
size_t DnD_GetLastDirName(const char *str, size_t strSize, char **dirName);

/* vmblock support functions. */
int DnD_InitializeBlocking(void);
Bool DnD_UninitializeBlocking(int blockFd);
Bool DnD_AddBlock(int blockFd, const char *blockPath);
Bool DnD_RemoveBlock(int blockFd, const char *blockedPath);

#endif // _DND_H_

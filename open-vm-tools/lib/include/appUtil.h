/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * appUtil.h --
 *
 *    Utility functions for guest applications.
 */

#ifndef _APP_UTIL_H_
#define _APP_UTIL_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include "vmware.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "vmware/guestrpc/capabilities.h"

#ifdef __cplusplus
};
#endif // __cplusplus

/*
 * Platform-agnostic bitmask of types of handlers to include.
 * Used by the AppUtil file type functions.
 */
typedef enum {
   FILE_TYPE_INCLUDE_NONE = 0,
   FILE_TYPE_INCLUDE_URI = 1,                     // Include URI handlers
   FILE_TYPE_INCLUDE_PERCEIVED_HANDLERS = 1 << 1, // Include perceived type handlers
                                                  //  (see bug 1440812).
   FILE_TYPE_INCLUDE_ALL = FILE_TYPE_INCLUDE_URI |
                           FILE_TYPE_INCLUDE_PERCEIVED_HANDLERS
} FileTypeInclusions;

#ifdef _WIN32

/* The maximum number of icons that can be retrieved in a single query. */
#define APPUTIL_MAX_NUM_ICONS 16

/* Predefined (N x N pixels) icon sizes */
#define APPUTIL_ICON_SMALL 16
#define APPUTIL_ICON_BIG 32

typedef struct AppUtilIconEntry {
   uint32 width;            // width of icon in pixels
   uint32 height;           // height of icon in pixels
   uint32 widthBytes;       // width of one row in bytes, including padding
   uint32 dataLength;       // length of bgra data, in bytes
   unsigned char *dataBGRA; // pointer to bgra data
} AppUtilIconEntry;

typedef struct AppUtilIconInfo {
   uint32 numEntries;
   AppUtilIconEntry *iconList;
} AppUtilIconInfo;

typedef enum {
   APPUTIL_UPPER_LEFT_DIB = -1,    // the origin is the upper-left corner of the bitmap
   APPUTIL_LOWER_LEFT_DIB = 1,     // the origin is the lower-left corner of the bitmap
} AppUtilBitmapOrigin;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

Bool AppUtil_GetIconDataByHandle(HICON hIcon,
                                 AppUtilBitmapOrigin origin,
                                 AppUtilIconEntry *icon);

Bool AppUtil_GetDIBitsAlloc(HDC hdc,
                            HBITMAP hbmp,
                            UINT uStartScan,
                            UINT cScanLines,
                            LPBITMAPINFO lpbi,
                            UINT uUsage,
                            char **bits);

HICON AppUtil_GetWindowIcon(HWND hwnd,
                            uint32 iconSize);

void AppUtil_BuildGlobalApplicationList(FileTypeInclusions inclusions);

wchar_t* AppUtil_SanitizeCommandLine(const wchar_t *commandLineUtf16);
char *AppUtil_ActionURIForCommandLine(const WCHAR *commandLineUtf16);
Bool  AppUtil_CommandLineForShellCommandURI(const char *shellCommandURI,
                                            char **executablePath,
                                            char **commandLine);

Bool AppUtil_GetLinkIconData(const TCHAR *path,
                             AppUtilIconInfo *iconInfo,
                             AppUtilBitmapOrigin dibOrientation);

Bool AppUtil_GetAppIconData(const TCHAR *path,
                            AppUtilIconInfo *iconInfo,
                            AppUtilBitmapOrigin dibOrientation);

Bool AppUtil_LoadIcon(HMODULE module,
                      LPCWSTR resID,
                      AppUtilBitmapOrigin origin,
                      AppUtilIconInfo *icon);

Bool AppUtil_CopyIcon(const AppUtilIconInfo *srcIcon, AppUtilIconInfo *dstIcon);

void AppUtil_DestroyIcon(AppUtilIconInfo *icon);

Bool AppUtil_GetIconIndexAndLocationForShortcut(const TCHAR *shortcut,
                                                int maxLen,
                                                TCHAR *iconFile,
                                                int *iconIndex);

PISECURITY_DESCRIPTOR AppUtil_AllocateLowIntegritySD(void);

LPSTR  AppUtil_ToLowerUtf8(LPCSTR s);
LPWSTR AppUtil_ToLowerUtf16(LPCWSTR s);

Bool AppUtil_IsHorizonVDIAppRemotingMode();

#ifdef __cplusplus
};
#endif // __cplusplus

#else // not _WIN32

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void AppUtil_Init(void);
GPtrArray *AppUtil_CollectIconArray(const char *iconName,
                                    unsigned long windowID);
void AppUtil_FreeIconArray(GPtrArray *pixbufs);

Bool AppUtil_AppIsSkippable(const char *appName);
char *AppUtil_CanonicalizeAppName(const char *appName,
                                  const char *cwd);

#ifdef __cplusplus
};
#endif // __cplusplus

#endif

/*
 * Platform-independent functions.
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void AppUtil_SendGuestCaps(const GuestCapabilities *caps,
                           size_t numCaps,
                           Bool enabled);

#ifdef __cplusplus
};
#endif // __cplusplus

#endif // _APP_UTIL_H_

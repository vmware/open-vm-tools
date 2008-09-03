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
 * appUtil.h --
 *
 *    Utility functions for guest applications.
 */

#ifndef _APP_UTIL_H_
#define _APP_UTIL_H_

#include "vmware.h"
#include "guestCaps.h"


#ifdef _WIN32

typedef struct _AppUtilIconEntry {
   uint32 width;
   uint32 height;
   uint32 widthBytes;
   uint32 dataLength;
   unsigned char *dataBGRA;
} AppUtilIconEntry;

typedef struct _AppUtilIconInfo {
   uint32 numEntries;
   AppUtilIconEntry *iconList;
} AppUtilIconInfo;

typedef enum {
   APPUTIL_UPPER_LEFT_DIB = -1,    // the origin is the upper-left corner of the bitmap
   APPUTIL_LOWER_LEFT_DIB = 1,     // the origin is the lower-left corner of the bitmap
} AppUtilBitmapOrigin;

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

void AppUtil_BuildGlobalApplicationList(void);
char *AppUtil_ActionURIForCommandLine(const WCHAR *commandLineUtf16);

#endif //_WIN32

#if defined(linux)
#include <glib.h>

void AppUtil_Init(void);
GPtrArray *AppUtil_CollectIconArray(const char *iconName,
                                    unsigned long windowID);
void AppUtil_FreeIconArray(GPtrArray *pixbufs);

Bool AppUtil_AppIsSkippable(const char *appName);
char *AppUtil_CanonicalizeAppName(const char *appName,
                                  const char *cwd);
#endif

/*
 * Platform-independent functions.
 */
void AppUtil_SendGuestCaps(const GuestCapabilities *caps,
                           size_t numCaps,
                           Bool enabled);

#endif // _APP_UTIL_H_


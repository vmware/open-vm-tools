/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * ghIntegration.h --
 *
 *    Commands for guest host integration.
 */

#ifndef _GH_INTEGRATION_H_
#define _GH_INTEGRATION_H_

#if defined(__cplusplus)
#if !defined(OPEN_VM_TOOLS) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#include "appUtilFileTypes.h"
#endif // !OPEN_VM_TOOLS && !__FREEBSD__ && !sun && !__APPLE__
#include <vector>
#include <string>
#include <list>
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include "vmware.h"
#include "dynbuf.h"

typedef Bool (*GHILaunchMenuChangeRPCFn)(int numFolderKeys,
                                         const char **folderKeysChanged);
typedef Bool (*GHISendTrashFolderStateRPCFn)(Bool empty);

typedef struct GHIHostCallbacks {
   GHILaunchMenuChangeRPCFn launchMenuChange;
} GHIHostCallbacks;

Bool GHI_IsSupported(void);
void GHI_Init(GMainLoop *mainLoop, const char **envp, GHIHostCallbacks hostCallbacks);
void GHI_Cleanup(void);

Bool GHI_OpenStartMenuTree(const char *rootUtf8, uint32 flags, DynBuf *buf);
Bool GHI_GetStartMenuItem(uint32 handle, uint32 itemIndex, DynBuf *buf);
Bool GHI_CloseStartMenuTree(uint32 handle);

Bool GHI_ShellOpen(const char *fileURIUtf8);
Bool GHI_ShellAction(const char *actionURI,
                     const char *targetURI,
                     const char **locations,
                     int numLocations);

Bool GHI_SetGuestHandler(const char *suffix, const char *mimeType, const char *UTI,
                         const char *actionURI, const char *targetURI);
Bool GHI_RestoreDefaultGuestHandler(const char *suffix,
                                    const char* mimetype,
                                    const char *UTI);
Bool GHI_SetOutlookTempFolder(const char *targetURI);

Bool GHI_TrayIconSendEvent(const char *iconID,
                           uint32 event,
                           uint32 x,
                           uint32 y);
Bool GHI_TrayIconStartUpdates(void);
Bool GHI_TrayIconStopUpdates(void);

Bool GHI_SetFocusedWindow(int32 windowId);

Bool GHI_GetExecInfoHash(const char *execPath,char **execInfoHash);

#ifndef _WIN32
const char *GHIX11_FindDesktopUriByExec(const char *exec);
#endif

#ifdef __cplusplus
}; /* extern "C" */
#endif // __cplusplus

#ifdef __cplusplus
namespace vmware { namespace tools {

class NotifyIconCallback;

} /* namespace tools */ } /* namespace vmware */

void GHI_RegisterNotifyIconCallback(vmware::tools::NotifyIconCallback *notifyIconCallback);
void GHI_UnregisterNotifyIconCallback(vmware::tools::NotifyIconCallback *notifyIconCallback);

#if !defined(OPEN_VM_TOOLS) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
const FileTypeList& GHI_GetBinaryHandlers(const char *pathUtf8);
#endif // !OPEN_VM_TOOLS && !__FreeBSD__ && !sun && !__APPLE__

typedef struct GHIBinaryIconInfo {
   uint32 width;
   uint32 height;
   std::vector<uint8> dataBGRA;
} GHIBinaryIconInfo;

Bool GHI_GetBinaryInfo(const char *pathUriUtf8, std::string &friendlyName, std::list<GHIBinaryIconInfo> &iconList);
#endif // __cplusplus

#endif // _GH_INTEGRATION_H_

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
 * ghIntegrationInt.h --
 *
 *    Platform specific functionality
 */

#ifndef _GH_INTEGRATION_INT_H_
#define _GH_INTEGRATION_INT_H_

#include "ghIntegration.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "dynbuf.h"

typedef struct _GHIPlatform GHIPlatform;

/*
 * Implemented by ghIntegration[Win32|X11|Cocoa (ha!)].c
 */

Bool GHIPlatformIsSupported(void);
GHIPlatform *GHIPlatformInit(GMainLoop *mainLoop, const char **envp, GHIHostCallbacks hostcallbacks);
void GHIPlatformCleanup(GHIPlatform *ghip);
Bool GHIPlatformOpenStartMenuTree(GHIPlatform *ghip,
                                  const char *rootUtf8,
                                  uint32 flags,
                                  DynBuf *buf);
Bool GHIPlatformGetStartMenuItem(GHIPlatform *ghip,
                                 uint32 handle,
                                 uint32 itemIndex,
                                 DynBuf *buf);
Bool GHIPlatformCloseStartMenuTree(GHIPlatform *ghip,
                                   uint32 handle);
Bool GHIPlatformShellOpen(GHIPlatform *ghip,
                          const char *fileUtf8);
Bool GHIPlatformShellAction(GHIPlatform *ghip,
                            const char *actionURI,
                            const char *targetURI,
                            const char **locations,
                            int numLocations);
Bool GHIPlatformSetGuestHandler(GHIPlatform *ghip,
                                const char *suffix,
                                const char *mimeType,
                                const char *UTI,
                                const char *actionURI,
                                const char *targetURI);
Bool GHIPlatformRestoreDefaultGuestHandler(GHIPlatform *ghip,
                                           const char *suffix,
                                           const char *mimetype,
                                           const char *UTI);

/*
 * Set the temporary folder used by Outlook to store attachments.
 */
Bool GHIPlatformSetOutlookTempFolder(GHIPlatform* ghip, const char *targetURI);

/*
 * Send a mouse event to a tray icon.
 */
Bool GHIPlatformTrayIconSendEvent(GHIPlatform *ghip,
                                  const char *iconID,
                                  uint32 event,
                                  uint32 x,
                                  uint32 y);

/*
 * Start sending tray icon updates to the VMX.
 */
Bool GHIPlatformTrayIconStartUpdates(GHIPlatform *ghip);

/*
 * Stop sending tray icon updates to the VMX.
 */
Bool GHIPlatformTrayIconStopUpdates(GHIPlatform *ghip);

/*
 * Set the specified window to be focused.
 */
Bool GHIPlatformSetFocusedWindow(GHIPlatform *ghip, int32 windowId);

/*
 * Get the hash (or timestamp) of information returned by get.binary.info.
 */
Bool GHIPlatformGetExecInfoHash(GHIPlatform *ghip,
                                const char *execPath,
                                char **execInfoHash);

#ifndef _WIN32
const gchar *
GHIX11FindDesktopUriByExec(GHIPlatform *ghip,
                           const char *exec);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace vmware { namespace tools {

class NotifyIconCallback;

} /* namespace tools */ } /* namespace vmware */

void GHIPlatformRegisterNotifyIconCallback(vmware::tools::NotifyIconCallback *notifyIconCallback);
void GHIPlatformUnregisterNotifyIconCallback(vmware::tools::NotifyIconCallback *notifyIconCallback);

#if !defined(OPEN_VM_TOOLS)  && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
const FileTypeList& GHIPlatformGetBinaryHandlers(GHIPlatform *ghip, const char *pathUtf8);
#endif // !OPEN_VM_TOOLS && !__FreeBSD__ && !sun && !__APPLE__

Bool GHIPlatformGetBinaryInfo(GHIPlatform *ghip, const char *pathUriUtf8, std::string &friendlyName, std::list<GHIBinaryIconInfo> &iconList);

#endif // __cplusplus

#endif // _GH_INTEGRATION_INT_H_

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

#include "guestrpc/ghiGetExecInfoHash.h"
#include "guestrpc/ghiProtocolHandler.h"
#include "ghIntegration.h"

#include "vmware/tools/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "dynbuf.h"
#include "unityCommon.h"
#include "rpcin.h"

typedef struct _GHIPlatform GHIPlatform;

/*
 * RPC Entry points
 */
RpcInRet GHITcloGetBinaryInfo(RpcInData *data);
RpcInRet GHITcloGetBinaryHandlers(RpcInData *data);
RpcInRet GHITcloGetStartMenuItem(RpcInData *data);
RpcInRet GHITcloOpenStartMenu(RpcInData *data);
RpcInRet GHITcloCloseStartMenu(RpcInData *data);
RpcInRet GHITcloShellOpen(RpcInData *data);
RpcInRet GHITcloShellAction(RpcInData *data);

RpcInRet GHITcloSetGuestHandler(RpcInData *data);
RpcInRet GHITcloRestoreDefaultGuestHandler(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.outlook.set.tempFolder" RPC.
 */
RpcInRet GHITcloSetOutlookTempFolder(RpcInData* data);

/*
 * Wrapper function for the "ghi.guest.outlook.restore.tempFolder" RPC.
 */
RpcInRet GHITcloRestoreOutlookTempFolder(RpcInData* data);

/*
 * Wrapper function for the "ghi.guest.trashFolder.action" RPC.
 */
RpcInRet GHITcloTrashFolderAction(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.trashFolder.getIcon" RPC.
 */
RpcInRet GHITcloTrashFolderGetIcon(RpcInData *data);

RpcInRet GHIUpdateHost(GHIProtocolHandlerList *handlers);

/*
 * Wrapper function for the "ghi.guest.trayIcon.sendEvent" RPC.
 */
RpcInRet GHITcloTrayIconSendEvent(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.trayIcon.startUpdates" RPC.
 */
RpcInRet GHITcloTrayIconStartUpdates(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.trayIcon.stopUpdates" RPC.
 */
RpcInRet GHITcloTrayIconStopUpdates(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.setFocusedWindow" RPC.
 */
RpcInRet GHITcloSetFocusedWindow(RpcInData *data);

/*
 * Wrapper function for the "ghi.guest.getExecInfoHash" RPC.
 */
RpcInRet GHITcloGetExecInfoHash(RpcInData *data);

/*
 * Implemented by ghIntegration[Win32|X11|Cocoa (ha!)].c
 */

Bool GHIPlatformIsSupported(void);
GHIPlatform *GHIPlatformInit(ToolsAppCtx *ctx);
void GHIPlatformCleanup(GHIPlatform *ghip);
Bool GHIPlatformGetBinaryInfo(GHIPlatform *ghip,
                              const char *pathURIUtf8,
                              DynBuf *buf);
Bool GHIPlatformGetBinaryHandlers(GHIPlatform *ghip,
                                  const char *pathUtf8,
                                  XDR *xdrs);
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
Bool GHIPlatformSetGuestHandler(GHIPlatform *ghip, const XDR *xdrs);
Bool GHIPlatformRestoreDefaultGuestHandler(GHIPlatform *ghip, const XDR *xdrs);

void GHIPlatformRegisterCaps(GHIPlatform *ghip);
void GHIPlatformUnregisterCaps(GHIPlatform *ghip);
Bool GHIPlatformGetProtocolHandlers(GHIPlatform *ghip,
                                    GHIProtocolHandlerList *protocolHandlerList);

/*
 * Set the temporary folder used by Outlook to store attachments.
 */
Bool GHIPlatformSetOutlookTempFolder(GHIPlatform* ghip, const XDR* xdrs);

/*
 * Restore the temporary folder used by Outlook to store attachments.
 */
Bool GHIPlatformRestoreOutlookTempFolder(GHIPlatform* ghip);

/*
 * Perform an action on the Trash (aka Recycle Bin) folder, such as opening it
 * or emptying it.
 */
Bool GHIPlatformTrashFolderAction(GHIPlatform* ghip, const XDR *xdrs);

/* Get the icon for the Trash (aka Recycle Bin) folder. */
Bool GHIPlatformTrashFolderGetIcon(GHIPlatform *ghip, XDR *xdrs);

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

/* Implemented by ghIntegration.c for use by the platform-specific code */
Bool GHILaunchMenuChangeRPC(int numFolderKeys, const char **folderKeysChanged);

/*
 * Used by the platform-specific code to send the "ghi.guest.trashFolder.state"
 * RPC to the host.
 */
Bool GHISendTrashFolderStateRPC(XDR *xdrs);

/*
 * Used by the platform-specific code to send the "ghi.guest.trayIcon.update"
 * RPC to the host.
 */
Bool GHISendTrayIconUpdateRpc(XDR *xdrs);

/*
 * Set the specified window to be focused.
 */
Bool GHIPlatformSetFocusedWindow(GHIPlatform *ghip, const XDR *xdrs);

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

#endif // _GH_INTEGRATION_INT_H_

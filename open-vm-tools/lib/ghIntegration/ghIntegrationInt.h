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

#include "dynbuf.h"
#include "unityCommon.h"
#include "guestrpc/ghiProtocolHandler.h"

typedef struct _GHIPlatform GHIPlatform;

/*
 * Implemented by ghIntegration[Win32|X11|Cocoa (ha!)].c
 */

Bool GHIPlatformIsSupported(void);
GHIPlatform *GHIPlatformInit(VMU_ControllerCB *vmuControllerCB, void *ctx);
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
Bool GHIPlatformShellAction(GHIPlatform *ghip, const XDR *xdrs);
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

/* Implemented by ghIntegration.c for use by the platform-specific code */
Bool GHILaunchMenuChangeRPC(void);

/*
 * Used by the platform-specific code to send the "tools.ghi.trash.change" RPC
 * to the host.
 */
Bool GHISendTrashFolderStateRPC(XDR *xdrs);

#endif

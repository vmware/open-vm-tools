/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

/**
 * @file ghiTclo.h
 *
 *    Functions for handling/dispatching GHI TCLO RPCs.
 */

#include "vmware/tools/plugin.h"

extern "C" {
   #include "rpcin.h"
   #include "dynxdr.h"
};

extern "C" {
   void GHITcloInit();
   void GHITcloCleanup();

   /* RPC Entry points */
   gboolean GHITcloGetBinaryInfo(RpcInData *data);
   gboolean GHITcloGetBinaryHandlers(RpcInData *data);
   gboolean GHITcloGetStartMenuItem(RpcInData *data);
   gboolean GHITcloOpenStartMenu(RpcInData *data);
   gboolean GHITcloCloseStartMenu(RpcInData *data);
   gboolean GHITcloShellOpen(RpcInData *data);
   gboolean GHITcloShellAction(RpcInData *data);
   gboolean GHITcloSetGuestHandler(RpcInData *data);
   gboolean GHITcloRestoreDefaultGuestHandler(RpcInData *data);
   gboolean GHITcloSetOutlookTempFolder(RpcInData* data);
   gboolean GHITcloRestoreOutlookTempFolder(RpcInData* data);
   gboolean GHITcloTrayIconSendEvent(RpcInData *data);
   gboolean GHITcloTrayIconStartUpdates(RpcInData *data);
   gboolean GHITcloTrayIconStopUpdates(RpcInData *data);
   gboolean GHITcloSetFocusedWindow(RpcInData *data);
   gboolean GHITcloGetExecInfoHash(RpcInData *data);

   /* Callback functions to send data to the host */
   Bool GHISendTrayIconUpdateRpc(XDR *xdrs);
   Bool GHILaunchMenuChangeRPC(int numFolderKeys, const char **folderKeysChanged);
};

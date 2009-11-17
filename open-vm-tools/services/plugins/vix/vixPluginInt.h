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

#ifndef _VIXPLUGININT_H_
#define _VIXPLUGININT_H_

/**
 * @file vixPluginInt.h
 *
 * Prototypes of VIX RPC handlers found in foundryToolsDaemon.c.
 */

#define Debug        g_debug
#define Warning      g_warning

#include "vmware/tools/plugin.h"
#include "rpcin.h"

RpcInRet
ToolsDaemonTcloCheckUserAccount(RpcInData *data);

RpcInRet
FoundryToolsDaemonGetToolsProperties(RpcInData *data);

RpcInRet
ToolsDaemonHgfsImpersonated(RpcInData *data);

RpcInRet
ToolsDaemonTcloMountHGFS(RpcInData *data);

RpcInRet
ToolsDaemonTcloReceiveVixCommand(RpcInData *data);

RpcInRet
FoundryToolsDaemonRunProgram(RpcInData *data);

#if defined(linux) || defined(_WIN32)
RpcInRet
ToolsDaemonTcloSyncDriverFreeze(RpcInData *data);

RpcInRet
ToolsDaemonTcloSyncDriverThaw(RpcInData *data);
#endif

#endif /* _VIXPLUGININT_H_ */


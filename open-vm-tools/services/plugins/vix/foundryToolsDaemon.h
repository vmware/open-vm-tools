/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * foundryToolsDaemon.h --
 *
 *    Foundry related Tools functionality.
 *
 */


#ifndef __VIX_TOOLS_DAEMON_H__
#   define __VIX_TOOLS_DAEMON_H__


#if defined(VMTOOLS_USE_GLIB)

#include "vmtoolsApp.h"

void
FoundryToolsDaemon_Initialize(ToolsAppCtx *ctx);

#else /* not vix plugin */

#include "vm_basic_types.h"
#include "rpcin.h"
#include "guestApp.h"

void FoundryToolsDaemon_RegisterRoutines(RpcIn *in, 
                                         GuestApp_Dict **confDictRef, 
                                         DblLnkLst_Links *eventQueue,
                                         const char * const *originalEnvp,
                                         Bool runAsRoot);
/* There isn't an _UnregisterRoutines yet. */

Bool FoundryToolsDaemon_RegisterSetPrinter(RpcIn *in);
Bool FoundryToolsDaemon_RegisterSetPrinterCapability(void);
Bool FoundryToolsDaemon_UnregisterSetPrinter(RpcIn *in);

Bool FoundryToolsDaemon_RegisterOpenUrl(RpcIn *in);
Bool FoundryToolsDaemon_RegisterOpenUrlCapability(void);
Bool FoundryToolsDaemon_UnregisterOpenUrl(void);

#endif

#endif /* __VIX_TOOLS_DAEMON_H__ */

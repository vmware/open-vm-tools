/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @file reload.c
 *
 * Code to respond to SIGUSR2 and restart the vmtoolsd instance.
 */

#include "desktopEventsInt.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static GSource *gReloadSrc;


/*
 ******************************************************************************
 * ReloadSelf --                                                        */ /**
 *
 * Signal handler for SIGUSR2. Stops the RPC channel and reloads the vmusr
 * instance.
 *
 * @param[in] info    Unused.
 * @param[in] data    The application context.
 *
 * @return FALSE.
 *
 ******************************************************************************
 */

static gboolean
ReloadSelf(const siginfo_t *info,
           gpointer data)
{
   ToolsAppCtx *ctx = data;
   if (ctx->rpc != NULL) {
      RpcChannel_Stop(ctx->rpc);
   }
   Reload_Do();
   return FALSE;
}


/*
 ******************************************************************************
 * Reload_Do --                                                         */ /**
 *
 * Re-launch vmware-user by attempting to execute VMUSER_TITLE ('vmware-user'),
 * relying on the user's search path.
 *
 * On success, vmware-user is relaunched in our stead.  On failure, we exit with
 * EXIT_FAILURE.
 *
 ******************************************************************************
 */

void
Reload_Do(void)
{
   g_debug("Reloading the vmusr instance.");
   execlp(VMUSER_TITLE, VMUSER_TITLE, NULL);
   _exit(EXIT_FAILURE);
}


/*
 ******************************************************************************
 * Reload_Init --                                                       */ /**
 *
 * Registers a signal handler for SIGUSR2 that reloads the container.
 *
 * @param[in] ctx       Application context.
 * @param[in] pdata     Registration data.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

gboolean
Reload_Init(ToolsAppCtx *ctx,
            ToolsPluginData *pdata)
{
   gReloadSrc = VMTools_NewSignalSource(SIGUSR2);
   VMTOOLSAPP_ATTACH_SOURCE(ctx, gReloadSrc, ReloadSelf, ctx, NULL);
   return TRUE;
}


/*
 ******************************************************************************
 * Reload_Shutdown --                                                   */ /**
 *
 * Unregisters the SIGUSR2 signal handler.
 *
 * @param[in] ctx   Application context.
 * @param[in] pdata Plugin data (unused).
 *
 ******************************************************************************
 */

void
Reload_Shutdown(ToolsAppCtx *ctx,
                ToolsPluginData *pdata)
{
   g_source_destroy(gReloadSrc);
   g_source_unref(gReloadSrc);
   gReloadSrc = NULL;
}


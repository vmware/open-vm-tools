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

/**
 * @file vmrpcdbg.c
 *
 * Implementation of the library functions not related to the RPC channel.
 */

#define G_LOG_DOMAIN "rpcdbg"

#include <gmodule.h>
#include <rpc/rpc.h>
#include "util.h"
#include "vmware/tools/rpcdebug.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

static GModule *gPlugin = NULL;

/* Atomic types are not volatile in old glib versions. */
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 10
static volatile gint gRefCount = 0;
#else
static gint gRefCount = 0;
#endif


/**
 * Decreases the internal ref count of the library. When the ref count reaches
 * zero, this function will ask the application's main loop to stop running.
 *
 * @param[in]  ctx   The application contexnt.
 */

void
RpcDebug_DecRef(ToolsAppCtx *ctx)
{
   if (g_atomic_int_dec_and_test(&gRefCount)) {
      g_main_loop_quit(ctx->mainLoop);
   }
}


/**
 * Increases the internal ref count of the library. Test code that needs the
 * process to stay alive should call this function to ensure that.
 */

void
RpcDebug_IncRef(void)
{
   g_atomic_int_inc(&gRefCount);
}


/**
 * Initializes the debug library and loads the debug plugin at the given path.
 * This function panics if something goes wrong.
 *
 * @param[in]  ctx         The application context.
 * @param[in]  dbgPlugin   Path to the debug plugin.
 *
 * @return Structure containing the debug library's information.
 */

RpcDebugLibData *
RpcDebug_Initialize(ToolsAppCtx *ctx,
                    gchar *dbgPlugin)
{
   RpcDebugOnLoadFn onload;
   RpcDebugLibData *ldata;

   ldata = g_malloc(sizeof *ldata);

   g_assert(gPlugin == NULL);
   gPlugin = g_module_open(dbgPlugin, G_MODULE_BIND_LOCAL);
   if (gPlugin == NULL) {
      g_error("Can't load plugin: %s\n", dbgPlugin);
   }

   if (!g_module_symbol(gPlugin, "RpcDebugOnLoad", (gpointer *) &onload)) {
      g_error("No entry point in debug plugin %s\n", dbgPlugin);
   }

   ldata->debugPlugin = onload(ctx);
   if (ldata->debugPlugin == NULL) {
      g_error("No registration data from plugin %s\n", dbgPlugin);
   }

   ldata->newDebugChannel = RpcDebug_NewDebugChannel;
   ldata->shutdown = RpcDebug_Shutdown;

   return ldata;
}


/**
 * Places the next item on the given RPC message list into the given RPC data.
 * Updates the current index of the list.
 *
 * @param[in]  rpcdata     The injected RPC data.
 * @param[in]  list        The message list.
 *
 * @return TRUE if updated the RPC data, FALSE if reached the end of the list.
 */

gboolean
RpcDebug_SendNext(RpcDebugMsgMapping *rpcdata,
                  RpcDebugMsgList *list)
{
   if (list->mappings[list->index].message != NULL) {
      rpcdata->message = list->mappings[list->index].message;
      rpcdata->messageLen = list->mappings[list->index].messageLen;
      rpcdata->validateFn = list->mappings[list->index].validateFn;
      rpcdata->freeMsg = list->mappings[list->index].freeMsg;
      list->index++;
      return TRUE;
   }
   return FALSE;
}


/**
 * Sets @a res / @a len when responding to an RPC.
 *
 * @param[in]  str   The string to set.
 * @param[out] res   Where to store the result.
 * @param[out] len   Where to store the length.
 */

void
RpcDebug_SetResult(const char *str,
                   char **res,
                   size_t *len)
{
   if (res != NULL) {
      *res = Util_SafeStrdup(str);
   }
   if (len != NULL) {
      *len = strlen(str);
   }
}


/**
 * Shuts down the debug library. Unloads the debug plugin. The plugin's data
 * shouldn't be used after this function is called.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  ldata    Debug library data.
 */

void
RpcDebug_Shutdown(ToolsAppCtx *ctx,
                  RpcDebugLibData *ldata)
{
   g_assert(g_atomic_int_get(&gRefCount) == 0);
   g_assert(ldata != NULL);

   if (ldata->debugPlugin != NULL && ldata->debugPlugin->shutdownFn != NULL) {
      ldata->debugPlugin->shutdownFn(ctx, ldata->debugPlugin);
   }
   if (gPlugin != NULL) {
      g_module_close(gPlugin);
      gPlugin = NULL;
   }
}


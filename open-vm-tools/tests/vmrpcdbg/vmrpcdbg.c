/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
#include "CUnit/Basic.h"
#include <CUnit/CUnit.h>

#include "util.h"
#include "vmrpcdbgInt.h"

#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);

static GModule *gPlugin = NULL;

#if defined(_WIN64) && (_MSC_VER == 1500) && GLIB_CHECK_VERSION(2, 46, 0)
/*
 * Turn off optimizer for this compiler, since something with new glib makes it
 * go into an infinite loop, only on 64bit and only with beta.
 */
#pragma optimize("", off)
#endif


/*
 * Static variables to hold the app's main loop data. CUnit test functions
 * don't take any parameters so there's no other way to do this...
 */
static struct {
   void (*mainLoop)(gpointer);
   gpointer          loopData;
   RpcDebugLibData  *libData;
   ToolsAppCtx      *ctx;
   gint              refCount;
} gLibRunData;


/*
 ******************************************************************************
 * RpcDebugRunLoop --                                                   */ /**
 *
 * Runs the app's main loop as part of a CUnit test.
 *
 ******************************************************************************
 */

static void
RpcDebugRunLoop(void)
{
   ASSERT(gLibRunData.libData);
   ASSERT(gLibRunData.mainLoop);
   ASSERT(gLibRunData.loopData);
   gLibRunData.mainLoop(gLibRunData.loopData);

   if (gLibRunData.libData->debugPlugin->shutdownFn != NULL) {
      gLibRunData.libData->debugPlugin->shutdownFn(gLibRunData.ctx,
                                                   gLibRunData.libData->debugPlugin);
   }
}


/*
 ******************************************************************************
 * RpcDebugRun --                                                       */ /**
 *
 * Runs the main application's main loop function through CUnit so that we
 * get all the test tracking / reporting goodness that it provides.
 *
 * @param[in] runMainLoop     A function that runs the application's main loop.
 *                            The function should take one argument,
 * @param[in] runData         Argument to be passed to the main loop function.
 * @param[in] ldata           Debug library data.
 *
 * @return CUnit test run result (cast to int).
 *
 ******************************************************************************
 */

static int
RpcDebugRun(ToolsAppCtx *ctx,
            gpointer runMainLoop,
            gpointer runData,
            RpcDebugLibData *ldata)
{
   CU_ErrorCode err;
   CU_Suite *suite;
   CU_Test *test;

   ASSERT(runMainLoop != NULL);
   ASSERT(ldata != NULL);

   err = CU_initialize_registry();
   ASSERT(err == CUE_SUCCESS);

   suite = CU_add_suite(g_module_name(gPlugin), NULL, NULL);
   ASSERT(suite != NULL);

   test = CU_add_test(suite, g_module_name(gPlugin), RpcDebugRunLoop);
   ASSERT_NOT_IMPLEMENTED(test != NULL);

   gLibRunData.ctx = ctx;
   gLibRunData.libData = ldata;
   gLibRunData.mainLoop = runMainLoop;
   gLibRunData.loopData = runData;

   err = CU_basic_run_tests();

   /* Clean up internal library / debug plugin state. */
   ASSERT(g_atomic_int_get(&gLibRunData.refCount) >= 0);

   if (gPlugin != NULL) {
      g_module_close(gPlugin);
      gPlugin = NULL;
   }

   if (CU_get_failure_list() != NULL) {
      err = 1;
   }

   CU_cleanup_registry();
   memset(&gLibRunData, 0, sizeof gLibRunData);
   return (int) err;
}


/**
 * Decreases the internal ref count of the library. When the ref count reaches
 * zero, this function will ask the application's main loop to stop running.
 *
 * @param[in]  ctx   The application contexnt.
 */

void
RpcDebug_DecRef(ToolsAppCtx *ctx)
{
   if (g_atomic_int_dec_and_test(&gLibRunData.refCount)) {
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
   g_atomic_int_inc(&gLibRunData.refCount);
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

   ASSERT(gPlugin == NULL);

   ldata = g_malloc(sizeof *ldata);

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
   ldata->run = RpcDebugRun;

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

#if defined(_WIN64) && (_MSC_VER == 1500) && GLIB_CHECK_VERSION(2, 46, 0)
/*
 * Restore optimizer.
 */
#pragma optimize("", on)
#endif


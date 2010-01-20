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
 * @file mainLoop.c
 *
 *    Functions for running the tools service's main loop.
 */

#if defined(_WIN32)
#  define MODULE_NAME(x)   #x "." G_MODULE_SUFFIX
#else
#  define MODULE_NAME(x)   "lib" #x "." G_MODULE_SUFFIX
#endif

#include <stdlib.h>
#include "toolsCoreInt.h"
#include "conf.h"
#include "guestApp.h"
#include "serviceObj.h"
#include "system.h"
#include "util.h"
#include "vmcheck.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/utils.h"


/*
 ******************************************************************************
 * ToolsCoreCleanup --                                                  */ /**
 *
 * Cleans up the main loop after it has executed. After this function
 * returns, the fields of the state object shouldn't be used anymore.
 *
 * @param[in]  state       Service state.
 *
 ******************************************************************************
 */

static void
ToolsCoreCleanup(ToolsServiceState *state)
{
   ToolsCore_UnloadPlugins(state);
   if (state->ctx.rpc != NULL) {
      RpcChannel_Destroy(state->ctx.rpc);
      state->ctx.rpc = NULL;
   }
   g_key_file_free(state->ctx.config);
   g_main_loop_unref(state->ctx.mainLoop);

#if defined(G_PLATFORM_WIN32)
   if (state->ctx.comInitialized) {
      CoUninitialize();
      state->ctx.comInitialized = FALSE;
   }
#endif

#if !defined(_WIN32)
   if (state->ctx.envp) {
      System_FreeNativeEnviron(state->ctx.envp);
      state->ctx.envp = NULL;
   }
#endif

   g_object_unref(state->ctx.serviceObj);
   state->ctx.serviceObj = NULL;
   state->ctx.config = NULL;
   state->ctx.mainLoop = NULL;
}


/**
 * Loads the debug library and calls its initialization function. This function
 * panics is something goes wrong.
 *
 * @param[in]  state    Service state.
 */

static void
ToolsCoreInitializeDebug(ToolsServiceState *state)
{
   RpcDebugLibData *libdata;
   RpcDebugInitializeFn initFn;

   state->debugLib = g_module_open(MODULE_NAME(vmrpcdbg), G_MODULE_BIND_LOCAL);
   if (state->debugLib == NULL) {
      g_error("Cannot load vmrpcdbg library.\n");
   }

   if (!g_module_symbol(state->debugLib,
                        "RpcDebug_Initialize",
                        (gpointer *) &initFn)) {
      g_error("Cannot find symbol: RpcDebug_Initialize\n");
   }

   libdata = initFn(&state->ctx, state->debugPlugin);
   ASSERT(libdata != NULL);
   ASSERT(libdata->debugPlugin != NULL);

   state->debugData = libdata;
}


/**
 * Timer callback that just calls ToolsCore_ReloadConfig().
 *
 * @param[in]  clientData  Service state.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreConfFileCb(gpointer clientData)
{
   ToolsCore_ReloadConfig(clientData, FALSE);
   return TRUE;
}


/*
 ******************************************************************************
 * ToolsCoreRunLoop --                                                  */ /**
 *
 * Loads and registers all plugins, and runs the service's main loop.
 *
 * @param[in]  state       Service state.
 *
 * @return Exit code.
 *
 ******************************************************************************
 */

static int
ToolsCoreRunLoop(ToolsServiceState *state)
{
   if (ToolsCore_GetTcloName(state) != NULL && !ToolsCore_InitRpc(state)) {
      return 1;
   }

   /*
    * Start the RPC channel if it's been created. The channel may be NULL if this is
    * not running in the context of a VM.
    */
   if (state->ctx.rpc && !RpcChannel_Start(state->ctx.rpc)) {
      return 1;
   }

   if (!ToolsCore_LoadPlugins(state)) {
      return 1;
   }

   /*
    * If not in a VM then there's no point in trying to run the loop, just exit
    * with a '0' return status (see bug 297528 for why '0'). If we ever want
    * to run vmtoolsd on physical hardware (or another hypervisor), we'll have
    * to revisit this code.
    */
   if (!state->ctx.isVMware) {
      return 0;
   }

   ToolsCore_RegisterPlugins(state);
   g_timeout_add(CONF_POLL_TIME * 10, ToolsCoreConfFileCb, state);

#if defined(__APPLE__)
   ToolsCore_CFRunLoop(state);
#else
   g_main_loop_run(state->ctx.mainLoop);
#endif

   ToolsCoreCleanup(state);
   return state->ctx.errorCode;
}


/**
 * Logs some information about the runtime state of the service: loaded
 * plugins, registered GuestRPC callbacks, etc. Also fires a signal so
 * that plugins can log their state if they want to.
 *
 * @param[in]  state    The service state.
 */

void
ToolsCore_DumpState(ToolsServiceState *state)
{
   guint i;
   const char *providerStates[] = {
      "idle",
      "active",
      "error"
   };

   ASSERT_ON_COMPILE(ARRAYSIZE(providerStates) == TOOLS_PROVIDER_MAX);

   g_message("VM Tools Service '%s':\n", state->name);
   g_message("   Plugin path: %s\n", state->pluginPath);

   for (i = 0; i < state->providers->len; i++) {
      ToolsAppProviderReg *prov = &g_array_index(state->providers,
                                                 ToolsAppProviderReg,
                                                 i);
      g_message("   App provider: %s (%s)\n",
                prov->prov->name,
                providerStates[prov->state]);
      if (prov->prov->dumpState != NULL) {
         prov->prov->dumpState(&state->ctx, prov->prov, NULL);
      }
   }

   ToolsCore_DumpPluginInfo(state);

   g_signal_emit_by_name(state->ctx.serviceObj,
                         TOOLS_CORE_SIG_DUMP_STATE,
                         &state->ctx);
}


/**
 * Returns the name of the TCLO app name. This will only return non-NULL
 * if the service is either the tools "guestd" or "userd" service.
 *
 * @param[in]  state    The service state.
 *
 * @return The app name, or NULL if not running a known TCLO app.
 */

const char *
ToolsCore_GetTcloName(ToolsServiceState *state)
{
   if (state->mainService) {
      return TOOLS_DAEMON_NAME;
   } else if (strcmp(state->name, VMTOOLS_USER_SERVICE) == 0) {
      return TOOLS_DND_NAME;
   } else {
      return NULL;
   }
}


/**
 * Reloads the config file and re-configure the logging subsystem if the
 * log file was updated. If the config file is being loaded for the first
 * time, try to upgrade it to the new version if an old version is
 * detected.
 *
 * @param[in]  state       Service state.
 * @param[in]  reset       Whether to reset the logging subsystem.
 */

void
ToolsCore_ReloadConfig(ToolsServiceState *state,
                       gboolean reset)
{
   gboolean first = state->ctx.config == NULL;
   gboolean loaded;

   loaded = VMTools_LoadConfig(state->configFile,
                               G_KEY_FILE_NONE,
                               &state->ctx.config,
                               &state->configMtime);

   if (!first && loaded) {
      g_debug("Config file reloaded.\n");

      /* Inform plugins of config file update. */
      g_signal_emit_by_name(state->ctx.serviceObj,
                            TOOLS_CORE_SIG_CONF_RELOAD,
                            &state->ctx);
   }

   if (state->ctx.config == NULL) {
      /* Couldn't load the config file. Just create an empty dictionary. */
      state->ctx.config = g_key_file_new();
   }

   if (reset || loaded) {
      VMTools_ConfigLogging(state->name,
                            state->ctx.config,
                            state->log,
                            reset);
   }
}


/**
 * Performs any initial setup steps for the service's main loop.
 *
 * @param[in]  state       Service state.
 */

void
ToolsCore_Setup(ToolsServiceState *state)
{
   GMainContext *gctx;

   if (!g_thread_supported()) {
      g_thread_init(NULL);
   }

   ToolsCore_ReloadConfig(state, FALSE);

   /* Initializes the app context. */
   gctx = g_main_context_default();
   state->ctx.version = TOOLS_CORE_API_V1;
   state->ctx.name = state->name;
   state->ctx.errorCode = EXIT_SUCCESS;
   state->ctx.mainLoop = g_main_loop_new(gctx, TRUE);
   state->ctx.isVMware = VmCheck_IsVirtualWorld();

   g_type_init();
   state->ctx.serviceObj = g_object_new(TOOLSCORE_TYPE_SERVICE, NULL);

   /* Initializes the debug library if needed. */
   if (state->debugPlugin != NULL) {
      ToolsCoreInitializeDebug(state);
   }
}


/**
 * Runs the service's main loop.
 *
 * @param[in]  state       Service state.
 *
 * @return Exit code.
 */

int
ToolsCore_Run(ToolsServiceState *state)
{
   if (state->debugData != NULL) {
      int ret = state->debugData->run(&state->ctx,
                                      ToolsCoreRunLoop,
                                      state,
                                      state->debugData);
      g_module_close(state->debugLib);
      state->debugData = NULL;
      state->debugLib = NULL;
      return ret;
   }
   return ToolsCoreRunLoop(state);
}


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
#  define MODULE_NAME(x)   #x ".dll"
#elif defined(__APPLE__)
#  define MODULE_NAME(x)   "lib" #x ".dylib"
#else
#  define MODULE_NAME(x)   "lib" #x ".so"
#endif

#include <stdlib.h>
#include "toolsCoreInt.h"
#include "conf.h"
#include "guestApp.h"
#include "serviceObj.h"
#include "util.h"
#include "vm_app.h"
#include "vmcheck.h"
#include "vmtools.h"


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
 * Reloads the config file and re-configure the logging subsystem if the
 * log file was updated. If the config file is being loaded for the first
 * time, try to upgrade it to the new version if an old version is
 * detected.
 *
 * @param[in]  clientData  Service state.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreReloadConfig(gpointer clientData)
{
   ToolsServiceState *state = clientData;
   char *confFile = state->configFile;
   gboolean loaded = TRUE;

   if (confFile == NULL) {
      confFile = VMTools_GetToolsConfFile();
   }

   if (state->ctx.config == NULL) {
      state->ctx.config = VMTools_LoadConfig(confFile,
                                             G_KEY_FILE_NONE,
                                             state->mainService);
      state->configMtime = time(NULL);
      if (state->ctx.config == NULL) {
         /* Couldn't load the config file. Just create an empty dictionary. */
         state->ctx.config = g_key_file_new();
      }
   } else if (VMTools_ReloadConfig(confFile,
                                   G_KEY_FILE_NONE,
                                   &state->ctx.config,
                                   &state->configMtime)) {
      g_debug("Config file reloaded.\n");
   } else {
      loaded = FALSE;
   }

   if (loaded) {
      VMTools_ConfigLogging(state->ctx.config);
      if (state->log) {
         VMTools_EnableLogging(state->log);
      }
   }

   if (state->configFile == NULL) {
      g_free(confFile);
   }
   return TRUE;
}


/**
 * Cleans up the main loop after it has executed. After this function
 * returns, the fields of the state object shouldn't be used anymore.
 *
 * @param[in]  state       Service state.
 */

void
ToolsCore_Cleanup(ToolsServiceState *state)
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

   if (state->debugData != NULL) {
      state->debugData->shutdown(&state->ctx, state->debugData);
      g_module_close(state->debugLib);
      state->debugData = NULL;
      state->debugLib = NULL;
   }

   g_object_unref(state->ctx.serviceObj);
   state->ctx.serviceObj = NULL;
   state->ctx.config = NULL;
   state->ctx.mainLoop = NULL;
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
 * Performs any initial setup steps for the service's main loop.
 *
 * @param[in]  state       Service state.
 *
 * @return Whether initialization was successful.
 */

gboolean
ToolsCore_Setup(ToolsServiceState *state)
{
   GMainContext *gctx;

   if (!g_thread_supported()) {
      g_thread_init(NULL);
   }

   VMTools_SetDefaultLogDomain(state->name);
   ToolsCoreReloadConfig(state);

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

   /* Initialize the RpcIn channel for the known tools services. */
   if (ToolsCore_GetTcloName(state) != NULL &&
       !ToolsCore_InitRpc(state)) {
      goto error;
   }

   if (!state->ctx.rpc->start(state->ctx.rpc)) {
      goto error;
   }

   if (!ToolsCore_LoadPlugins(state)) {
      goto error;
   }

   ToolsCore_RegisterPlugins(state);
   goto exit;

error:
   if (state->ctx.rpc != NULL) {
      state->ctx.rpc->shutdown(state->ctx.rpc);
      state->ctx.rpc = NULL;
   }
   if (state->ctx.mainLoop != NULL) {
      g_main_loop_unref(state->ctx.mainLoop);
      state->ctx.mainLoop = NULL;
   }

exit:
   return (state->ctx.mainLoop != NULL);
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
   g_timeout_add(CONF_POLL_TIME * 10, ToolsCoreReloadConfig, state);
#if defined(__APPLE__)
   ToolsCore_CFRunLoop(state);
#else
   g_main_loop_run(state->ctx.mainLoop);
#endif
   return state->ctx.errorCode;
}


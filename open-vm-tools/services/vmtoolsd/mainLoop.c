/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#include "toolsHangDetector.h"
#include "str.h"
#include "system.h"
#include "util.h"
#include "vmcheck.h"
#include "vm_tools_version.h"
#include "vm_version.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"


/*
 * Establish the default and maximum vmusr RPC channel error limits
 * that will be used to detect that the single allowed toolbox-dnd channel
 * is not available.
 */

/*
 * Lowest number of RPC channel errors to reasonably indicate that the
 * single allowed toolbox-dnd channel is currently in use by another
 * process.
 */
#define VMUSR_CHANNEL_ERR_MIN 3        /* approximately 3 secs. */

/*
 * The default number of vmusr channel errors before quitting the vmusr
 * process start-up.
 */
#define VMUSR_CHANNEL_ERR_DEFAULT 5    /* approximately 5  secs. */

/*
 * Arbitrary upper vmusr channel error count limit.
 */
#define VMUSR_CHANNEL_ERR_MAX 15       /* approximately 15 secs. */

#define CONFNAME_MAX_CHANNEL_ATTEMPTS "maxChannelAttempts"


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
   ToolsCorePool_Shutdown(&state->ctx);
   ToolsCore_UnloadPlugins(state);
#if defined(__linux__)
   if (state->mainService) {
      ToolsCore_ReleaseVsockFamily(state);
   }
#endif
   if (state->ctx.rpc != NULL) {
      RpcChannel_Stop(state->ctx.rpc);
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

   g_object_set(state->ctx.serviceObj, TOOLS_CORE_PROP_CTX, NULL, NULL);
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
#if defined(_WIN32)
   VMTools_AttachConsole();
#endif
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


/**
 * IO freeze signal handler. Disables the conf file check task if I/O is
 * frozen, re-enable it otherwise. See bug 529653.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  freeze   Whether I/O is being frozen.
 * @param[in]  state    Service state.
 */

static void
ToolsCoreIOFreezeCb(gpointer src,
                    ToolsAppCtx *ctx,
                    gboolean freeze,
                    ToolsServiceState *state)
{
   if (state->configCheckTask > 0 && freeze) {
      g_source_remove(state->configCheckTask);
      state->configCheckTask = 0;
      VMTools_SuspendLogIO();
   } else if (state->configCheckTask == 0 && !freeze) {
      VMTools_ResumeLogIO();
      state->configCheckTask = g_timeout_add(CONF_POLL_TIME * 1000,
                                             ToolsCoreConfFileCb,
                                             state);
   }
}


/*
 ******************************************************************************
 * ToolsCoreReportVersionData --                                         */ /**
 *
 * Report version info as guest variables.
 *
 * @param[in]  state       Service state.
 *
 ******************************************************************************
 */

static void
ToolsCoreReportVersionData(ToolsServiceState *state)
{
   char *value;
   const static char cmdPrefix[] = "info-set guestinfo.vmtools.";

   /*
    * These values are documented with specific formats.  Do not change
    * the formats, as client code can depend on them.
    */


   /*
    * Version description as a human-readable string.  This value should
    * not be parsed, so its format can be modified if necessary.
    */
   value = g_strdup_printf("%sdescription "
#ifdef OPEN_VM_TOOLS
                           "open-vm-tools %s build %s",
#else
                           "VMware Tools %s build %s",
#endif
                           cmdPrefix,
                           TOOLS_VERSION_CURRENT_STR,
                           BUILD_NUMBER_NUMERIC_STRING);
   if (!RpcChannel_Send(state->ctx.rpc, value,
                        strlen(value) + 1, NULL, NULL)) {
      g_warning("%s: failed to send description", __FUNCTION__);
   }
   g_free(value);

   /*
    * Version number as a code-readable string.  This value can
    * be parsed, so its format should not be modified.
    */
   value = g_strdup_printf("%sversionString "
                           "%s", cmdPrefix, TOOLS_VERSION_CURRENT_STR);
   if (!RpcChannel_Send(state->ctx.rpc, value,
                        strlen(value) + 1, NULL, NULL)) {
      g_warning("%s: failed to send versionString", __FUNCTION__);
   }
   g_free(value);

   /*
    * Version number as a code-readable integer.  This value can
    * be parsed, so its format should not be modified.
    */
   value = g_strdup_printf("%sversionNumber "
                           "%d", cmdPrefix, TOOLS_VERSION_CURRENT);
   if (!RpcChannel_Send(state->ctx.rpc, value,
                         strlen(value) + 1, NULL, NULL)) {
      g_warning("%s: failed to send versionNumber", __FUNCTION__);
   }
   g_free(value);

   /*
    * Build number as a code-readable integer.  This value can
    * be parsed, so its format should not be modified.
    */
   value = g_strdup_printf("%sbuildNumber "
                           "%d", cmdPrefix, BUILD_NUMBER_NUMERIC);
   if (!RpcChannel_Send(state->ctx.rpc, value,
                        strlen(value) + 1, NULL, NULL)) {
      g_warning("%s: failed to send buildNumber", __FUNCTION__);
   }
   g_free(value);
}


/*
 ******************************************************************************
 * ToolsCoreSetOptionSignalCb --
 *
 *  The SET_OPTION signal callback. The signal was triggered from
 *  ToolsCoreRpcSetOption. This function is needed to safely run code
 *  outside of the TCLO RPC handler.
 *
 *  Check for TOOLSOPTION_GUEST_LOG_LEVEL,
 *  and reinitialize the Vmx Guest Logger.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The ToolsAppCtx for passing config.
 * @param[in]  option   The option key.
 * @param[in]  value    The option value.
 * @param[in]  data     Unused.
 *
 * Result:
 *      TRUE on success.
 *
 * Side-effects:
 *      None
 *
 ******************************************************************************
 */

static gboolean
ToolsCoreSetOptionSignalCb(gpointer src,               // IN
                           ToolsAppCtx *ctx,           // IN
                           const gchar *option,        // IN
                           const gchar *value,         // IN
                           gpointer data)              // IN
{
   if (strcmp(option, TOOLSOPTION_GUEST_LOG_LEVEL) == 0) {
      ASSERT(value); /* Caller ensures */
      g_info("Received the tools set option for the guest log level '%s'.\n",
             value);

      /* Reuse the existing RPC channel */
      VMTools_SetupVmxGuestLog(FALSE, ctx->config, value);
   }

   return TRUE;
}


/*
 ******************************************************************************
 *
 * ToolsCoreResetSignalCb --
 *  The RESET signal callback. The signal was triggered from
 *  ToolsCoreCheckReset. This function is needed to safely run code
 *  outside of the RPC Channel reset code.
 *
 *  Reinitialize the Vmx Guest Logger.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The ToolsAppCtx for passing the config.
 * @param[in]  data     Unused.
 *
 ******************************************************************************
 */

static void
ToolsCoreResetSignalCb(gpointer src,          // IN
                       ToolsAppCtx *ctx,      // IN
                       gpointer data)         // IN
{
   g_info("Reinitialize the Vmx Guest Logger with a new RPC channel.\n");
   VMTools_SetupVmxGuestLog(TRUE, ctx->config, NULL); /* New RPC channel */
   g_info("Clear out the tools hang detector RPC cache state\n");
   ToolsCoreHangDetector_RpcReset();
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
   if (!ToolsCore_InitRpc(state)) {
      return 1;
   }

   /*
    * Start the RPC channel if it's been created. The channel may be NULL if this is
    * not running in the context of a VM.
    */
   if (state->ctx.rpc && !RpcChannel_Start(state->ctx.rpc)) {
      return 1;
   }

   /* Report version info as guest Vars */
   if (state->ctx.rpc) {
      ToolsCoreReportVersionData(state);
   }

   if (!ToolsCore_LoadPlugins(state)) {
      return 1;
   }

#if defined(__linux__)
   /*
    * Init a reference to vSocket family in the main service.
    */
   if (state->mainService) {
      ToolsCore_InitVsockFamily(state);
   }
#endif

   /*
    * The following criteria needs to hold for the main loop to be run:
    *
    * . no plugin has requested the service to shut down during initialization.
    * . we're either on a VMware hypervisor, or running an unknown service name.
    * . we're running in debug mode.
    *
    * In the non-VMware hypervisor case, just exit with a '0' return status (see
    * bug 297528 for why '0').
    */
   if (state->ctx.errorCode == 0 &&
       (state->ctx.isVMware ||
        ToolsCore_GetTcloName(state) == NULL ||
        state->debugPlugin != NULL)) {
      ToolsCore_RegisterPlugins(state);

      /*
       * Listen for the I/O freeze signal. We have to disable the config file
       * check when I/O is frozen or the (Win32) sync driver may cause the service
       * to hang (and make the VM unusable until it times out).
       */
      if (g_signal_lookup(TOOLS_CORE_SIG_IO_FREEZE,
                          G_OBJECT_TYPE(state->ctx.serviceObj)) != 0) {
         g_signal_connect(state->ctx.serviceObj,
                          TOOLS_CORE_SIG_IO_FREEZE,
                          G_CALLBACK(ToolsCoreIOFreezeCb),
                          state);
      }

      if (g_signal_lookup(TOOLS_CORE_SIG_SET_OPTION,
                          G_OBJECT_TYPE(state->ctx.serviceObj)) != 0) {
         g_signal_connect(state->ctx.serviceObj,
                          TOOLS_CORE_SIG_SET_OPTION,
                          G_CALLBACK(ToolsCoreSetOptionSignalCb),
                          NULL);
      }

      if (g_signal_lookup(TOOLS_CORE_SIG_RESET,
                          G_OBJECT_TYPE(state->ctx.serviceObj)) != 0) {
         g_signal_connect(state->ctx.serviceObj,
                          TOOLS_CORE_SIG_RESET,
                          G_CALLBACK(ToolsCoreResetSignalCb),
                          NULL);
      }

      state->configCheckTask = g_timeout_add(CONF_POLL_TIME * 1000,
                                             ToolsCoreConfFileCb,
                                             state);

#if defined(__APPLE__)
      ToolsCore_CFRunLoop(state);
#else
      /*
       * For now exclude the MAC due to limited testing.
       */
      if (state->mainService && ToolsCoreHangDetector_Start(&state->ctx)) {
         g_info("Successfully started tools hang detector");
      }
      g_main_loop_run(state->ctx.mainLoop);
#endif
   }

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

   if (!g_main_loop_is_running(state->ctx.mainLoop)) {
      ToolsCore_LogState(TOOLS_STATE_LOG_ROOT,
                         "VM Tools Service '%s': not running.\n",
                         state->name);
      return;
   }

   ToolsCore_LogState(TOOLS_STATE_LOG_ROOT,
                      "VM Tools Service '%s':\n",
                      state->name);
   ToolsCore_LogState(TOOLS_STATE_LOG_CONTAINER,
                      "Plugin path: %s\n",
                      state->pluginPath);

   for (i = 0; i < state->providers->len; i++) {
      ToolsAppProviderReg *prov = &g_array_index(state->providers,
                                                 ToolsAppProviderReg,
                                                 i);
      ToolsCore_LogState(TOOLS_STATE_LOG_CONTAINER,
                         "App provider: %s (%s)\n",
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
 * Return the RpcChannel failure threshold for the tools user service.
 *
 * @param[in]      state       The service state.
 *
 * @return  The RpcChannel failure limit for the user tools service.
 */

guint
ToolsCore_GetVmusrLimit(ToolsServiceState *state)      // IN
{
   gint errorLimit = 0;      /* Special value 0 means no error threshold. */

   if (TOOLS_IS_USER_SERVICE(state)) {
      errorLimit = VMTools_ConfigGetInteger(state->ctx.config,
                                            state->name,
                                            CONFNAME_MAX_CHANNEL_ATTEMPTS,
                                            VMUSR_CHANNEL_ERR_DEFAULT);

      /*
       * A zero value is allowed and will disable the single vmusr
       * process restriction.
       */
      if (errorLimit != 0 &&
          (errorLimit < VMUSR_CHANNEL_ERR_MIN ||
           errorLimit > VMUSR_CHANNEL_ERR_MAX)) {
         g_warning("%s: Invalid %s: %s (%d) specified in tools configuration; "
                   "using default value (%d)\n", __FUNCTION__,
                   state->name, CONFNAME_MAX_CHANNEL_ATTEMPTS,
                   errorLimit, VMUSR_CHANNEL_ERR_DEFAULT);
         errorLimit = VMUSR_CHANNEL_ERR_DEFAULT;
      }
   }

   return errorLimit;
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
   } else if (TOOLS_IS_USER_SERVICE(state)) {
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

      /*
       * Inform plugins of config file update.
       */
      ASSERT(state->ctx.serviceObj != NULL);
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
                            TRUE,
                            reset);

      /*
       * Reinitialize the level setting of the VMX Guest Logger
       * since either the config is changing or we are forcing a reload
       * of the tools logging subsystem.
       * However, reuse the RPC channel since it is not affected.
       */
      VMTools_SetupVmxGuestLog(FALSE, state->ctx.config, NULL);
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
   ToolsServiceProperty ctxProp = { TOOLS_CORE_PROP_CTX };

   /*
    * Useful for debugging purposes. Log the vesion and build information.
    */
   g_message("Tools Version: %s (%s)\n", TOOLS_VERSION_EXT_CURRENT_STR, BUILD_NUMBER);

   /* Initializes the app context. */
   gctx = g_main_context_default();
   state->ctx.version = TOOLS_CORE_API_V1;
   state->ctx.name = state->name;
   state->ctx.errorCode = EXIT_SUCCESS;
#if defined(__APPLE__)
   /*
    * Mac OS doesn't use g_main_loop_run(), so need to create the loop as
    * "running".
    */
   state->ctx.mainLoop = g_main_loop_new(gctx, TRUE);
#else
   state->ctx.mainLoop = g_main_loop_new(gctx, FALSE);
#endif
   /*
    * Valgrind can't handle the backdoor check.
    */
#ifdef USE_VALGRIND
   state->ctx.isVMware = TRUE;
#else
   state->ctx.isVMware = VmCheck_IsVirtualWorld();
#endif
   g_main_context_unref(gctx);

   g_type_init();
   state->ctx.serviceObj = g_object_new(TOOLSCORE_TYPE_SERVICE, NULL);

   /* Register the core properties. */
   ToolsCoreService_RegisterProperty(state->ctx.serviceObj,
                                     &ctxProp);
   g_object_set(state->ctx.serviceObj, TOOLS_CORE_PROP_CTX, &state->ctx, NULL);
   ToolsCorePool_Init(&state->ctx);

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


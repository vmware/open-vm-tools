/*********************************************************
 * Copyright (C) 2008-2020 VMware, Inc. All rights reserved.
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
 * @file pluginMgr.c
 *
 *    Provides functions for loading and manipulating Tools plugins.
 */

#include <string.h>
#include "toolsCoreInt.h"

#include "vm_assert.h"
#include "guestApp.h"
#include "serviceObj.h"
#include "util.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"


/** Defines the internal data about a plugin. */
typedef struct ToolsPlugin {
   gchar               *fileName;
   GModule             *module;
   ToolsPluginOnLoad    onload;
   ToolsPluginData     *data;
} ToolsPlugin;


#ifdef USE_APPLOADER
static Bool (*LoadDependencies)(char *libName, Bool useShipped);
#endif

typedef void (*PluginDataCallback)(ToolsServiceState *state,
                                   ToolsPluginData *plugin);

typedef gboolean (*PluginAppRegCallback)(ToolsServiceState *state,
                                         ToolsPluginData *plugin,
                                         ToolsAppType type,
                                         ToolsAppProviderReg *preg,
                                         gpointer reg);


/**
 * State dump callback for application registration information.
 *
 * @param[in]  state The service state.
 * @param[in]  plugin   The plugin information.
 * @param[in]  type     Application type.
 * @param[in]  preg     Provider information.
 * @param[in]  reg      Application registration.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreDumpAppInfo(ToolsServiceState *state,
                     ToolsPluginData *plugin,
                     ToolsAppType type,
                     ToolsAppProviderReg *preg,
                     gpointer reg)
{
   if (preg != NULL) {
      if (preg->prov->dumpState != NULL) {
         preg->prov->dumpState(&state->ctx, preg->prov, reg);
      } else {
         ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN,
                            "App type %u (no provider info).\n",
                            type);
      }
    } else {
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN,
                         "App type %u (no provider).\n",
                         type);
   }
   return TRUE;
}


/**
 * State dump callback for generic plugin information.
 *
 * @param[in]  state    The service state.
 * @param[in]  plugin   The plugin information.
 */

static void
ToolsCoreDumpPluginInfo(ToolsServiceState *state,
                        ToolsPluginData *plugin)
{
   ToolsCore_LogState(TOOLS_STATE_LOG_CONTAINER, "Plugin: %s\n", plugin->name);

   if (plugin->regs == NULL) {
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN, "No registrations.\n");
   }
}


/**
 * State dump callback for service properties.
 *
 * @param[in]  ctx   The application context.
 * @param[in]  prov  Unused.
 * @param[in]  reg   The application registration data.
 */

static void
ToolsCoreDumpProperty(ToolsAppCtx *ctx,
                      ToolsAppProvider *prov,
                      gpointer reg)
{
   if (reg != NULL) {
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN, "Service property: %s.\n",
                         ((ToolsServiceProperty *)reg)->name);
   }
}


/**
 * State dump callback for GuestRPC applications.
 *
 * @param[in]  ctx   The application context.
 * @param[in]  prov  Unused.
 * @param[in]  reg   The application registration data.
 */

static void
ToolsCoreDumpRPC(ToolsAppCtx *ctx,
                 ToolsAppProvider *prov,
                 gpointer reg)
{
   if (reg != NULL) {
      RpcChannelCallback *cb = reg;
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN, "RPC callback: %s\n", cb->name);
   }
}


/**
 * State dump callback for signal connections.
 *
 * @param[in]  ctx   The application context.
 * @param[in]  prov  Unused.
 * @param[in]  reg   The application registration data.
 */

static void
ToolsCoreDumpSignal(ToolsAppCtx *ctx,
                    ToolsAppProvider *prov,
                    gpointer reg)
{
   if (reg != NULL) {
      ToolsPluginSignalCb *sig = reg;
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN, "Signal callback: %s\n", sig->signame);
   }
}


/**
 * Frees memory associated with a ToolsPlugin instance. If the plugin hasn't
 * been initialized yet, this will unload the shared object.
 *
 * @param[in]  plugin   ToolsPlugin instance.
 */

static void
ToolsCoreFreePlugin(ToolsPlugin *plugin)
{
   if (plugin->module != NULL && !g_module_close(plugin->module)) {
      g_warning("Error unloading plugin '%s': %s\n",
                plugin->fileName,
                g_module_error());
   }
   g_free(plugin->fileName);
   g_free(plugin);
}


/**
 * Callback to register applications with the given provider.
 *
 * @param[in]  state    The service state.
 * @param[in]  plugin   The plugin information.
 * @param[in]  type     Application type.
 * @param[in]  preg     Provider information.
 * @param[in]  reg      Application registration.
 *
 * @return Whether to continue registering other apps.
 */

static gboolean
ToolsCoreRegisterApp(ToolsServiceState *state,
                     ToolsPluginData *plugin,
                     ToolsAppType type,
                     ToolsAppProviderReg *preg,
                     gpointer reg)
{
   gboolean error = TRUE;

   if (type == TOOLS_APP_PROVIDER) {
      /* We should already have registered all providers. */
      return TRUE;
   }

   ASSERT(preg != NULL);

   if (preg->state == TOOLS_PROVIDER_ERROR) {
      g_warning("Plugin %s wants to register app of type %d but the "
                "provider failed to activate.\n", plugin->name, type);
      goto exit;
   }

   /*
    * Register the app with the provider, activating it if necessary. If
    * it fails to activate, tag it so we don't try again.
    */
   if (preg->state == TOOLS_PROVIDER_IDLE) {
      if (preg->prov->activate != NULL) {
         GError *err = NULL;
         preg->prov->activate(&state->ctx, preg->prov, &err);
         if (err != NULL) {
            g_warning("Error activating provider %s: %s.\n",
                      preg->prov->name, err->message);
            preg->state = TOOLS_PROVIDER_ERROR;
            g_clear_error(&err);
            goto exit;
         }
      }
      preg->state = TOOLS_PROVIDER_ACTIVE;
   }

   if (!preg->prov->registerApp(&state->ctx, preg->prov, plugin, reg)) {
      g_warning("Failed registration of app type %d (%s) from plugin %s.",
                type, preg->prov->name, plugin->name);
      goto exit;
   }
   error = FALSE;

exit:
   if (error && plugin->errorCb != NULL) {
      return plugin->errorCb(&state->ctx, type, reg, plugin);
   }
   return TRUE;
}


/**
 * Callback to register application providers.
 *
 * @param[in]  state    The service state.
 * @param[in]  plugin   The plugin information.
 * @param[in]  type     Application type.
 * @param[in]  preg     Provider information.
 * @param[in]  reg      Application registration.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreRegisterProvider(ToolsServiceState *state,
                          ToolsPluginData *plugin,
                          ToolsAppType type,
                          ToolsAppProviderReg *preg,
                          gpointer reg)
{
   if (type == TOOLS_APP_PROVIDER) {
      guint k;
      ToolsAppProvider *prov = reg;
      ToolsAppProviderReg newreg = { prov, TOOLS_PROVIDER_IDLE };

      ASSERT(prov->name != NULL);
      ASSERT(prov->registerApp != NULL);

      /* Assert that no two providers choose the same app type. */
      for (k = 0; k < state->providers->len; k++) {
         ToolsAppProviderReg *existing = &g_array_index(state->providers,
                                                        ToolsAppProviderReg,
                                                        k);
         ASSERT(prov->regType != existing->prov->regType);
         g_return_val_if_fail(prov->regType != existing->prov->regType, TRUE);
      }

      g_array_append_val(state->providers, newreg);
   }

   return TRUE;
}


/**
 * Iterates through the list of plugins, and through each plugin's app
 * registration data, calling the appropriate callback for each piece
 * of data.
 *
 * One of the two callback arguments must be provided.
 *
 * @param[in]  state       Service state.
 * @param[in]  pluginCb    Callback called for each plugin data instance.
 * @param[in]  appRegCb    Callback called for each application registration.
 */

static void
ToolsCoreForEachPlugin(ToolsServiceState *state,
                       PluginDataCallback pluginCb,
                       PluginAppRegCallback appRegCb)
{
   guint i;

   ASSERT(pluginCb != NULL || appRegCb != NULL);

   for (i = 0; i < state->plugins->len; i++) {
      ToolsPlugin *plugin = g_ptr_array_index(state->plugins, i);
      GArray *regs = (plugin->data != NULL) ? plugin->data->regs : NULL;
      guint j;

      if (pluginCb != NULL) {
         pluginCb(state, plugin->data);
      }

      if (regs == NULL || appRegCb == NULL) {
         continue;
      }

      for (j = 0; j < regs->len; j++) {
         guint k;
         guint pregIdx;
         ToolsAppReg *reg = &g_array_index(regs, ToolsAppReg, j);
         ToolsAppProviderReg *preg = NULL;

         /* Find the provider for the desired reg type. */
         for (k = 0; k < state->providers->len; k++) {
            ToolsAppProviderReg *tmp = &g_array_index(state->providers,
                                                      ToolsAppProviderReg,
                                                      k);
            if (tmp->prov->regType == reg->type) {
               preg = tmp;
               pregIdx = k;
               break;
            }
         }

         if (preg == NULL) {
            g_message("Cannot find provider for app type %d, plugin %s may not work.\n",
                      reg->type, plugin->data->name);
            if (plugin->data->errorCb != NULL &&
                !plugin->data->errorCb(&state->ctx, reg->type, NULL, plugin->data)) {
               break;
            }
            continue;
         }

         for (k = 0; k < reg->data->len; k++) {
            gpointer appdata = &reg->data->data[preg->prov->regSize * k];
            if (!appRegCb(state, plugin->data, reg->type, preg, appdata)) {
               /* Break out of the outer loop. */
               j = regs->len;
               break;
            }

            /*
             * The registration callback may have modified the provider array,
             * so we need to re-read the provider pointer.
             */
            preg = &g_array_index(state->providers, ToolsAppProviderReg, pregIdx);
         }
      }
   }
}


/**
 * Callback to register service properties.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  prov     Unused.
 * @param[in]  plugin   Unused.
 * @param[in]  reg      The property registration data.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreRegisterProperty(ToolsAppCtx *ctx,
                          ToolsAppProvider *prov,
                          ToolsPluginData *plugin,
                          gpointer reg)
{
   ToolsCoreService_RegisterProperty(ctx->serviceObj, reg);
   return TRUE;
}


/**
 * Registration callback for GuestRPC applications.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  prov     Unused.
 * @param[in]  plugin   Unused.
 * @param[in]  reg      The application registration data.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreRegisterRPC(ToolsAppCtx *ctx,
                     ToolsAppProvider *prov,
                     ToolsPluginData *plugin,
                     gpointer reg)
{
   RpcChannel_RegisterCallback(ctx->rpc, reg);
   return TRUE;
}


/**
 * Registration callback for signal connections.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  prov     Unused.
 * @param[in]  plugin   Unused.
 * @param[in]  reg      The application registration data.
 *
 * @return TRUE if the signal exists.
 */

static gboolean
ToolsCoreRegisterSignal(ToolsAppCtx *ctx,
                        ToolsAppProvider *prov,
                        ToolsPluginData *plugin,
                        gpointer reg)
{
   gboolean valid;
   guint sigId;
   GQuark sigDetail;
   ToolsPluginSignalCb *sig = reg;

   valid = g_signal_parse_name(sig->signame,
                               G_OBJECT_TYPE(ctx->serviceObj),
                               &sigId,
                               &sigDetail,
                               FALSE);
   if (valid) {
      g_signal_connect(ctx->serviceObj,
                       sig->signame,
                       sig->callback,
                       sig->clientData);
      return TRUE;
   }

   g_debug("Plugin '%s' unable to connect to signal '%s'.\n", plugin->name,
           sig->signame);
   return FALSE;
}


/**
 * Compares two strings. To be used with g_ptr_array_sort.
 *
 * @param[in]  _str1    Pointer to string for comparison.
 * @param[in]  _str2    Pointer to string for comparison.
 *
 * @return Result of strcmp.
 */

static gint
ToolsCoreStrPtrCompare(gconstpointer _str1,
                       gconstpointer _str2)
{
   const gchar *str1 = *((const gchar **) _str1);
   const gchar *str2 = *((const gchar **) _str2);
   return strcmp(str1, str2);
}


/**
 * Loads all the plugins found in the given directory, adding the registration
 * data to the given array.
 *
 * @param[in]  ctx         Application context.
 * @param[in]  pluginPath  Path where to look for plugins.
 * @param[out] regs        Array where to store plugin registration info.
 */

static gboolean
ToolsCoreLoadDirectory(ToolsAppCtx *ctx,
                       const gchar *pluginPath,
                       GPtrArray *regs)
{
   gboolean ret = FALSE;
   const gchar *staticEntry;
   guint i;
   GDir *dir = NULL;
   GError *err = NULL;
   GPtrArray *plugins;

   dir = g_dir_open(pluginPath, 0, &err);
   if (dir == NULL) {
      g_warning("Error opening dir: %s\n", err->message);
      g_clear_error(&err);
      goto exit;
   }

   plugins = g_ptr_array_new();

   /*
    * Load plugins in alphabetical order, so the load order is the same
    * regardless of how the filesystem returns entries.
    */
   while ((staticEntry = g_dir_read_name(dir)) != NULL) {
      if (g_str_has_suffix(staticEntry, "." G_MODULE_SUFFIX)) {
         g_ptr_array_add(plugins, g_strdup(staticEntry));
      }
   }

   g_dir_close(dir);

   g_ptr_array_sort(plugins, ToolsCoreStrPtrCompare);

   for (i = 0; i < plugins->len; i++) {
      gchar *entry;
      gchar *path;
      GModule *module = NULL;
      ToolsPlugin *plugin = NULL;
      ToolsPluginOnLoad onload;

      entry = g_ptr_array_index(plugins, i);
      path = g_strdup_printf("%s%c%s", pluginPath, DIRSEPC, entry);

      if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
         g_warning("File '%s' is not a regular file, skipping.\n", entry);
         goto next;
      }

#ifdef USE_APPLOADER
      /* Trying loading the plugins with system libraries */
      if (!LoadDependencies(path, FALSE)) {
         g_warning("Loading of library dependencies for %s failed.\n", entry);
         goto next;
      }
#endif

#ifdef _WIN32
      /*
       * Only load compatible versions of a plugin which requires that a plugin
       * and tools product versions match.
       * Using FALSE compares the major.minor.base components of the version.
       * Version format is: "major.minor.base.buildnumber" e.g. "11.2.0.19761"
       * Use TRUE for a more strict check to verify all four version components.
       */
      if (!ToolsCore_CheckModuleVersion(path, FALSE)) {
         g_warning("%s: Version check of plugin '%s' failed: not loaded.\n",
                    __FUNCTION__, path);
         goto next;
      }
#endif

      module = g_module_open(path, G_MODULE_BIND_LOCAL);
#ifdef USE_APPLOADER
      if (module == NULL) {
         g_info("Opening plugin '%s' with system libraries failed: %s\n",
                   entry, g_module_error());
         /* Falling back to the shipped libraries */
         if (!LoadDependencies(path, TRUE)) {
            g_warning("Loading of shipped library dependencies for %s failed.\n",
                     entry);
            goto next;
         }
         module = g_module_open(path, G_MODULE_BIND_LOCAL);
      }
#endif
      if (module == NULL) {
         g_warning("Opening plugin '%s' failed: %s.\n", entry, g_module_error());
         goto next;
      }

      if (!g_module_symbol(module, "ToolsOnLoad", (gpointer *) &onload)) {
         g_warning("Lookup of plugin entry point for '%s' failed.\n", entry);
         goto next;
      }

      plugin = g_malloc(sizeof *plugin);
      plugin->fileName = entry;
      plugin->data = NULL;
      plugin->module = module;
      plugin->onload = onload;
      g_ptr_array_add(regs, plugin);

   next:
      g_free(path);
      if (plugin == NULL && module != NULL) {
         if (!g_module_close(module)) {
            g_warning("Error unloading plugin '%s': %s\n", entry, g_module_error());
         }
      }
   }

   g_ptr_array_free(plugins, TRUE);
   ret = TRUE;

exit:
   return ret;
}


/**
 * State dump callback for logging information about loaded plugins.
 *
 * @param[in]  state    The service state.
 */

void
ToolsCore_DumpPluginInfo(ToolsServiceState *state)
{
   if (state->plugins == NULL) {
      g_message("   No plugins loaded.");
   } else {
      ToolsCoreForEachPlugin(state, ToolsCoreDumpPluginInfo, ToolsCoreDumpAppInfo);
   }
}


/**
 * Loads all plugins present in the plugin directory. If the plugin path
 * is NULL, then default directories are used in case the service is either
 * the main tools service of the user daemon, otherwise failure is returned.
 *
 * @param[in]  state    The service state.
 *
 * @return Whether loading the plugins was successful.
 */

gboolean
ToolsCore_LoadPlugins(ToolsServiceState *state)
{
   gboolean pluginDirExists;
   gboolean ret = FALSE;
   gchar *pluginRoot;
   guint i;
   GPtrArray *plugins = NULL;

#if defined(sun) && defined(__x86_64__)
   const char *subdir = "/amd64";
#else
   const char *subdir = "";
#endif

#if defined(OPEN_VM_TOOLS)
   pluginRoot = g_strdup(VMTOOLSD_PLUGIN_ROOT);
#else
   char *instPath = GuestApp_GetInstallPath();
   pluginRoot = g_strdup_printf("%s%cplugins", instPath, DIRSEPC);
   vm_free(instPath);
#endif

   ASSERT(g_module_supported());

#ifdef USE_APPLOADER
   {
      Bool ret;
      GModule *mainModule = g_module_open(NULL, G_MODULE_BIND_LAZY);
      ASSERT(mainModule);

      ret = g_module_symbol(mainModule, "AppLoader_LoadLibraryDependencies",
                            (gpointer *)&LoadDependencies);
      g_module_close(mainModule);

      if (!ret) {
         g_critical("Unable to locate library dependency loading function.\n");
         goto exit;
      }
   }
#endif

   plugins = g_ptr_array_new();

   /*
    * First, load plugins from the common directory. The common directory
    * is not required to exist unless provided on the command line.
    */
   if (state->commonPath == NULL) {
      state->commonPath = g_strdup_printf("%s%s%c%s",
                                          pluginRoot,
                                          subdir,
                                          DIRSEPC,
                                          TOOLSCORE_COMMON);
   } else if (!g_file_test(state->commonPath, G_FILE_TEST_IS_DIR)) {
      g_warning("Common plugin path is not a directory: %s\n", state->commonPath);
      goto exit;
   }

   if (g_file_test(state->commonPath, G_FILE_TEST_IS_DIR) &&
       !ToolsCoreLoadDirectory(&state->ctx, state->commonPath, plugins)) {
      goto exit;
   }

   /*
    * Load the container-specific plugins. Ignore if the plugin directory
    * doesn't exist when running in debug mode.
    */

   if (state->pluginPath == NULL) {
      state->pluginPath = g_strdup_printf("%s%s%c%s",
                                          pluginRoot,
                                          subdir,
                                          DIRSEPC,
                                          state->name);
   }

   pluginDirExists = g_file_test(state->pluginPath, G_FILE_TEST_IS_DIR);
   if (state->debugPlugin == NULL && !pluginDirExists) {
      g_warning("Plugin path is not a directory: %s\n", state->pluginPath);
      goto exit;
   }

   if (pluginDirExists &&
       !ToolsCoreLoadDirectory(&state->ctx, state->pluginPath, plugins)) {
      goto exit;
   }


   /*
    * All plugins are loaded, now initialize them.
    */

   state->plugins = g_ptr_array_new();

   for (i = 0; i < plugins->len; i++) {
      ToolsPlugin *plugin = g_ptr_array_index(plugins, i);

      plugin->data = plugin->onload(&state->ctx);

      if (plugin->data == NULL) {
         g_info("Plugin '%s' didn't provide deployment data, unloading.\n",
                plugin->fileName);
         ToolsCoreFreePlugin(plugin);
      } else if (state->ctx.errorCode != 0) {
         /* Break early if a plugin has requested the container to quit. */
         ToolsCoreFreePlugin(plugin);
         break;
      } else {
         ASSERT(plugin->data->name != NULL);
         g_module_make_resident(plugin->module);
         g_ptr_array_add(state->plugins, plugin);
         VMTools_BindTextDomain(plugin->data->name, NULL, NULL);
         g_message("Plugin '%s' initialized.\n", plugin->data->name);
      }
   }


   /*
    * If there is a debug plugin, see if it exports standard plugin registration
    * data too.
    */
   if (state->debugData != NULL && state->debugData->debugPlugin->plugin != NULL) {
      ToolsPluginData *data = state->debugData->debugPlugin->plugin;
      ToolsPlugin *plugin = g_malloc(sizeof *plugin);
      plugin->fileName = NULL;
      plugin->module = NULL;
      plugin->data = data;
      VMTools_BindTextDomain(data->name, NULL, NULL);
      g_ptr_array_add(state->plugins, plugin);
   }

   ret = TRUE;

exit:
   if (plugins != NULL) {
      g_ptr_array_free(plugins, TRUE);
   }
   g_free(pluginRoot);
   return ret;
}


/**
 * Registers all RPC handlers provided by the loaded and enabled plugins.
 *
 * @param[in]  state    The service state.
 */

void
ToolsCore_RegisterPlugins(ToolsServiceState *state)
{
   ToolsAppProvider *fakeProv;
   ToolsAppProviderReg fakeReg;

   if (state->plugins == NULL) {
      return;
   }

   /*
    * Create "fake" app providers for the functionality provided by
    * vmtoolsd (GuestRPC channel, glib signals, custom app providers).
    */
   state->providers = g_array_new(FALSE, TRUE, sizeof (ToolsAppProviderReg));

   if (state->ctx.rpc != NULL) {
      fakeProv = g_malloc0(sizeof *fakeProv);
      fakeProv->regType = TOOLS_APP_GUESTRPC;
      fakeProv->regSize = sizeof (RpcChannelCallback);
      fakeProv->name = "GuestRPC";
      fakeProv->registerApp = ToolsCoreRegisterRPC;
      fakeProv->dumpState = ToolsCoreDumpRPC;

      fakeReg.prov = fakeProv;
      fakeReg.state = TOOLS_PROVIDER_ACTIVE;
      g_array_append_val(state->providers, fakeReg);
   }

   fakeProv = g_malloc0(sizeof *fakeProv);
   fakeProv->regType = TOOLS_APP_SIGNALS;
   fakeProv->regSize = sizeof (ToolsPluginSignalCb);
   fakeProv->name = "Signals";
   fakeProv->registerApp = ToolsCoreRegisterSignal;
   fakeProv->dumpState = ToolsCoreDumpSignal;

   fakeReg.prov = fakeProv;
   fakeReg.state = TOOLS_PROVIDER_ACTIVE;
   g_array_append_val(state->providers, fakeReg);

   fakeProv = g_malloc0(sizeof *fakeProv);
   fakeProv->regType = TOOLS_APP_PROVIDER;
   fakeProv->regSize = sizeof (ToolsAppProvider);
   fakeProv->name = "App Provider";
   fakeProv->registerApp = NULL;
   fakeProv->dumpState = NULL;

   fakeReg.prov = fakeProv;
   fakeReg.state = TOOLS_PROVIDER_ACTIVE;
   g_array_append_val(state->providers, fakeReg);

   fakeProv = g_malloc0(sizeof *fakeProv);
   fakeProv->regType = TOOLS_SVC_PROPERTY;
   fakeProv->regSize = sizeof (ToolsServiceProperty);
   fakeProv->name = "Service Properties";
   fakeProv->registerApp = ToolsCoreRegisterProperty;
   fakeProv->dumpState = ToolsCoreDumpProperty;

   fakeReg.prov = fakeProv;
   fakeReg.state = TOOLS_PROVIDER_ACTIVE;
   g_array_append_val(state->providers, fakeReg);


   /*
    * First app providers need to be identified, so that we know that they're
    * available for use by plugins who need them.
    */
   ToolsCoreForEachPlugin(state, NULL, ToolsCoreRegisterProvider);

   /*
    * Now that we know all app providers, register all the apps, activating
    * individual app providers as necessary.
    */
   ToolsCoreForEachPlugin(state, NULL, ToolsCoreRegisterApp);
}


/**
 * Calls the shutdown callback for all loaded plugins, and cleans up the list
 * of loaded plugins. Plugins are unloaded in the opposite order they were
 * loaded.
 *
 * Note that if a plugin does not provide a shutdown callback, it may leak
 * data that may have been dynamically allocated in the plugin registration
 * info. Since this function is intended to be called once during service
 * shutdown, this it not that big of a deal.
 *
 * @param[in]  state    The service state.
 */

void
ToolsCore_UnloadPlugins(ToolsServiceState *state)
{
   guint i;

   if (state->plugins == NULL) {
      return;
   }

   /* 
    * Signal handlers in some plugins may require RPC Channel. Therefore, we don't
    * emit the signal if RPC channel is not available. See PR 1798412 for details.
    */
   if (state->capsRegistered && state->ctx.rpc) {
      GArray *pcaps = NULL;
      g_signal_emit_by_name(state->ctx.serviceObj,
                            TOOLS_CORE_SIG_CAPABILITIES,
                            &state->ctx,
                            FALSE,
                            &pcaps);

      if (pcaps != NULL) {
         ToolsCore_SetCapabilities(state->ctx.rpc, pcaps, FALSE);
         g_array_free(pcaps, TRUE);
      }
   }

   /*
    * Stop all app providers, and free the memory we allocated for the
    * internal app providers.
    */
   for (i = 0; state->providers != NULL && i < state->providers->len; i++) {
       ToolsAppProviderReg *preg = &g_array_index(state->providers,
                                                  ToolsAppProviderReg,
                                                  i);

      if (preg->prov->shutdown != NULL && preg->state == TOOLS_PROVIDER_ACTIVE) {
         preg->prov->shutdown(&state->ctx, preg->prov);
      }

      if (preg->prov->regType == TOOLS_APP_GUESTRPC ||
          preg->prov->regType == TOOLS_APP_SIGNALS ||
          preg->prov->regType == TOOLS_APP_PROVIDER ||
          preg->prov->regType == TOOLS_SVC_PROPERTY) {
         g_free(preg->prov);
      }
   }

   g_signal_emit_by_name(state->ctx.serviceObj, TOOLS_CORE_SIG_SHUTDOWN, &state->ctx);

   while (state->plugins->len > 0) {
      ToolsPlugin *plugin = g_ptr_array_index(state->plugins, state->plugins->len - 1);
      GArray *regs = (plugin->data != NULL) ? plugin->data->regs : NULL;

      g_message("Unloading plugin '%s'.\n",
                plugin->data != NULL ? plugin->data->name : "unknown");

      if (regs != NULL) {
         guint i;
         for (i = 0; i < regs->len; i++) {
            ToolsAppReg *reg = &g_array_index(regs, ToolsAppReg, i);
            if (reg->data != NULL) {
               g_array_free(reg->data, TRUE);
            }
         }
         g_array_free(regs, TRUE);
      }

      g_ptr_array_remove_index(state->plugins, state->plugins->len - 1);
      ToolsCoreFreePlugin(plugin);
  }

   if (state->providers != NULL) {
      g_array_free(state->providers, TRUE);
      state->providers = NULL;
   }

   g_ptr_array_free(state->plugins, TRUE);
   state->plugins = NULL;
}

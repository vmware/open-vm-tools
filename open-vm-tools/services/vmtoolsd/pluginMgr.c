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
 * @file pluginMgr.c
 *
 *    Provides functions for loading and manipulating Tools plugins.
 */

#include <string.h>
#include "vm_assert.h"
#include "guestApp.h"
#include "toolsCoreInt.h"
#include "util.h"
#include "vmtools.h"


#ifdef USE_APPLOADER
static Bool (*LoadDependencies)(char *libName, Bool useShipped);
#endif

typedef void (*PluginDataCallback)(ToolsServiceState *state,
                                   ToolsPluginData *plugin);

typedef void (*PluginAppRegCallback)(ToolsServiceState *state,
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
 */

static void
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
         g_message("      App type %u (no provider info).\n", type);
      }
    } else {
      g_message("      App type %u (no provider).\n", type);
   }
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
   g_message("   Plugin: %s\n", plugin->name);

   if (plugin->regs == NULL) {
      g_message("      No registrations.\n");
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
      g_message("      RPC callback: %s\n", cb->name);
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
      g_message("      Signal callback: %s\n", sig->signame);
   }
}


/**
 * Callback to register applications with the given provider.
 *
 * @param[in]  state    The service state.
 * @param[in]  plugin   The plugin information.
 * @param[in]  type     Application type.
 * @param[in]  preg     Provider information.
 * @param[in]  reg      Application registration.
 */

static void
ToolsCoreRegisterApp(ToolsServiceState *state,
                     ToolsPluginData *plugin,
                     ToolsAppType type,
                     ToolsAppProviderReg *preg,
                     gpointer reg)
{
   if (preg == NULL) {
      g_warning("Plugin %s wants to register app of type %d but no "
                "provider was found.\n", plugin->name, type);
      return;
   }

   if (preg->state == TOOLS_PROVIDER_ERROR) {
      g_warning("Plugin %s wants to register app of type %d but the "
                "provider failed to activate.\n", plugin->name, type);
      return;
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
            return;
         }
      }
      preg->state = TOOLS_PROVIDER_ACTIVE;
   }

   preg->prov->registerApp(&state->ctx, preg->prov, reg);
}


/**
 * Callback to register application providers.
 *
 * @param[in]  state    The service state.
 * @param[in]  plugin   The plugin information.
 * @param[in]  type     Application type.
 * @param[in]  preg     Provider information.
 * @param[in]  reg      Application registration.
 */

static void
ToolsCoreRegisterProvider(ToolsServiceState *state,
                          ToolsPluginData *plugin,
                          ToolsAppType type,
                          ToolsAppProviderReg *preg,
                          gpointer reg)
{
   if (type == TOOLS_APP_PROVIDER) {
      ToolsAppProvider *prov = reg;
      ToolsAppProviderReg newreg = { prov, TOOLS_PROVIDER_IDLE };

      /* Assert that no two providers choose the same app type. */
      ASSERT(preg == NULL);

      ASSERT(prov->name != NULL);
      ASSERT(prov->registerApp != NULL);
      g_array_append_val(state->providers, newreg);
   }
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
         guint provIdx = -1;
         ToolsAppReg *reg = &g_array_index(regs, ToolsAppReg, j);
         ToolsAppProviderReg *preg = NULL;

         /* Find the provider for the desired reg type. */
         for (k = 0; k < state->providers->len; k++) {
            ToolsAppProviderReg *tmp = &g_array_index(state->providers,
                                                      ToolsAppProviderReg,
                                                      k);
            if (tmp->prov->regType == reg->type) {
               preg = tmp;
               provIdx = k;
               break;
            }
         }

         for (k = 0; k < reg->data->len; k++) {
            gpointer appdata = &reg->data->data[preg->prov->regSize * k];
            appRegCb(state, plugin->data, reg->type, preg, appdata);
         }
      }
   }
}


/**
 * Registration callback for GuestRPC applications.
 *
 * @param[in]  ctx   The application context.
 * @param[in]  prov  Unused.
 * @param[in]  reg   The application registration data.
 */

static void
ToolsCoreRegisterRPC(ToolsAppCtx *ctx,
                     ToolsAppProvider *prov,
                     gpointer reg)
{
   RpcChannel_RegisterCallback(ctx->rpc, reg);
}


/**
 * Registration callback for signal connections.
 *
 * @param[in]  ctx   The application context.
 * @param[in]  prov  Unused.
 * @param[in]  reg   The application registration data.
 */

static void
ToolsCoreRegisterSignal(ToolsAppCtx *ctx,
                        ToolsAppProvider *prov,
                        gpointer reg)
{
   ToolsPluginSignalCb *sig = reg;
   g_signal_connect(ctx->serviceObj,
                    sig->signame,
                    sig->callback,
                    sig->clientData);
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
      goto exit;
   }

   plugins = g_ptr_array_new();

   /*
    * Load plugins in alphabetical order, so the load order is the same
    * regardless of how the filesystem returns entries.
    */
   while ((staticEntry = g_dir_read_name(dir)) != NULL) {
      g_ptr_array_add(plugins, g_strdup(staticEntry));
   }

   g_ptr_array_sort(plugins, ToolsCoreStrPtrCompare);

   for (i = 0; i < plugins->len; i++) {
      gchar *entry;
      gchar *path;
      GModule *module = NULL;
      ToolsPlugin *plugin = NULL;
      ToolsPluginData *data = NULL;
      ToolsPluginOnLoad onload;

      entry = g_ptr_array_index(plugins, i);
      path = g_strdup_printf("%s%c%s", pluginPath, DIRSEPC, entry);

      if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
         g_warning("File '%s' is not a regular file, skipping.\n", entry);
         goto next;
      }

#ifdef USE_APPLOADER
      if (!LoadDependencies(path, FALSE)) {
         g_warning("Loading of library dependencies for %s failed.\n", entry);
         goto next;
      }
#endif

      module = g_module_open(path, G_MODULE_BIND_LOCAL);
      if (module == NULL) {
         g_warning("Opening plugin '%s' failed: %s.\n", entry, g_module_error());
         goto next;
      }

      if (!g_module_symbol(module, "ToolsOnLoad", (gpointer *) &onload)) {
         g_warning("Lookup of plugin entry point for '%s' failed.\n", entry);
         goto next;
      }

      if (onload != NULL) {
         data = onload(ctx);
      }

      if (data == NULL) {
         g_message("Plugin '%s' didn't provide deployment data, unloading.\n", entry);
         goto next;
      }

      ASSERT(data->name != NULL);
      g_module_make_resident(module);
      plugin = g_malloc(sizeof *plugin);
      plugin->module = module;
      plugin->data = data;

      g_ptr_array_add(regs, plugin);
      g_debug("Plugin '%s' initialized.\n", plugin->data->name);

   next:
      g_free(path);
      if (plugin == NULL && module != NULL) {
         if (!g_module_close(module)) {
            g_warning("Error unloading plugin '%s': %s\n", entry, g_module_error());
         }
      }
   }

   for (i = 0; i < plugins->len; i++) {
      g_free(g_ptr_array_index(plugins, i));
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
   gboolean ret = FALSE;
   gchar *pluginRoot;

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
      Bool ret = FALSE;
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

   state->plugins = g_ptr_array_new();

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
       !ToolsCoreLoadDirectory(&state->ctx, state->commonPath, state->plugins)) {
      goto exit;
   }

   /* Load the container-specific plugins. */

   if (state->pluginPath == NULL) {
      state->pluginPath = g_strdup_printf("%s%s%c%s",
                                          pluginRoot,
                                          subdir,
                                          DIRSEPC,
                                          state->name);
   }

   if (!g_file_test(state->pluginPath, G_FILE_TEST_IS_DIR)) {
      g_warning("Plugin path is not a directory: %s\n", state->pluginPath);
      goto exit;
   }

   if (!ToolsCoreLoadDirectory(&state->ctx, state->pluginPath, state->plugins)) {
      goto exit;
   }

   ret = TRUE;

exit:
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
    * Create two "fake" app providers for the functionality provided by
    * vmtoolsd (GuestRPC channel, glib signals).
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
   GArray *pcaps = NULL;

   if (state->plugins == NULL) {
      return;
   }

   g_signal_emit_by_name(state->ctx.serviceObj,
                         TOOLS_CORE_SIG_CAPABILITIES,
                         &state->ctx,
                         FALSE,
                         &pcaps);

   if (pcaps != NULL) {
      if (state->ctx.rpc) {
         ToolsCore_SetCapabilities(state->ctx.rpc, pcaps, FALSE);
      }
      g_array_free(pcaps, TRUE);
   }

   g_signal_emit_by_name(state->ctx.serviceObj, TOOLS_CORE_SIG_SHUTDOWN, &state->ctx);

   /*
    * Stop all app providers, and free the memory we allocated for the two
    * internal app providers.
    */
   for (i = 0; i < state->providers->len; i++) {
       ToolsAppProviderReg *preg = &g_array_index(state->providers,
                                                  ToolsAppProviderReg,
                                                  i);

       if (preg->prov->shutdown != NULL) {
          preg->prov->shutdown(&state->ctx, preg->prov);
       }

      if (preg->prov->regType == TOOLS_APP_GUESTRPC ||
          preg->prov->regType == TOOLS_APP_SIGNALS) {
         g_free(preg->prov);
      }
   }

   while (state->plugins->len > 0) {
      ToolsPlugin *plugin = g_ptr_array_index(state->plugins, state->plugins->len - 1);
      GArray *regs = (plugin->data != NULL) ? plugin->data->regs : NULL;

      g_debug("Unloading plugin '%s'.\n", plugin->data->name);

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
      g_module_close(plugin->module);
      g_free(plugin);
   }

   g_array_free(state->providers, TRUE);
   state->providers = NULL;

   g_ptr_array_free(state->plugins, TRUE);
   state->plugins = NULL;
}

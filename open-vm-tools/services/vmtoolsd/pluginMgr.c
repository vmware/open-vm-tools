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
   const gchar *staticEntry;
   guint i;
   GDir *dir = NULL;
   GError *err = NULL;
   GPtrArray *plugins;

   g_assert(g_module_supported());

   if (state->pluginPath == NULL) {
      if (state->mainService ||
          strcmp(state->name, VMTOOLS_USER_SERVICE) == 0) {
         char *instPath;
         instPath = GuestApp_GetInstallPath();
         state->pluginPath = g_strdup_printf("%s%cplugins%c%s",
                                             instPath,
                                             DIRSEPC,
                                             DIRSEPC,
                                             state->name);
         vm_free(instPath);
      } else {
         g_warning("No plugin path provided for service '%s'.\n", state->name);
         goto exit;
      }
   }

   if (!g_file_test(state->pluginPath, G_FILE_TEST_IS_DIR)) {
      g_warning("Plugin path is not a directory: %s\n", state->pluginPath);
      goto exit;
   }

   dir = g_dir_open(state->pluginPath, 0, &err);
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

   state->plugins = g_ptr_array_new();
   for (i = 0; i < plugins->len; i++) {
      gchar *entry;
      gchar *path;
      GModule *module = NULL;
      ToolsPlugin *plugin = NULL;
      ToolsPluginData *data = NULL;
      ToolsPluginOnLoad onload;

      entry = g_ptr_array_index(plugins, i);
      path = g_strdup_printf("%s%c%s", state->pluginPath, DIRSEPC, entry);

      if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
         g_warning("File '%s' is not a regular file, skipping.\n", entry);
         goto next;
      }

      module = g_module_open(path, G_MODULE_BIND_LOCAL);
      if (module == NULL) {
         g_warning("Opening plugin '%s' failed.\n", entry);
         goto next;
      }

      if (!g_module_symbol(module, "ToolsOnLoad", (gpointer *) &onload)) {
         g_warning("Lookup of plugin entry point for '%s' failed.\n", entry);
         goto next;
      }

      if (onload != NULL) {
         data = onload(&state->ctx);
      }

      if (data == NULL) {
         g_message("Plugin '%s' didn't provide deployment data, unloading.\n", entry);
         goto next;
      }

      g_assert(data->name != NULL);
      g_module_make_resident(module);
      plugin = g_malloc(sizeof *plugin);
      plugin->module = module;
      plugin->data = data;

      g_ptr_array_add(state->plugins, plugin);
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
   if (dir != NULL) {
      g_dir_close(dir);
   }
   g_clear_error(&err);
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
   guint i;

   if (state->plugins == NULL) {
      return;
   }

   for (i = 0; i < state->plugins->len; i++) {
      ToolsPlugin *plugin = g_ptr_array_index(state->plugins, i);
      GArray *regs = (plugin->data != NULL) ? plugin->data->regs : NULL;
      guint j;

      if (regs == NULL) {
         continue;
      }

      for (j = 0; j < regs->len; j++) {
         guint k;
         ToolsAppReg *reg = &g_array_index(regs, ToolsAppReg, j);

         switch (reg->type) {
         case TOOLS_APP_GUESTRPC:
            ASSERT(reg->data != NULL);
            if (state->ctx.rpc == NULL) {
               g_warning("Plugin '%s' asked to register a Guest RPC handler, "
                         "but there's no RPC channel.\n", plugin->data->name);
            } else {
               for (k = 0; k < reg->data->len; k++) {
                  RpcChannelCallback *cb = &g_array_index(reg->data,
                                                          RpcChannelCallback,
                                                          k);
                  RpcChannel_RegisterCallback(state->ctx.rpc, cb);
               }
            }
            break;

         case TOOLS_APP_SIGNALS:
            ASSERT(reg->data != NULL);
            for (k = 0; k < reg->data->len; k++) {
               ToolsPluginSignalCb *sig = &g_array_index(reg->data,
                                                         ToolsPluginSignalCb,
                                                         k);
               g_signal_connect(state->ctx.serviceObj,
                                sig->signame,
                                sig->callback,
                                sig->clientData);
            }
            break;

         default:
            NOT_IMPLEMENTED();
         }
      }
   }
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
      ToolsCore_SetCapabilities(state->ctx.rpc, pcaps, FALSE);
      g_array_free(pcaps, TRUE);
   }

   g_signal_emit_by_name(state->ctx.serviceObj, TOOLS_CORE_SIG_SHUTDOWN, &state->ctx);

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

   g_ptr_array_free(state->plugins, TRUE);
   state->plugins = NULL;
}


/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * @file desktopEvents.c
 *
 * Entry point for the desktop events plugin. Initializes all the individual
 * features of the plugin.
 */

/*
 * DE_MAIN enables the definition of the list of features of this plugin
 * (gFeatures) defined in the platform-specific "deFeatures.h" files.
 */
#define DE_MAIN
#include "vmware.h"
#include "desktopEventsInt.h"

/*
 ******************************************************************************
 * DesktopEventsShutdown --                                             */ /**
 *
 * Description of DesktopEventsShutdown.
 *
 * Calls the shutdown function of the available features.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  plugin   Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static gboolean
DesktopEventsShutdown(ToolsAppCtx *ctx,
                      ToolsPluginData *plugin)
{
   size_t i;

   for (i = 0; i < ARRAYSIZE(gFeatures); i++) {
      DesktopEventFuncs *f = &gFeatures[i];
      if (f->initialized && f->shutdownFn != NULL) {
         f->shutdownFn(ctx);
      }
   }

   return TRUE;
}


/*
 ******************************************************************************
 * ToolsOnLoad --                                                       */ /**
 *
 * Returns the registration data for the plugin.
 *
 * @param[in]  ctx   The application context.
 *
 * @return The registration data.
 *
 ******************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "desktopEvents",
      NULL,
      NULL
   };

   size_t i;

#if defined(_WIN32)
   g_return_val_if_fail(gPluginHandle != NULL, NULL);
#endif

   regData.regs = g_array_new(FALSE, TRUE, sizeof (ToolsAppReg));
   for (i = 0; i < ARRAYSIZE(gFeatures); i++) {
      DesktopEventFuncs *f = &gFeatures[i];
      if (!f->initFn(ctx, &regData)) {
         break;
      }
      f->initialized = TRUE;
   }

   /*
    * Register the shutdown callback and return if all features were
    * initialized successfully.
    */
   if (i == ARRAYSIZE(gFeatures)) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_SHUTDOWN, DesktopEventsShutdown, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) },
      };
      g_array_append_vals(regData.regs, regs, ARRAYSIZE(regs));
      return &regData;
   }

   /* Failed to initialize something, clean up and unload. */
   DesktopEventsShutdown(ctx, &regData);

   /* Cleanup regData to make sure memory is freed. */
   for (i = 0; i < regData.regs->len; i++) {
      ToolsAppReg *reg = &g_array_index(regData.regs, ToolsAppReg, i);
      if (reg->data != NULL) {
         g_array_free(reg->data, TRUE);
      }
   }
   g_array_free(regData.regs, TRUE);

   return NULL;
}


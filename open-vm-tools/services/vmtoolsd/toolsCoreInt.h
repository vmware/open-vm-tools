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

#ifndef _TOOLSCOREINT_H_
#define _TOOLSCOREINT_H_

/**
 * @file toolsCoreInt.h
 *
 *    Internal functions for the tools daemon.
 */

#define VMW_TEXT_DOMAIN    "vmtoolsd"
#define G_LOG_DOMAIN       VMW_TEXT_DOMAIN
#define TOOLSCORE_COMMON   "common"

#include <glib-object.h>
#include <gmodule.h>
#include <time.h>
#include "vmware/tools/plugin.h"
#include "vmware/tools/rpcdebug.h"

/* Used by the Windows implementation to communicate with other processes. */
#if defined(G_PLATFORM_WIN32)
#  define QUIT_EVENT_NAME_FMT         L"%S\\VMwareToolsQuitEvent_%s"
#  define DUMP_STATE_EVENT_NAME_FMT   L"%S\\VMwareToolsDumpStateEvent_%s"
#endif

/* On Mac OS, G_MODULE_SUFFIX seems to be defined to "so"... */
#if defined(__APPLE__)
#  if defined(G_MODULE_SUFFIX)
#     undef G_MODULE_SUFFIX
#  endif
#  define G_MODULE_SUFFIX "dylib"
#endif

#define VMTOOLS_APP_NAME "vmtools"

/** State of app providers. */
typedef enum {
   TOOLS_PROVIDER_IDLE,
   TOOLS_PROVIDER_ACTIVE,
   TOOLS_PROVIDER_ERROR,

   /* Keep this as the last one, always. */
   TOOLS_PROVIDER_MAX
} ToolsAppProviderState;

/** Defines the internal app provider data. */
typedef struct ToolsAppProviderReg {
   ToolsAppProvider       *prov;
   ToolsAppProviderState   state;
} ToolsAppProviderReg;

/** Defines internal service state. */
typedef struct ToolsServiceState {
   gchar         *name;
   gchar         *configFile;
   time_t         configMtime;
   guint          configCheckTask;
   gboolean       mainService;
   gboolean       capsRegistered;
   gchar         *commonPath;
   gchar         *pluginPath;
   GPtrArray     *plugins;
#if defined(_WIN32)
   gchar         *displayName;
#else
   gchar         *pidFile;
#endif
   GModule       *debugLib;
   gchar         *debugPlugin;
   RpcDebugLibData  *debugData;
   ToolsAppCtx    ctx;
   GArray        *providers;
#if defined(__linux__)
   /*
    * We hold a reference to vSocket device to avoid
    * address family re-registration when someone
    * connects over vSocket. We have vsockFamily
    * here mainly because it does not cost much
    * and it is useful for debug logs.
    */
   int            vsockDev;
   int            vsockFamily;
#endif
} ToolsServiceState;


gboolean
ToolsCore_ParseCommandLine(ToolsServiceState *state,
                           int argc,
                           char *argv[]);

void
ToolsCore_DumpPluginInfo(ToolsServiceState *state);

void
ToolsCore_DumpState(ToolsServiceState *state);

guint
ToolsCore_GetVmusrLimit(ToolsServiceState *state);

const char *
ToolsCore_GetTcloName(ToolsServiceState *state);

int
ToolsCore_Run(ToolsServiceState *state);

void
ToolsCore_Setup(ToolsServiceState *state);

gboolean
ToolsCore_InitRpc(ToolsServiceState *state);

#if defined(__linux__)
void
ToolsCore_InitVsockFamily(ToolsServiceState *state);

void
ToolsCore_ReleaseVsockFamily(ToolsServiceState *state);
#endif

gboolean
ToolsCore_LoadPlugins(ToolsServiceState *state);

void
ToolsCore_ReloadConfig(ToolsServiceState *state,
                       gboolean reset);

void
ToolsCore_RegisterPlugins(ToolsServiceState *state);

void
ToolsCore_SetCapabilities(RpcChannel *chan,
                          GArray *caps,
                          gboolean set);

void
ToolsCore_UnloadPlugins(ToolsServiceState *state);

#if defined(__APPLE__)
void
ToolsCore_CFRunLoop(ToolsServiceState *state);
#endif

void
ToolsCorePool_Init(ToolsAppCtx *ctx);

void
ToolsCorePool_Shutdown(ToolsAppCtx *ctx);

#endif /* _TOOLSCOREINT_H_ */


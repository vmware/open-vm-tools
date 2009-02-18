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
 * @file resolution.c --
 *
 * Set of functions to handle guest screen resizing for vmware-{user,guestd}.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "vm_app.h"
#include "debug.h"
#include "rpcout.h"
#include "str.h"
#include "strutil.h"

#include "resolutionInt.h"

#include "vmtools.h"
#include "vmtoolsApp.h"
#include "xdrutil.h"

/*
 * Internal global variables
 */

/**
 * Describes current state of the library.
 */
ResolutionInfoType resolutionInfo;

/*
 * Local function prototypes
 */

static Bool ResolutionResolutionSetCB(RpcInData *data);
static Bool ResolutionDisplayTopologySetCB(RpcInData *data);

/*
 * Global function definitions
 */

/**
 *
 * Initialize the guest resolution library.
 *
 * @param[in] handle  Back-end specific handle, if needed.  E.g., in the X11
                      case, this refers to the X11 display handle.
 * @return TRUE on success, FALSE on failure
 */

static Bool
ResolutionInit(InitHandle handle)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   ASSERT(resInfo->initialized == FALSE);

   if (!ResolutionBackendInit(handle)) {
      return FALSE;
   }

   resInfo->initialized = TRUE;

   return TRUE;
}


/**
 *
 * Shutdown the plugin, free resources, etc.
 * Resolution_* calls will fail until user next calls ResolutionInit().
 */

static void
ResolutionCleanup(void)
{
   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized) {
      return;
   }

   ResolutionBackendCleanup();
}


/**
 *
 * Handler for TCLO 'Resolution_Set'.
 *
 * Routine unmarshals RPC arguments and passes over to back-end ResolutionSet().
 *
 * @param[in] data RPC data
 * @return TRUE if we can reply, FALSE otherwise.
 */

static Bool
ResolutionResolutionSetCB(RpcInData *data)
{
   uint32 width = 0 ;
   uint32 height = 0;
   unsigned int index = 0;
   Bool retval = FALSE;

   /* parse the width and height */
   if (!StrUtil_GetNextUintToken(&width, &index, data->args, " ")) {
      goto invalid_arguments;
   }
   if (!StrUtil_GetNextUintToken(&height, &index, data->args, "")) {
      goto invalid_arguments;
   }

   retval = ResolutionSetResolution(width, height);

invalid_arguments:
   return RPCIN_SETRETVALS(data, retval ? "" : "Invalid arguments", retval);
}


/**
 *
 * Handler for TCLO 'DisplayTopology_Set'.
 *
 * Routine unmarshals RPC arguments and passes over to back-end TopologySet().
 *
 * @param[in] data RPC data
 * @return TRUE if we can reply, FALSE otherwise.
 */

static Bool
ResolutionDisplayTopologySetCB(RpcInData *data)
{
   DisplayTopologyInfo *displays = NULL;
   unsigned int count, i;
   Bool success = FALSE;
   const char *p;

   /*
    * The argument string will look something like:
    *   <count> [ , <x> <y> <w> <h> ] * count.
    *
    * e.g.
    *    3 , 0 0 640 480 , 640 0 800 600 , 0 480 640 480
    */

   if (sscanf(data->args, "%u", &count) != 1) {
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"count\"",
                              FALSE);
   }

   displays = malloc(sizeof *displays * count);
   if (!displays) {
      RPCIN_SETRETVALS(data,
                       "Failed to alloc buffer for display info",
                       FALSE);
      goto out;
   }

   for (p = data->args, i = 0; i < count; i++) {
      p = strchr(p, ',');
      if (!p) {
         RPCIN_SETRETVALS(data,
                          "Expected comma separated display list",
                          FALSE);
         goto out;
      }
      p++; /* Skip past the , */

      if (sscanf(p, " %d %d %d %d ", &displays[i].x,
                 &displays[i].y, &displays[i].width, &displays[i].height) != 4) {
         RPCIN_SETRETVALS(data,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }
   }

   success = ResolutionSetTopology(count, displays);

   RPCIN_SETRETVALS(data, success ? "" : "ResolutionSetTopology failed", success);

out:
   free(displays);
   return success;
}


/**
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
ResolutionSetShutdown(gpointer src,
                      ToolsAppCtx *ctx,
                      gpointer data)
{
   ResolutionCleanup();
}


/**
 * Sends the resolution_server capability to the VMX.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 * @param[in]  set      Whether setting or unsetting the capability.
 *
 * @return NULL. The function sends the capability directly.
 */

static void
ResolutionServerCapReg(ToolsAppCtx *ctx,
                       gint set)
{
   gchar *msg;
   const char *appName = NULL;

   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) == 0) {
      appName = TOOLS_DAEMON_NAME;
   } else if (strcmp(ctx->name, VMTOOLS_USER_SERVICE) == 0) {
      appName = TOOLS_DND_NAME;
   } else {
      NOT_REACHED();
   }

   msg = g_strdup_printf("tools.capability.resolution_server %s %d",
                         appName,
                         set);

   if (!RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_warning("Setting resolution_server capability failed!\n");
   }

   g_free(msg);
}


/**
 * Returns the list of the plugin's capabilities.
 *
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  set      Whether setting or unsetting the capability.
 * @param[in]  data     Unused.
 *
 * @return A list of capabilities.
 */

static GArray *
ResolutionSetCapabilities(gpointer src,
                          ToolsAppCtx *ctx,
                          gboolean set,
                          gpointer data)
{
   enum {
      RES_SET_IDX              = 0,
      DPY_TOPO_SET_IDX         = 1,
      DPY_GLOBAL_OFFSET_IDX    = 2
   };

   ToolsAppCapability caps[] = {
      { TOOLS_CAP_OLD, "resolution_set", 0, 0 },
      { TOOLS_CAP_OLD, "display_topology_set", 0, 0 },
      { TOOLS_CAP_OLD, "display_global_offset", 0, 0 }
   };

   ResolutionInfoType *resInfo = &resolutionInfo;
   int resServerCap = 0;

   if (set) {
      if (!resInfo->initialized) {
         return FALSE;
      }

      if (resInfo->canSetResolution) {
         caps[RES_SET_IDX].value = 1;
         resServerCap = 1;
      }

      if (resInfo->canSetTopology) {
         caps[DPY_TOPO_SET_IDX].value = 2;
         caps[DPY_GLOBAL_OFFSET_IDX].value = 1;
      }
   }

   ResolutionServerCapReg(ctx, resServerCap);

   return VMTools_WrapArray(caps, sizeof *caps, ARRAYSIZE(caps));
}


/**
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   InitHandle handle;

   static ToolsPluginData regData = {
      "resolutionSet",
      NULL,
      NULL
   };

   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_CAPABILITIES, ResolutionSetCapabilities, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, ResolutionSetShutdown, &regData }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, NULL },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   ResolutionInfoType *resInfo = &resolutionInfo;

   resInfo->initialized = FALSE;

   /*
    * XXX move to some shared lib or plugin
    */
   handle = ResolutionToolkitInit();

   ResolutionInit(handle);

   /*
    * Add one or both of the callbacks based on capabilities.
    */
   if (resInfo->canSetResolution || resInfo->canSetTopology) {
      int index = 0;
      RpcChannelCallback rpcs[2];

      memset(rpcs, '\0', sizeof rpcs);

      if (resInfo->canSetResolution) {
         rpcs[index].name = "Resolution_Set";
         rpcs[index].callback = ResolutionResolutionSetCB;
         index++;
      }

      if (resInfo->canSetTopology) {
         rpcs[index].name = "DisplayTopology_Set";
         rpcs[index].callback = ResolutionDisplayTopologySetCB;
         index++;
      }

      regs[0].data = VMTools_WrapArray(rpcs, sizeof *rpcs, index);
      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   }

   return &regData;
}

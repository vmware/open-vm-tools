/*********************************************************
 * Copyright (C) 2008-2018 VMware, Inc. All rights reserved.
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
#include "debug.h"
#include "rpcout.h"
#include "str.h"
#include "strutil.h"

#include "resolutionInt.h"

#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"


#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);

/*
 * The maximum number of capabilities we can set.
 *
 * See ResolutionSetCapabilities().
 */
#define RESOLUTION_SET_CAPABILITIES_MAX 5

/*
 * Internal global variables
 */

/**
 * The name of the RPC channel we're using, e.g. TOOLS_DAEMON_NAME. Used by
 * ResolutionSet_SetServerCapability() to determine which capability to set.
 */
static const char *rpcChannelName = NULL;

/**
 * Describes current state of the library.
 */
ResolutionInfoType resolutionInfo;


/*
 * Global function definitions
 */

/**
 *
 * Initialize the guest resolution library.
 *
 * @param[in] handle  Back-end specific handle, if needed.
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

static gboolean
ResolutionResolutionSetCB(RpcInData *data)
{
   uint32 width = 0 ;
   uint32 height = 0;
   unsigned int index = 0;
   gboolean retval = FALSE;

   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized) {
      g_debug("%s: FAIL! Request for resolution set but plugin is not initialized\n",
              __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid guest state: resolution set not initialized", FALSE);
   }

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


#if defined(RESOLUTION_WIN32)
/**
 *
 * Handler for TCLO 'ChangeHost3DAvailabilityHint'.
 *
 * Routine unmarshals RPC arguments and passes over to back-end for handling.
 *
 * @param[in] data RPC data
 * @return TRUE if we can reply, FALSE otherwise.
 */

static gboolean
ResolutionChangeHost3DAvailabilityHintCB(RpcInData *data)
{
   unsigned int set;
   gboolean success = FALSE;
   unsigned int index = 0;

   g_debug("%s: enter\n", __FUNCTION__);

   if (!StrUtil_GetNextUintToken(&set, &index, data->args, " ")) {
      g_debug("%s: invalid arguments\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"set\"",
                              FALSE);
   }

   success = ResolutionChangeHost3DAvailabilityHint(set?TRUE:FALSE);

   RPCIN_SETRETVALS(data, success ? "" : "ResolutionChangeHost3DAvailabilityHint failed", success);

   g_debug("%s: leave\n", __FUNCTION__);
   return success;
}


/**
 *
 * Handler for TCLO 'DisplayTopologyModes_Set'.
 *
 * Routine unmarshals RPC arguments and passes over to back-end
 * ModesTopologySet().
 *
 * @note the following can be added as a unit test:
 *
 * RpcInData testdata;
 * testdata.args = "10 0 1, 1111 111, 2222 222, 3333 333, 4444 444, 5555 555, 6666 666, 7777 777, 8888 888, 9999 999, 0000 000";
 * ResolutionDisplayTopologyModesSetCB(&testdata);
 *
 * @param[in] data RPC data
 * @return TRUE if we can reply, FALSE otherwise.
 */

static gboolean
ResolutionDisplayTopologyModesSetCB(RpcInData *data)
{
   DisplayTopologyInfo *displays = NULL;
   unsigned int count;
   unsigned int i;
   unsigned int cmd;
   unsigned int screen;
   gboolean success = FALSE;
   const char *p;

   g_debug("%s: enter\n", __FUNCTION__);

   /*
    * The argument string will look something like:
    *   <count> <screen> <cmd> [ , <w> <h> ] * count.
    *
    * e.g.
    *    3 0 1, 640 480 , 800 600 , 1024 768
    */

   if (sscanf(data->args, "%u %u %u", &count, &screen, &cmd) != 3) {
      g_debug("%s: invalid arguments\n", __FUNCTION__);
      return RPCIN_SETRETVALS(data,
                              "Invalid arguments. Expected \"count\", \"screen\",  and \"cmd\"",
                              FALSE);
   }

   displays = malloc(sizeof *displays * count);
   if (!displays) {
      g_debug("%s: alloc failed\n", __FUNCTION__);
      RPCIN_SETRETVALS(data,
                       "Failed to alloc buffer for display modes",
                       FALSE);
      goto out;
   }

   for (p = data->args, i = 0; i < count; i++) {
      p = strchr(p, ',');
      if (!p) {
         g_debug("%s: expected comma separated display modes list\n", __FUNCTION__);
         RPCIN_SETRETVALS(data,
                          "Expected comma separated display modes list",
                          FALSE);
         goto out;
      }
      p++; /* Skip past the , */

      if (sscanf(p, " %d %d ", &displays[i].width, &displays[i].height) != 2) {
         g_debug("%s: expected w, h in display modes entry\n", __FUNCTION__);
         RPCIN_SETRETVALS(data,
                          "Expected w, h in display modes entry",
                          FALSE);
         goto out;
      }
   }

   success = ResolutionSetTopologyModes(screen, cmd, count, displays);

   RPCIN_SETRETVALS(data, success ? "" : "ResolutionSetTopologyModes failed", success);

out:
   free(displays);
   g_debug("%s: leave\n", __FUNCTION__);
   return success;
}
#endif


/**
 *
 * Handler for TCLO 'DisplayTopology_Set'.
 *
 * Routine unmarshals RPC arguments and passes over to back-end TopologySet().
 *
 * @param[in] data RPC data
 * @return TRUE if we can reply, FALSE otherwise.
 */

static gboolean
ResolutionDisplayTopologySetCB(RpcInData *data)
{
   DisplayTopologyInfo *displays = NULL;
   unsigned int count, i;
   gboolean success = FALSE;
   const char *p;

   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized) {
      g_debug("%s: FAIL! Request for topology set but plugin is not initialized\n",
              __FUNCTION__);
      RPCIN_SETRETVALS(data, "Invalid guest state: topology set not initialized", FALSE);
      goto out;
   }

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
 * Sends the tools.capability.resolution_server RPC to the VMX.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  value    The value to send for the capability bit.
 */

static void
ResolutionSetServerCapability(RpcChannel *chan,
                              unsigned int value)
{
   gchar *msg;

   if (!rpcChannelName) {
      g_debug("Channel name is null, RPC not sent.\n");
      return;
   }

   msg = g_strdup_printf("tools.capability.resolution_server %s %d",
                         rpcChannelName,
                         value);
   if (!RpcChannel_Send(chan, msg, strlen(msg), NULL, NULL)) {
      g_warning("%s: Unable to set tools.capability.resolution_server\n",
                __FUNCTION__);
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
   /* The array of capabilities to return to the tools service. */
   ToolsAppCapability capabilityArray[RESOLUTION_SET_CAPABILITIES_MAX];

   /* The next unused entry in the capabilities array. */
   unsigned int capabilityCount = 0;

   ResolutionInfoType *resInfo = &resolutionInfo;

   g_debug("%s: enter\n", __FUNCTION__);

   if (!resInfo->initialized) {
      return FALSE;
   }

   /*
    * XXX: We must register display_topology_set before resolution_set to avoid
    *      a race condition in the host. See bug 472343.
    */
   /*
    * If we can set the guest topology, add the display_topology_set and
    * display_global_offset capabilities to our array.
    */
   if (resInfo->canSetTopology) {
      /*
       * XXX: We use a value of '2' here because, for historical reasons, the
       *      Workstation/Fusion UI will treat a value of 1 for this capability
       *      as unsupported. See bug 149541.
       */
      capabilityArray[capabilityCount].type  = TOOLS_CAP_OLD;
      capabilityArray[capabilityCount].name  = "display_topology_set";
      capabilityArray[capabilityCount].index = 0;
      capabilityArray[capabilityCount].value = set ? 2 : 0;
      capabilityCount++;

      capabilityArray[capabilityCount].type  = TOOLS_CAP_OLD;
      capabilityArray[capabilityCount].name  = "display_global_offset";
      capabilityArray[capabilityCount].index = 0;
      capabilityArray[capabilityCount].value = set ? 1 : 0;
      capabilityCount++;
   }

   /*
    * If we can set the guest resolution, add the resolution_set capability to
    * our array.
    */
   if (resInfo->canSetResolution) {
      capabilityArray[capabilityCount].type  = TOOLS_CAP_OLD;
      capabilityArray[capabilityCount].name  = "resolution_set";
      capabilityArray[capabilityCount].index = 0;
      capabilityArray[capabilityCount].value = set ? 1 : 0;
      capabilityCount++;

      /*
       * Send the resolution_server RPC to the VMX.
       *
       * XXX: We need to send this ourselves, instead of including it in the
       *      capability array, because the resolution_server RPC includes the
       *      name of the RPC channel that the VMX should use when sending
       *      resolution set RPCs as an argument.
       */
      if (ctx && ctx->rpc && ctx->isVMware) {
         ResolutionSetServerCapability(ctx->rpc, set ? 1 : 0);
      }
   }

#if defined(RESOLUTION_WIN32)
   /*
    * XXX: I believe we can always handle these RPCs from the service, even on
    *      Vista, so we always set the capabilities here, regardless of the
    *      value of resInfo->canSetTopology.
    */
   g_debug("%s: setting DPY_TOPO_MODES_SET_IDX to %u\n", __FUNCTION__,
           set ? 1 : 0);

   capabilityArray[capabilityCount].type  = TOOLS_CAP_NEW;
   capabilityArray[capabilityCount].name  = NULL;
   capabilityArray[capabilityCount].index = CAP_SET_TOPO_MODES;
   capabilityArray[capabilityCount].value = set ? 1 : 0;
   capabilityCount++;

   capabilityArray[capabilityCount].type  = TOOLS_CAP_NEW;
   capabilityArray[capabilityCount].name  = NULL;
   capabilityArray[capabilityCount].index = CAP_CHANGE_HOST_3D_AVAILABILITY_HINT;
   capabilityArray[capabilityCount].value = set ? 1 : 0;
   capabilityCount++;
#endif

   ASSERT(capabilityCount <= RESOLUTION_SET_CAPABILITIES_MAX);

   return VMTools_WrapArray(capabilityArray,
                            sizeof *capabilityArray,
                            capabilityCount);
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
   RpcChannelCallback rpcs[] = {
      { "Resolution_Set",               &ResolutionResolutionSetCB },
      { "DisplayTopology_Set",          &ResolutionDisplayTopologySetCB },
#if defined(RESOLUTION_WIN32)
      { "DisplayTopologyModes_Set",     &ResolutionDisplayTopologyModesSetCB },
      { "ChangeHost3DAvailabilityHint", &ResolutionChangeHost3DAvailabilityHintCB },
#endif
   };

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

   /*
    * If we aren't running in a VM (e.g., running in bootcamp natively on
    * a Mac), then just return NULL.
    */
   if (!ctx->isVMware) {
      return NULL;
   }

   /*
    * Save the RPC channel name from the ToolsAppCtx so that we can use it later
    * in calls to ResolutionSetServerCapability().
    */

   if (TOOLS_IS_MAIN_SERVICE(ctx)) {
      rpcChannelName = TOOLS_DAEMON_NAME;
   } else if (TOOLS_IS_USER_SERVICE(ctx)) {
      rpcChannelName = TOOLS_DND_NAME;
   } else {
      NOT_REACHED();
   }

   resInfo->initialized = FALSE;

   /*
    * XXX move to some shared lib or plugin
    */
   handle = ResolutionToolkitInit(ctx);

   if (!ResolutionInit(handle))
      return NULL;

   regs[0].data = VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs));
   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   return &regData;
}

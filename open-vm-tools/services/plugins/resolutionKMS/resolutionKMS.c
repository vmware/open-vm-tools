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
 * @file resolutionKMS.c --
 *
 * Plugin to communicate GUI topology to the vmwgfx drm device through a
 * control node. This file is a modified version of resolutionSet.c
 */

#define G_LOG_DOMAIN "resolutionKMS"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "strutil.h"

#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "../resolutionSet/resolutionCommon.h"
#include "../resolutionSet/resolutionDL.h"

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
 * Global information about the communication state
 */
typedef struct {
   gboolean initialized;       // Whether the plugin is already initialized.
   int fd;                     // File descriptor to the DRM device.
} KMSInfoType;

/*
 * Internal global variables
 */
KMSInfoType kmsInfo;

/*
 * The name of the RPC channel we're using, e.g. TOOLS_DAEMON_NAME. Used by
 * ResolutionSet_SetServerCapability() to determine which capability to set.
 */
static const char *rpcChannelName = NULL;

/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionWriteToKernel --
 *
 *     Write GUI topology info to the drm device.
 *
 * Results:
 *     TRUE on success, FALSE on failure.
 *
 * Side effects:
 *     The drm device will send an uevent and expose the new
 *     topology.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ResolutionWriteToKernel(const struct drm_vmw_rect *rects, // IN: Screen rects
			unsigned int num_rects)           // IN: Number of
                                                          // rects
{
   struct drm_vmw_update_layout_arg arg;
   int ret;

   memset(&arg, 0, sizeof arg);
   arg.num_outputs = num_rects;
   arg.rects = (unsigned long) rects;

   ret = drmCommandWrite(kmsInfo.fd, DRM_VMW_UPDATE_LAYOUT, &arg, sizeof arg);
   if (ret < 0) {
      g_debug("%s: FAIL! Resolutionset write to kernel failed: %d\n",
              __FUNCTION__, ret);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionResolutionSetCB --
 *
 *     Handler for TCLO 'Resolution_Set'.
 *     Routine unmarshals RPC arguments and passes over to back-end
 *     ResolutionWriteToKernel().
 *
 * Results:
 *     TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ResolutionResolutionSetCB(RpcInData *data) // IN: The RPC data
{
   struct drm_vmw_rect rect;
   unsigned int index = 0;
   gboolean retval = FALSE;

   if (!kmsInfo.initialized) {
      g_debug("%s: FAIL! Request for resolution set but plugin is not initialized\n",
              __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Invalid guest state: resolution set not initialized", FALSE);
   }

   rect.x = 0;
   rect.y = 0;

   /* parse the width and height */
   if (!StrUtil_GetNextUintToken(&rect.w, &index, data->args, " ")) {
      goto invalid_arguments;
   }
   if (!StrUtil_GetNextUintToken(&rect.h, &index, data->args, "")) {
      goto invalid_arguments;
   }

   retval = ResolutionWriteToKernel(&rect, 1);

invalid_arguments:
   return RPCIN_SETRETVALS(data, retval ? "" : "Invalid arguments", retval);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionDisplayTopologySetCB --
 *
 *     Handler for TCLO 'DisplayTopology_Set'.
 *     Routine unmarshals RPC arguments and passes over to back-end
 *     ResolutionWriteToKernel().
 *
 * Results:
 *     TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ResolutionDisplayTopologySetCB(RpcInData *data)
{
   struct drm_vmw_rect *rects = NULL;
   unsigned int count, i;
   gboolean success = FALSE;
   const char *p;

   if (!kmsInfo.initialized) {
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

   rects = malloc(sizeof *rects * count);
   if (!rects) {
      RPCIN_SETRETVALS(data,
                       "Failed to alloc buffer for display info",
                       FALSE);
      return FALSE;
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

      if (sscanf(p, " %d %d %d %d ", &rects[i].x,
                 &rects[i].y, &rects[i].w, &rects[i].h) != 4) {
         RPCIN_SETRETVALS(data,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }
   }

   success = ResolutionWriteToKernel(rects, count);
   RPCIN_SETRETVALS(data, success ? "" : "ResolutionSetTopology failed", success);
out:
   free(rects);
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionKMSServerCapability --
 *
 * Sends the tools.capability.resolution_server RPC to the VMX.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ResolutionKMSServerCapability(RpcChannel *chan,   // IN: The RPC channel.
                              unsigned int value) // IN: The value to send for
                                                  // the capability bit.
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

   if (value == 1) {
      /*
       * Whenever resolutionKMS is enabled, send
       * "tools.capability.resolution_server toolbox-dnd 0" to clear
       * resolutionSet as resolution server.
       *
       * Note: The below rpc is sent to TOOLS_DND_NAME if rpcChannelName is
       * TOOLS_DAEMON_NAME and vice versa (to clear the opposite channel).
       * This is how rpcChannelName is selected in ToolsOnLoad():
       *
       *    if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) == 0) {
       *       rpcChannelName = TOOLS_DAEMON_NAME;
       *    } else if (strcmp(ctx->name, VMTOOLS_USER_SERVICE) == 0) {
       *       rpcChannelName = TOOLS_DND_NAME;
       *    }
       */
      gchar *msgClear;
      msgClear = g_strdup_printf("tools.capability.resolution_server %s 0",
                                 (strcmp(rpcChannelName, TOOLS_DAEMON_NAME) == 0 ?
                                 TOOLS_DND_NAME : TOOLS_DAEMON_NAME));
      if (!RpcChannel_Send(chan, msgClear, strlen(msgClear), NULL, NULL)) {
         g_warning("%s: Unable to clear tools.capability.resolution_server\n",
                   __FUNCTION__);
      }
      g_free(msgClear);
   }

   g_free(msg);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionKMSShutdown --
 *
 * Cleans up internal data on shutdown.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ResolutionKMSShutdown(gpointer src,     // IN: Unused.
                      ToolsAppCtx *ctx, // IN: The app context.
                      gpointer data)    // IN: Unused.
{
   if (kmsInfo.initialized && ctx && ctx->rpc && ctx->isVMware) {
      ResolutionKMSServerCapability(ctx->rpc, 0);
   }

   if (kmsInfo.initialized) {
      resolutionDRMClose(kmsInfo.fd);
      kmsInfo.initialized = FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionKMSCapabilities --
 *
 * Cleans up internal data on shutdown.
 *
 * Results:
 *     An array of capabilities
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static GArray *
ResolutionKMSCapabilities(gpointer src,     // IN: The source object.
                          ToolsAppCtx *ctx, // IN: The app context.
                          gboolean set,     // IN: Whether setting or unsetting
			                    // the capability.
                          gpointer data)    // Unused.
{
   /* The array of capabilities to return to the tools service. */
   ToolsAppCapability capabilityArray[RESOLUTION_SET_CAPABILITIES_MAX];

   /* The next unused entry in the capabilities array. */
   unsigned int capabilityCount = 0;

   g_debug("%s: enter\n", __FUNCTION__);

   /*
    * We must register display_topology_set before resolution_set to avoid
    * a race condition in the host. See bug 472343.
    */

   /*
    * We use a value of '2' here because, for historical reasons, the
    * Workstation/Fusion UI will treat a value of 1 for this capability
    * as unsupported. See bug 149541.
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

   if (kmsInfo.initialized && ctx && ctx->rpc && ctx->isVMware) {
      ResolutionKMSServerCapability(ctx->rpc, set ? 1:0);
   }

   ASSERT(capabilityCount <= RESOLUTION_SET_CAPABILITIES_MAX);

   return VMTools_WrapArray(capabilityArray,
                            sizeof *capabilityArray,
                            capabilityCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsOnLoad --
 *
 * Plugin entry point
 *
 * Results:
 *     The registration data.
 *
 * Side effects:
 *     Initializes internal plugin state.
 *
 *-----------------------------------------------------------------------------
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   RpcChannelCallback rpcs[] = {
      { "Resolution_Set",               &ResolutionResolutionSetCB },
      { "DisplayTopology_Set",          &ResolutionDisplayTopologySetCB },
   };

   static ToolsPluginData regData = {
      "resolutionKMS",
      NULL,
      NULL
   };

   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_CAPABILITIES, ResolutionKMSCapabilities, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, ResolutionKMSShutdown, &regData }
   };

   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, NULL },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   /*
    * If we aren't running in a VM (e.g., running in bootcamp natively on
    * a Mac), then just return NULL.
    */
   if (!ctx->isVMware) {
      return NULL;
   }

   kmsInfo.fd = resolutionCheckForKMS(ctx);
   if (kmsInfo.fd < 0) {
      return NULL;
   }
   kmsInfo.initialized = TRUE;

   /*
    * Save the RPC channel name from the ToolsAppCtx so that we can use it later
    * in calls to ResolutionKMSServerCapability().
    */

   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) == 0) {
      rpcChannelName = TOOLS_DAEMON_NAME;
   } else if (strcmp(ctx->name, VMTOOLS_USER_SERVICE) == 0) {
      rpcChannelName = TOOLS_DND_NAME;
   } else {
      NOT_REACHED();
   }

   regs[0].data = VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs));
   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   return &regData;
}

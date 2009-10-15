/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

/*
 * resolution.c --
 *
 *     Set of functions to handle guest screen resizing for
 *     vmware-{user,guestd}.
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
#include "resolution.h"


/*
 * Internal global variables
 */


/*
 * Describes current state of the library.
 */
ResolutionInfoType resolutionInfo = { .initialized = FALSE };


/*
 * Local function prototypes
 */

static Bool ResolutionResolutionSetCB(RpcInData *data);
static Bool ResolutionDisplayTopologySetCB(RpcInData *data);


/*
 * Global function definitions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_Init --
 *
 *      Initialize the guest resolution library.
 *
 * Results:
 *      TRUE on success, FALSE on failure (bad arguments?).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Resolution_Init(const char *tcloChannel,
                        // IN: TCLO channel name; used during capability registration
                        //     to tell the VMX whether Resolution_Set is being handled
                        //     by VMwareService/guestd or VMwareUser/vmware-user.
                InitHandle handle)
                        // IN: Back-end specific handle, if needed.  E.g., in the X11
                        //     case, this refers to the X11 display handle.
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   ASSERT(resInfo->initialized == FALSE);
   ASSERT((strcmp(tcloChannel, TOOLS_DAEMON_NAME) == 0) ||
          (strcmp(tcloChannel, TOOLS_DND_NAME)) == 0);

   /*
    * Blank out the resolutionInfo field, then copy user's arguments.
    */
   memset(resInfo, 0, sizeof *resInfo);

   strncpy(resInfo->tcloChannel, tcloChannel, sizeof resInfo->tcloChannel);
   resInfo->tcloChannel[sizeof resInfo->tcloChannel - 1] = '\0';

   if (!ResolutionBackendInit(handle)) {
      return FALSE;
   }

   resInfo->initialized = TRUE;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_Cleanup --
 *
 *      Shutdown the library, free resources, etc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Resolution_* calls will fail until user next calls Resolution_Init().
 *
 *-----------------------------------------------------------------------------
 */

void
Resolution_Cleanup(void)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized) {
      return;
   }

   Resolution_UnregisterCaps();
   Resolution_CleanupBackdoor();
   ResolutionBackendCleanup();

   ASSERT(!resInfo->cbResolutionRegistered);
   ASSERT(!resInfo->cbTopologyRegistered);
   ASSERT(!resInfo->rpcIn);
}


/*-----------------------------------------------------------------------------
 *
 * Resolution_InitBackdoor --
 *
 *      Register RpcIn callbacks for supported/available RpcIn commands.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Resolution_Set & DisplayTopology_Set callbacks may be registered.
 *
 *-----------------------------------------------------------------------------
 */

void
Resolution_InitBackdoor(RpcIn *rpcIn)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   ASSERT(resInfo->initialized);
   ASSERT(rpcIn);

   resInfo->rpcIn = rpcIn;

   if (resInfo->canSetResolution) {
      RpcIn_RegisterCallbackEx(resInfo->rpcIn, "Resolution_Set",
                               ResolutionResolutionSetCB, NULL);
      resInfo->cbResolutionRegistered = TRUE;
   }

   if (resInfo->canSetTopology) {
      RpcIn_RegisterCallbackEx(resInfo->rpcIn, "DisplayTopology_Set",
                               ResolutionDisplayTopologySetCB, NULL);
      resInfo->cbTopologyRegistered = TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_CleanupBackdoor --
 *
 *      Unregisters RpcIn callbacks.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Resolution callbacks are removed.
 *
 *-----------------------------------------------------------------------------
 */

void
Resolution_CleanupBackdoor(void)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized || !resInfo->rpcIn) {
      return;
   }

   if (resInfo->cbResolutionRegistered) {
      RpcIn_UnregisterCallback(resInfo->rpcIn, "Resolution_Set");
      resInfo->cbResolutionRegistered = FALSE;
   }

   if (resInfo->cbTopologyRegistered) {
      RpcIn_UnregisterCallback(resInfo->rpcIn, "DisplayTopology_Set");
      resInfo->cbTopologyRegistered = FALSE;
   }

   resInfo->rpcIn = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_RegisterCaps --
 *
 *      Register the "Resolution_Set" capability. Sometimes this needs to
 *      be done separately from the TCLO callback registration, so we
 *      provide it separately here.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Resolution_RegisterCaps(void)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   if (!resInfo->initialized) {
      return FALSE;
   }

   if (resInfo->canSetResolution) {
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 1")) {
         Debug("%s: Unable to register resolution set capability\n",
               __FUNCTION__);
         return FALSE;
      }

      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 1",
                          resInfo->tcloChannel)) {
         Debug("%s: Unable to register resolution server capability\n",
               __FUNCTION__);

         /*
          * Note that we do not return false so that we stay backwards
          * compatible with old vmx code (Workstation 6/ESX 3.5) that doesn't
          * handle resolution_server.
          */
      }
   }

   if (resInfo->canSetTopology) {
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 2")) {
         Debug("%s: Unable to register topology set capability\n",
               __FUNCTION__);
      }

      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 1")) {
         Debug("%s: Unable to register topology global offset capability\n",
               __FUNCTION__);
         /*
          * Ignore failures - host may not support these RPCs.
          */
      }
   }

   return TRUE;
}




/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_UnregisterCaps --
 *
 *      Unregister the "Resolution_Set" and "DisplayTopology_Set" capabilities.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Resolution_UnregisterCaps(void)
{
   // Shorter-named convenience alias.  I expect this to be optimized out.
   ResolutionInfoType *resInfo = &resolutionInfo;

   /*
    * RpcIn doesn't have an unregister facility, so all we need to do
    * here is unregister the capability.
    */

   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 0")) {
      Debug("%s: Unable to unregister ResolutionSet capability\n",
	    __FUNCTION__);
      return FALSE;
   }

   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 0",
		       resInfo->tcloChannel)) {
      Debug("%s: Unable to unregister resolution server capability\n",
	    __FUNCTION__);

      /*
       * Don't return false here so that an older vmx (Workstation 6/ESX 3.5)
       * that that supports resolution_set and not resolution_server will
       * still work.
       */
   }

   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 0") ||
       !RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 0")) {
      Debug("%s: Unable to unregister TopologySet capability\n",
	    __FUNCTION__);
      /*
       * Ignore failures - host may not support these RPCs.
       */
   }

   return TRUE;
}


/*
 * Local function definitions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionResolutionSetCB  --
 *
 *      Handler for TCLO 'Resolution_Set'.
 *
 * Results:
 *      TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ResolutionResolutionSetCB(RpcInData *data)      // IN/OUT:
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


/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionDisplayTopologySetCB  --
 *
 *      Handler for TCLO 'DisplayTopology_Set'.
 *
 *      Routine unmarshals RPC arguments and passes over to back-end
 *      TopologySet().
 *
 * Results:
 *      TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ResolutionDisplayTopologySetCB(RpcInData *data) // IN/OUT:
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

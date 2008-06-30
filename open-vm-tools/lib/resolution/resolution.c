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
#include "vm_app.h"
#include "debug.h"
#include "rpcout.h"
#include "str.h"
#include "strutil.h"

#include "resolutionInt.h"
#include "resolution.h"


/*
 * Internal global variables
 */

Bool     gCanSetResolution;     // Initialized via ResolutionBackendInit()
Bool     gCanSetTopology;       // Initialized via ResolutionBackendInit()

/*
 * Must either be TOOLS_DAEMON_NAME or TOOLS_DND_NAME.  See vm_app.h.
 */
static char     gTcloChannel[MAX(sizeof TOOLS_DAEMON_NAME,
                                 sizeof TOOLS_DND_NAME)];


/*
 * Local function prototypes
 */

static Bool     ResolutionRpcInSetCB(char const **result, size_t *resultLen,
                                     const char *name, const char *args,
                                     size_t argsSize, void *clientData);
static Bool     TopologyRpcInSetCB(char const **result, size_t *resultLen,
                                   const char *name, const char *args,
                                   size_t argsSize, void *clientData);


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
Resolution_Init(const char *tcloChannel,        // IN:
                InitHandle handle)              // IN:
{
   gCanSetResolution = FALSE;
   gCanSetTopology = FALSE;

   ASSERT((strcmp(tcloChannel, TOOLS_DAEMON_NAME) == 0) ||
          (strcmp(tcloChannel, TOOLS_DND_NAME)) == 0);

   if (!ResolutionBackendInit(handle)) {
      return FALSE;
   }

   strncpy(gTcloChannel, tcloChannel, sizeof gTcloChannel);
   gTcloChannel[sizeof gTcloChannel - 1] = '\0';

   return TRUE;
}


/*-----------------------------------------------------------------------------
 *
 * Resolution_Register --
 *
 *      Register the capability and resolution setting callbacks.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Resolution_Register(RpcIn *rpcIn)
{
   if (!rpcIn) {
      return FALSE;
   }

   if (gCanSetResolution) {
      RpcIn_RegisterCallback(rpcIn, "Resolution_Set", ResolutionRpcInSetCB, NULL);
   }

   if (gCanSetTopology) {
      RpcIn_RegisterCallback(rpcIn, "DisplayTopology_Set", TopologyRpcInSetCB, NULL);
   }

   return Resolution_RegisterCapability();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_RegisterCapability --
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
Resolution_RegisterCapability(void)
{
   if (gCanSetResolution) {
      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 1")) {
         Debug("%s: Unable to register resolution set capability\n",
               __FUNCTION__);
         return FALSE;
      }

      if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 1",
                          gTcloChannel)) {
         Debug("%s: Unable to register resolution server capability\n",
               __FUNCTION__);

         /* 
          * Note that we do not return false so that we stay backwards 
          * compatible with old vmx code (Workstation 6/ESX 3.5) that doesn't 
          * handle resolution_server.
          */
      }
   }

   if (gCanSetTopology) {
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
 * Resolution_UnregisterCapability --
 *
 *      Unregister the "Resolution_Set" capability.
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
Resolution_UnregisterCapability(void)
{
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
		       gTcloChannel)) {
      Debug("%s: Unable to unregister resolution server capability\n",
	    __FUNCTION__);

      /* 
       * Don't return false here so that an older vmx (Workstation 6/ESX 3.5)
       * that that supports resolution_set and not resolution_server will 
       * still work.
       */
   }

   if (gCanSetTopology &&
       (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 0")
        || !RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 0"))) {
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
 * ResolutionRpcInSetCB  --
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
ResolutionRpcInSetCB(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // IN
                     const char *args,        // IN
                     size_t argsSize,         // unused
                     void *clientData)        // unused

{
   uint32 width = 0 ;
   uint32 height = 0;
   unsigned int index = 0;

   /* parse the width and height */
   if (!StrUtil_GetNextUintToken(&width, &index, args, " ")) {
      goto invalid_arguments;
   }
   if (!StrUtil_GetNextUintToken(&height, &index, args, "")) {
      goto invalid_arguments;
   }

   return RpcIn_SetRetVals(result, resultLen, "",
                           ResolutionSetResolution(width, height));

invalid_arguments:
   return RpcIn_SetRetVals(result, resultLen, "Invalid arguments", FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TopologyRpcInSetCB  --
 *
 *      Handler for TCLO 'DisplayTopology_Set'.
 *
 *      Routine unmarshals RPC arguments and passes over to backend
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
TopologyRpcInSetCB(char const **result,     // OUT
                   size_t *resultLen,       // OUT
                   const char *name,        // IN
                   const char *args,        // IN
                   size_t argsSize,         // unused
                   void *clientData)        // unused
{
   DisplayTopologyInfo *displays = NULL;
   unsigned int count, i;
   Bool success = FALSE;

   /*
    * The argument string will look something like:
    *   <count> [ , <x> <y> <w> <h> ] * count.
    *
    * e.g.
    *    3 , 0 0 640 480 , 640 0 800 600 , 0 480 640 480
    */

   if (sscanf(args, "%u", &count) != 1) {
      return RpcIn_SetRetVals(result, resultLen,
                              "Invalid arguments. Expected \"count\"",
                              FALSE);
   }



   displays = malloc(sizeof *displays * count);
   if (!displays) {
      RpcIn_SetRetVals(result, resultLen,
                       "Failed to alloc buffer for display info",
                       FALSE);
      goto out;
   }

   for (i = 0; i < count; i++) {
      args = strchr(args, ',');
      if (!args) {
         RpcIn_SetRetVals(result, resultLen,
                          "Expected comma separated display list",
                          FALSE);
         goto out;
      }
      args++; /* Skip past the , */

      if (sscanf(args, " %d %d %d %d ", &displays[i].x,
                 &displays[i].y, &displays[i].width, &displays[i].height) != 4) {
         RpcIn_SetRetVals(result, resultLen,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }
   }

   success = ResolutionSetTopology(result, resultLen, count, displays);

   if (success) {
      RpcIn_SetRetVals(result, resultLen, "", TRUE);
   }

out:
   free(displays);
   return success;
}

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
 *     Set of functions to handle guest screen resizing for the vmwareuser.
 */

#include "vmwareuserInt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm_assert.h"
#include "vm_app.h"
#include "debug.h"
#include "fileIO.h"
#include "str.h"
#include "strutil.h"

#include "libvmwarectrl.h"

#define VMWAREDRV_PATH_64   "/usr/X11R6/lib64/modules/drivers/vmware_drv.o"
#define VMWAREDRV_PATH      "/usr/X11R6/lib/modules/drivers/vmware_drv.o"
#define VERSION_STRING      "VMware Guest X Server"

/*-----------------------------------------------------------------------------
 *
 * ResolutionCanSet --
 *
 *      Is the VMware SVGA driver a high enough version to support resolution
 *      changing? We check by searching the driver binary for a known version
 *      string.
 *
 * Results:
 *      TRUE if the driver version is high enough, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
ResolutionCanSet(void)
{
   FileIODescriptor fd;
   FileIOResult res;
   int64 filePos = 0;
   Bool keepSearching = TRUE;
   Bool found = FALSE;
   char buf[sizeof VERSION_STRING + 10]; // size of VERSION_STRING plus some extra for the version number
   const char versionString[] = VERSION_STRING;
   size_t bytesRead;
   int32 major, minor, level;
   unsigned int tokPos;

   /* See if the randr X module is loaded */
   if ( gXDisplay && !XRRQueryVersion(gXDisplay, &major, &minor) ) {
      return FALSE;
   }

   /* See if the VMWARE_CTRL extension is supported */
   gCanUseVMwareCtrl =
      gXDisplay ? VMwareCtrl_QueryVersion(gXDisplay, &major, &minor) : FALSE;

   if (gCanUseVMwareCtrl) {
      /*
       * We need both a new enough VMWARE_CTRL and Xinerama for this to work.
       */
#ifndef NO_MULTIMON
      gCanUseVMwareCtrlTopologySet = major >= 0 && minor >= 2 &&
                                     XineramaQueryVersion(gXDisplay, &major, &minor);
#endif

      return TRUE;
   } else {
      gCanUseVMwareCtrlTopologySet = FALSE;
   }

   /*
    * XXX: This check does not work with XOrg 6.9/7.0 for two reasons: Both
    * versions now use .so for the driver extension and 7.0 moves the drivers
    * to a completely different directory. As long as we ship a driver for
    * 6.9/7.0, we can instead just use the VMWARE_CTRL check.
    */
   buf[sizeof buf - 1] = '\0';
   FileIO_Invalidate(&fd);
   res = FileIO_Open(&fd, VMWAREDRV_PATH_64, FILEIO_ACCESS_READ, FILEIO_OPEN);
   if (res != FILEIO_SUCCESS) {
      res = FileIO_Open(&fd, VMWAREDRV_PATH, FILEIO_ACCESS_READ, FILEIO_OPEN);
   }
   if (res == FILEIO_SUCCESS) {
      /*
       * One of the opens succeeded, so start searching thru the file.
       */
      while (keepSearching) {
         res = FileIO_Read(&fd, buf, sizeof buf - 1, &bytesRead);
         if (res != FILEIO_SUCCESS || bytesRead < sizeof buf -1 ) {
            keepSearching = FALSE;
         } else {
            if (Str_Strncmp(versionString, buf, sizeof versionString - 1) == 0) {
               keepSearching = FALSE;
               found = TRUE;
            }
         }
         filePos = FileIO_Seek(&fd, filePos+1, FILEIO_SEEK_BEGIN);
         if (filePos == -1) {
            keepSearching = FALSE;
         }
      }
      FileIO_Close(&fd);
      if (found) {
         /*
          * We NUL-terminated buf earlier, but Coverity really wants it to
          * be NUL-terminated after the call to FileIO_Read (because
          * FileIO_Read doesn't NUL-terminate). So we'll do it again.
          */
         buf[sizeof buf - 1] = '\0';

         /*
          * Try and parse the major, minor and level versions
          */
         tokPos = sizeof versionString - 1;
         if (!StrUtil_GetNextIntToken(&major, &tokPos, buf, ".- ")) {
            return FALSE;
         }
         if (!StrUtil_GetNextIntToken(&minor, &tokPos, buf, ".- ")) {
            return FALSE;
         }
         if (!StrUtil_GetNextIntToken(&level, &tokPos, buf, ".- ")) {
            return FALSE;
         }

         return ((major > 10) || (major == 10 && minor >= 11));
      }
   }
   return FALSE;
}


/*-----------------------------------------------------------------------------
 *
 * ResolutionSet --
 *
 *      Given a width and height, find the biggest resolution that will "fit".
 *      This is called as a result of the resolution set request from the vmx.
 *
 * Results:
 *      TRUE if we are able to set to the exact size requested, FALSE otherwise.
 *
 * Side effects:
 *      The screen resolution of will change.
 *
 *-----------------------------------------------------------------------------
 */

Bool
ResolutionSet(uint32 width,  // IN
              uint32 height) // IN
{
   XRRScreenConfiguration* xrrConfig;
   XRRScreenSize *xrrSizes;
   Rotation xrrCurRotation;
   uint32  xrrNumSizes;
   uint32 i;
   uint32 bestFitIndex = 0;
   uint64 bestFitSize = 0;
   uint64 potentialSize;

   xrrConfig = XRRGetScreenInfo(gXDisplay, gXRoot);
   xrrSizes = XRRConfigSizes(xrrConfig, &xrrNumSizes);
   XRRConfigCurrentConfiguration(xrrConfig, &xrrCurRotation);

   /*
    * Iterate thru the list finding the best fit that is still <= in both width 
    * and height. 
    */
   for (i = 0; i < xrrNumSizes; i++) {
      potentialSize = xrrSizes[i].width * xrrSizes[i].height;
      if (xrrSizes[i].width <= width && xrrSizes[i].height <= height && 
          potentialSize > bestFitSize ) {
         bestFitSize = potentialSize;
         bestFitIndex = i;
      }
   }

   if (bestFitSize > 0) {
      Debug("Setting guest resolution to: %dx%d (requested: %d, %d)\n",
            xrrSizes[bestFitIndex].width, xrrSizes[bestFitIndex].height, width, height);
      XRRSetScreenConfig(gXDisplay, xrrConfig, gXRoot, bestFitIndex, xrrCurRotation,
                         GDK_CURRENT_TIME);
   } else {
      Debug("Can't find a suitable guest resolution, ignoring request for %dx%d\n",
            width, height);
   }

   XRRFreeScreenConfigInfo(xrrConfig);
   return xrrSizes[bestFitIndex].width == width && 
          xrrSizes[bestFitIndex].height == height;
}


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

Bool
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

   if (gCanUseVMwareCtrl) {
      /*
       * If so, set the current res with it. This means we'll get an exact
       * match when we search in ResolutionSet. (Unless the res is too large).
       *
       * As such, we don't care if this succeeds or fails, we'll make a best
       * effort attempt to change resolution anyway.
       */
      VMwareCtrl_SetRes(gXDisplay, DefaultScreen(gXDisplay), width, height);
   }

   return RpcIn_SetRetVals(result, resultLen, "", ResolutionSet(width, height));

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
 *      Solaris 10 uses a different Xinerama standard than expected here. As a
 *      result, topology set is not supported and this function is excluded from
 *      Solaris builds.
 *
 * Results:
 *      TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifndef NO_MULTIMON
Bool
TopologyRpcInSetCB(char const **result,     // OUT
                   size_t *resultLen,       // OUT
                   const char *name,        // IN
                   const char *args,        // IN
                   size_t argsSize,         // unused
                   void *clientData)        // unused

{
   Bool success = FALSE;
   uint32 count, i;
   xXineramaScreenInfo *displays = NULL;
   short maxX = 0;
   short maxY = 0;
   int minX = 0;
   int minY = 0;

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
   displays = (xXineramaScreenInfo *)malloc(sizeof *displays * count);
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

      if (sscanf(args, " %hd %hd %hd %hd ", &displays[i].x_org,
                 &displays[i].y_org, &displays[i].width, &displays[i].height) != 4) {
         RpcIn_SetRetVals(result, resultLen,
                          "Expected x, y, w, h in display entry",
                          FALSE);
         goto out;
      }
      maxX = MAX(maxX, displays[i].x_org + displays[i].width);
      maxY = MAX(maxY, displays[i].y_org + displays[i].height);
      minX = MIN(minX, displays[i].x_org);
      minY = MIN(minY, displays[i].y_org);
   }

   if (minX != 0 || minY != 0) {
      Warning("The bounding box of the display topology does not have an origin of (0,0)\n");
   }

   /*
    * Transform the topology so that the bounding box has an origin of (0,0). Since the
    * host is already supposed to pass a normalized topology, this should not have any
    * effect.
    */
   for (i = 0; i < count; i++) {
      displays[i].x_org -= minX;
      displays[i].y_org -= minY;
   }

   if (!VMwareCtrl_SetTopology(gXDisplay, DefaultScreen(gXDisplay), displays, count)) {
      RpcIn_SetRetVals(result, resultLen, "Failed to set topology in the driver.",
                       FALSE);
      goto out;
   }

   if (!ResolutionSet(maxX - minX, maxY - minY)) {
      RpcIn_SetRetVals(result, resultLen, "Failed to set new resolution.",
                       FALSE);
      goto out;
   }

   RpcIn_SetRetVals(result, resultLen, "", TRUE);
   success = TRUE;

out:
   free(displays);
   return success;
}
#endif


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
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_set 1")) {
      Debug("%s: Unable to register resolution set capability\n",
	    __FUNCTION__);
      return FALSE;
   }
   if (!RpcOut_sendOne(NULL, NULL, "tools.capability.resolution_server %s 1",
		       TOOLS_DND_NAME)) {
      Debug("%s: Unable to register resolution server capability\n",
	    __FUNCTION__);

      /* 
       * Note that we do not return false so that we stay backwards 
       * compatible with old vmx code (Workstation 6/ESX 3.5) that doesn't 
       * handle resolution_server.
       */
   }
#ifndef NO_MULTIMON
   if (gCanUseVMwareCtrlTopologySet &&
       !RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 1")) {
      Debug("%s: Unable to register topology set capability\n",
	    __FUNCTION__);
      return FALSE;
   }
   if (gCanUseVMwareCtrlTopologySet &&
       !RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 1")) {
      Debug("%s: Unable to register topology global offset capability\n",
	    __FUNCTION__);
      /*
       * Ignore failures - host may not support these RPCs.
       */
   }
#endif
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
Resolution_Register(void)
{
   if (!gRpcIn) {
      return FALSE;
   }
   if (!ResolutionCanSet()) {
      return FALSE;
   }

   RpcIn_RegisterCallback(gRpcIn, "Resolution_Set", ResolutionRpcInSetCB, NULL);
#ifndef NO_MULTIMON
   if (gCanUseVMwareCtrlTopologySet) {
      RpcIn_RegisterCallback(gRpcIn, "DisplayTopology_Set", TopologyRpcInSetCB, NULL);
   }
#endif
   if (!Resolution_RegisterCapability()) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Resolution_Unregister --
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
Resolution_Unregister(void)
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
		       TOOLS_DND_NAME)) {
      Debug("%s: Unable to unregister resolution server capability\n",
	    __FUNCTION__);

      /* 
       * Don't return false here so that an older vmx (Workstation 6/ESX 3.5)
       * that that supports resolution_set and not resolution_server will 
       * still work.
       */
   }
#ifndef NO_MULTIMON
   if (gCanUseVMwareCtrlTopologySet &&
       (!RpcOut_sendOne(NULL, NULL, "tools.capability.display_topology_set 0")
        || !RpcOut_sendOne(NULL, NULL, "tools.capability.display_global_offset 0"))) {
      Debug("%s: Unable to unregister TopologySet capability\n",
	    __FUNCTION__);
      /*
       * Ignore failures - host may not support these RPCs.
       */
   }
#endif

   return TRUE;
}

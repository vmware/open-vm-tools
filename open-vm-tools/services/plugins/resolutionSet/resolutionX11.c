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

/*
 * @file resolutionX11.c 
 *
 * X11 backend for resolutionSet plugin.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#ifndef NO_MULTIMON
#include <X11/extensions/Xinerama.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#undef Bool

#include "vmware.h"

#include "resolution.h"
#include "resolutionInt.h"

#include "debug.h"
#include "fileIO.h"
#include "libvmwarectrl.h"
#include "str.h"
#include "strutil.h"
#include "util.h"

#define VMWAREDRV_PATH_64   "/usr/X11R6/lib64/modules/drivers/vmware_drv.o"
#define VMWAREDRV_PATH      "/usr/X11R6/lib/modules/drivers/vmware_drv.o"
#define VERSION_STRING      "VMware Guest X Server"

#define RR12_OUTPUT_FORMAT "LVDS%u"
#define RR12_MODE_FORMAT "autofit-%ux%u"
#define RR12_MODE_MAXLEN (sizeof RR12_MODE_FORMAT + 2 * (10 - 2) + 1)
#define RR12_DEFAULT_DPI 96.0
#define MILLIS_PER_INCH 25.4


/**
 * Describes the state of the X11 back-end of lib/resolution.
 */
typedef struct {
   Display      *display;       // X11 connection / display context
   Window       rootWindow;     // points to display's root window
   Bool         canUseVMwareCtrl;
                                // TRUE if VMwareCtrl extension available
   Bool         canUseVMwareCtrlTopologySet;
                                // TRUE if VMwareCtrl extension supports topology set
   Bool         canUseRandR12;  // TRUE if RandR extension >= 1.2 available
   unsigned int topologyDisplays;
                                // Number of displays in current topology
   unsigned int topologyWidth;  // Total width of current topology
   unsigned int topologyHeight; // Total height of current topology
} ResolutionInfoX11Type;


/*
 * Global variables
 */

ResolutionInfoX11Type resolutionInfoX11;

/*
 * Local function prototypes
 */

static Bool ResolutionCanSet(void);
static Bool TopologyCanSet(void);
#ifndef NO_MULTIMON
static Bool RandR12_SetTopology(unsigned int ndisplays,
                                xXineramaScreenInfo *displays,
                                unsigned int width, unsigned int height);
#endif
static Bool SelectResolution(uint32 width, uint32 height);


/*
 * Global function definitions
 */


/**
 * X11 back-end initializer.  Records caller's X11 display, then determines
 * which capabilities are available.
 *
 * @param[in] handle User's X11 display
 * @return TRUE on success, FALSE on failure.
 */

Bool
ResolutionBackendInit(InitHandle handle)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   ResolutionInfoType *resInfo = &resolutionInfo;
   int dummy1;
   int dummy2;

   memset(resInfoX, 0, sizeof *resInfoX);

   resInfoX->display = handle;

   if (resInfoX->display == NULL) {
      Warning("%s: Called with invalid X display!\n", __func__);
      return FALSE;
   }

   resInfoX->display = handle;
   resInfoX->rootWindow = DefaultRootWindow(resInfoX->display);
   resInfoX->canUseVMwareCtrl = VMwareCtrl_QueryVersion(resInfoX->display, &dummy1,
                                                        &dummy2);
   resInfoX->canUseVMwareCtrlTopologySet = FALSE;
   resInfoX->canUseRandR12 = FALSE;

   resInfo->canSetResolution = ResolutionCanSet();
   resInfo->canSetTopology = TopologyCanSet();

   return TRUE;
}


/**
 * Stub implementation of ResolutionBackendCleanup for the X11 back-end.
 */

void
ResolutionBackendCleanup(void)
{
   return;
}


/**
 * Given a width and height, define a custom resolution (if VMwareCtrl is
 * available), then issue a change resolution request via XRandR.
 *
 * This is called as a result of the Resolution_Set request from the vmx.
 *
 * @param[in] width requested width
 * @param[in] height requested height
 * @return TRUE if we are able to set to the exact size requested, FALSE otherwise.
 */

Bool
ResolutionSetResolution(uint32 width,
                        uint32 height)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   ASSERT(resolutionInfo.canSetResolution);

   if (resInfoX->canUseVMwareCtrl) {
      /*
       * If so, use the VMWARE_CTRL extension to provide a custom resolution
       * which we'll find as an exact match from XRRConfigSizes() (unless
       * the resolution is too large).
       *
       * As such, we don't care if this succeeds or fails, we'll make a best
       * effort attempt to change resolution anyway.
       */
      VMwareCtrl_SetRes(resInfoX->display, DefaultScreen(resInfoX->display),
                        width, height);
   }

   return SelectResolution(width, height);
}


/**
 * Employs the Xinerama extension to declare a new display topology.
 *
 * @note Solaris 10 uses a different Xinerama standard than expected here. As a
 * result, topology set is not supported and this function is excluded from
 * Solaris builds. With Solaris 10 shipping X.org, perhaps we should revisit 
 * this decision.
 *
 * @param[in] ndisplays number of elements in topology
 * @param[in] topology array of display geometries
 * @return TRUE if operation succeeded, FALSE otherwise.
 */

Bool
ResolutionSetTopology(unsigned int ndisplays,
                      DisplayTopologyInfo *topology)
{
#ifdef NO_MULTIMON
   return FALSE;
#else
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   Bool success = FALSE;
   unsigned int i;
   xXineramaScreenInfo *displays = NULL;
   short maxX = 0;
   short maxY = 0;
   int minX = 0;
   int minY = 0;

   ASSERT(resolutionInfo.canSetTopology);

   /*
    * Allocate xXineramaScreenInfo array & translate from DisplayTopologyInfo.
    * Iterate over displays looking for minimum, maximum dimensions.
    * Warn if min isn't at (0,0).
    * Transform to (0,0).
    * Call out to VMwareCtrl_SetTopology.
    * Set new jumbotron resolution.
    */

   displays = malloc(sizeof *displays * ndisplays);
   if (!displays) {
      goto out;
   }

   for (i = 0; i < ndisplays; i++) {
      displays[i].x_org = topology[i].x;
      displays[i].y_org = topology[i].y;
      displays[i].width = topology[i].width;
      displays[i].height = topology[i].height;

      maxX = MAX(maxX, displays[i].x_org + displays[i].width);
      maxY = MAX(maxY, displays[i].y_org + displays[i].height);
      minX = MIN(minX, displays[i].x_org);
      minY = MIN(minY, displays[i].y_org);
   }

   if (minX != 0 || minY != 0) {
      Warning("The bounding box of the display topology does not have an "
              "origin of (0,0)\n");
   }

   /*
    * Transform the topology so that the bounding box has an origin of (0,0). Since the
    * host is already supposed to pass a normalized topology, this should not have any
    * effect.
    */
   for (i = 0; i < ndisplays; i++) {
      displays[i].x_org -= minX;
      displays[i].y_org -= minY;
   }

   if (resInfoX->canUseVMwareCtrlTopologySet) {
      if (!VMwareCtrl_SetTopology(resInfoX->display, DefaultScreen(resInfoX->display),
                                  displays, ndisplays)) {
         Debug("Failed to set topology in the driver.\n");
         goto out;
      }

      if (!SelectResolution(maxX - minX, maxY - minY)) {
         Debug("Failed to set new resolution.\n");
         goto out;
      }

      success = TRUE;
   } else if (resInfoX->canUseRandR12) {
      success = RandR12_SetTopology(ndisplays, displays,
                                    maxX - minX, maxY - minY);
   }

out:
   free(displays);
   return success;
#endif
}


/*
 * Local function definitions
 */


/**
 * Is the VMware SVGA driver a high enough version to support resolution
 * changing? We check by searching the driver binary for a known version
 * string.
 *
 * @return TRUE if the driver version is high enough, FALSE otherwise.
 */

static Bool
ResolutionCanSet(void)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
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
   if (!XRRQueryVersion(resInfoX->display, &major, &minor) ) {
      return FALSE;
   }

   /* See if the VMWARE_CTRL extension is supported */
   if (resInfoX->canUseVMwareCtrl) {
      return TRUE;
   }

#ifndef NO_MULTIMON
   /* See if RandR >= 1.2 can be used: The extension version is high enough and
    * all output names match the expected format.
    */
   if (major > 1 || (major == 1 && minor >= 2)) {
      XRRScreenResources* xrrRes;
      XRROutputInfo* xrrOutput;
      unsigned int num;
      int i;

      XGrabServer(resInfoX->display);

      xrrRes = XRRGetScreenResources(resInfoX->display, resInfoX->rootWindow);

      if (xrrRes) {
         for (i = 0; i < xrrRes->noutput; i++) {
            xrrOutput = XRRGetOutputInfo(resInfoX->display, xrrRes,
                                         xrrRes->outputs[i]);
            if (!xrrOutput) {
               break;
            }

            if (sscanf(xrrOutput->name, RR12_OUTPUT_FORMAT, &num) != 1 ||
                num < 1) {
               XRRFreeOutputInfo(xrrOutput);
               break;
            }

            XRRFreeOutputInfo(xrrOutput);
         }

         if (i == xrrRes->noutput) {
            resInfoX->canUseRandR12 = TRUE;
         } else {
            Debug("RandR >= 1.2 not usable\n");
         }

         XRRFreeScreenResources(xrrRes);
      }

      XUngrabServer(resInfoX->display);

      if (resInfoX->canUseRandR12) {
         return TRUE;
      }
   }
#endif // ifndef NO_MULTIMON

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


/**
 * Tests whether or not we can change display topology.
 *
 * @return TRUE if we're able to reset topology, otherwise FALSE.
 * @note resInfoX->canUseVMwareCtrlTopologySet will be set to TRUE on success.
 */

static Bool
TopologyCanSet(void)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
#ifdef NO_MULTIMON
   resInfoX->canUseVMwareCtrlTopologySet = FALSE;
   return FALSE;
#else
   int major;
   int minor;

   if (resInfoX->canUseRandR12) {
      return TRUE;
   }

   if (resInfoX->canUseVMwareCtrl && XineramaQueryVersion(resInfoX->display, &major,
                                                          &minor)) {
      /*
       * We need both a new enough VMWARE_CTRL and Xinerama for this to work.
       */
      resInfoX->canUseVMwareCtrlTopologySet = (major > 0) || (major == 0 && minor >= 2);
   } else {
      resInfoX->canUseVMwareCtrlTopologySet = FALSE;
   }

   return resInfoX->canUseVMwareCtrlTopologySet;
#endif
}


#ifndef NO_MULTIMON

/**
 * Employs the RandR 1.2 extension to set a new display topology.
 *
 * @return TRUE if operation succeeded, FALSE otherwise.
 */

static Bool
RandR12_SetTopology(unsigned int ndisplays,
                    // IN:  number of elements in topology
                    xXineramaScreenInfo *displays,
                    // IN: array of display geometries
                    unsigned int width,
                    // IN: total width of topology
                    unsigned int height)
                    // IN: total height of topology
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   int minWidth, minHeight, maxWidth, maxHeight;
   XRRScreenResources* xrrRes = NULL;
   XRROutputInfo** xrrOutputs;
   unsigned int numOutputs;
   unsigned int* outputMap;
   XRRCrtcInfo** xrrCrtcs;
   XRRScreenConfiguration* xrrConfig = NULL;
   XRRScreenSize* xrrSizes;
   Rotation xrrCurRotation;
   uint32 xrrNumSizes;
   SizeID currentSize;
   XRRModeInfo xrrModes[ndisplays];
   char name[RR12_MODE_MAXLEN];
   float dpi;
   int i, j, k;
   Bool success = FALSE;

   if (!XRRGetScreenSizeRange(resInfoX->display, resInfoX->rootWindow,
                              &minWidth, &minHeight, &maxWidth, &maxHeight) ||
       width < minWidth || height < minHeight ||
       width > maxWidth || height > maxHeight) {
      return FALSE;
   }

   /* Grab the server for two reasons:
    * - Avoid race conditions with other clients changing RandR configuration.
    * - Make our changes appear as atomic as possible to other clients.
    */
   XGrabServer(resInfoX->display);

   xrrRes = XRRGetScreenResources(resInfoX->display, resInfoX->rootWindow);
   if (!xrrRes) {
      goto error;
   }

   xrrCrtcs = Util_SafeCalloc(sizeof *xrrCrtcs, xrrRes->ncrtc);
   xrrOutputs = Util_SafeCalloc(sizeof *xrrOutputs, xrrRes->noutput);
   outputMap = Util_SafeCalloc(sizeof *outputMap, xrrRes->noutput);

   /* RandR may enumerate outputs differently from the host. Apply the nth
    * topology rectangle to the output called LVDS<n>.
    */
   for (i = 0; i < xrrRes->noutput; i++) {
      outputMap[i] = i;
   }

   numOutputs = 0;
   for (i = 0; i < xrrRes->noutput; i++) {
      unsigned int num;

      xrrOutputs[i] = XRRGetOutputInfo(resInfoX->display, xrrRes,
                                       xrrRes->outputs[i]);

      if (!xrrOutputs[i]) {
         goto error;
      }

      if (sscanf(xrrOutputs[i]->name, RR12_OUTPUT_FORMAT, &num) != 1 ||
          num > ndisplays) {
         continue;
      }

      outputMap[num - 1] = i;

      if (num > numOutputs) {
         numOutputs = num;
      }
   }

   /* Disable any CRTCs which won't be used or wont't fit in new screen size. */
   for (i = 0; i < xrrRes->ncrtc; i++) {
      xrrCrtcs[i] = XRRGetCrtcInfo(resInfoX->display, xrrRes,
                                   xrrRes->crtcs[i]);

      if (!xrrCrtcs[i]) {
         goto error;
      }

      for (j = 0; j < numOutputs; j++) {
         if (xrrOutputs[outputMap[j]]->crtc == xrrRes->crtcs[i]) {
            break;
         }
      }

      if (xrrCrtcs[i]->mode == None ||
          (j < numOutputs &&
           (xrrCrtcs[i]->x + xrrCrtcs[i]->width) <= width &&
           (xrrCrtcs[i]->y + xrrCrtcs[i]->height) <= height)) {
         continue;
      }

      if (XRRSetCrtcConfig(resInfoX->display, xrrRes, xrrRes->crtcs[i],
                           xrrCrtcs[i]->timestamp, 0, 0, None, RR_Rotate_0, NULL, 0)
          != Success) {
         goto error;
      }
   }

   /* Set new screen size. */
   xrrConfig = XRRGetScreenInfo(resInfoX->display, resInfoX->rootWindow);
   xrrSizes = XRRConfigSizes(xrrConfig, &xrrNumSizes);
   currentSize = XRRConfigCurrentConfiguration(xrrConfig, &xrrCurRotation);

   if (xrrSizes[currentSize].mheight > 0) {
      dpi = MILLIS_PER_INCH * xrrSizes[currentSize].height / xrrSizes[currentSize].mheight;

      if (!dpi) {
         dpi = RR12_DEFAULT_DPI;
      }
   } else {
      dpi = RR12_DEFAULT_DPI;
   }

   XRRSetScreenSize(resInfoX->display, resInfoX->rootWindow, width, height,
                    (MILLIS_PER_INCH * width) / dpi,
                    (MILLIS_PER_INCH * height) / dpi);

   /* Set new topology. */
   for (i = 0; i < numOutputs; i++) {
      memset(xrrModes + i, 0, sizeof xrrModes[0]);

      xrrModes[i].width = displays[i].width;
      xrrModes[i].height = displays[i].height;

      /* Look for existing matching autofit mode. */
      for (j = 0; j < i && !xrrModes[i].id; j++) {
         if (xrrModes[j].id &&
             xrrModes[j].width == displays[i].width &&
             xrrModes[j].height == displays[i].height) {
            xrrModes[i].id = xrrModes[j].id;
            break;
         }
      }

      for (j = 0; j < xrrRes->nmode && !xrrModes[i].id; j++) {
         unsigned int w, h;

         if (sscanf(xrrRes->modes[j].name, RR12_MODE_FORMAT, &w, &h) == 2 &&
             w == displays[i].width && h == displays[i].height) {
            xrrModes[i].id = xrrRes->modes[j].id;
            break;
         }
      }

      /* If no luck, create new autofit mode. */
      if (!xrrModes[i].id) {
         Str_Sprintf(name, sizeof name, RR12_MODE_FORMAT, displays[i].width,
                     displays[i].height);
         xrrModes[i].name = name;
         xrrModes[i].nameLength = strlen(xrrModes[i].name);
         xrrModes[i].id = XRRCreateMode(resInfoX->display, resInfoX->rootWindow,
                                        xrrModes + i);
      }

      if (xrrModes[i].id == None) {
         continue;
      }

      /* Set autofit mode. */
      if (xrrOutputs[outputMap[i]]->crtc == None) {
         xrrOutputs[outputMap[i]]->crtc = xrrOutputs[outputMap[i]]->crtcs[0];
      }

      for (j = 0; j < xrrOutputs[outputMap[i]]->nmode; j++) {
         if (xrrModes[i].id == xrrOutputs[outputMap[i]]->modes[j]) {
            break;
         }
      }
      if (j == xrrOutputs[outputMap[i]]->nmode) {
         XRRAddOutputMode(resInfoX->display, xrrRes->outputs[outputMap[i]],
                          xrrModes[i].id);
      }
      if (XRRSetCrtcConfig(resInfoX->display, xrrRes,
                           xrrOutputs[outputMap[i]]->crtc, xrrCrtcs[i]->timestamp,
                           displays[i].x_org, displays[i].y_org, xrrModes[i].id,
                           RR_Rotate_0, xrrRes->outputs + outputMap[i], 1)
          != Success) {
         goto error;
      }
   }

   /* Delete unused autofit modes. */
   for (i = 0; i < xrrRes->nmode; i++) {
      unsigned int w, h;
      Bool destroy = TRUE;

      if (sscanf(xrrRes->modes[i].name, RR12_MODE_FORMAT, &w, &h) != 2) {
         continue;
      }

      for (j = 0; j < xrrRes->noutput; j++) {
         if (j < numOutputs &&
             w == displays[j].width && h == displays[j].height) {
            destroy = FALSE;
            continue;
         }

         for (k = 0; k < xrrOutputs[outputMap[j]]->nmode; k++) {
            if (xrrOutputs[outputMap[j]]->modes[k] == xrrRes->modes[i].id) {
               XRRDeleteOutputMode(resInfoX->display,
                                   xrrRes->outputs[outputMap[j]],
                                   xrrOutputs[outputMap[j]]->modes[k]);
               break;
            }
         }
      }

      if (destroy) {
         XRRDestroyMode(resInfoX->display, xrrRes->modes[i].id);
      }
   }

   resInfoX->topologyDisplays = ndisplays;
   resInfoX->topologyWidth = width;
   resInfoX->topologyHeight = height;

   success = TRUE;

error:
   XUngrabServer(resInfoX->display);

   if (xrrConfig) {
      XRRFreeScreenConfigInfo(xrrConfig);
   }
   if (xrrRes) {
      for (i = 0; i < xrrRes->noutput; i++) {
         if (xrrOutputs[i]) {
            XRRFreeOutputInfo(xrrOutputs[i]);
         }
      }
      for (i = 0; i < xrrRes->ncrtc; i++) {
         if (xrrCrtcs[i]) {
            XRRFreeCrtcInfo(xrrCrtcs[i]);
         }
      }
      free(outputMap);
      free(xrrOutputs);
      free(xrrCrtcs);
      XRRFreeScreenResources(xrrRes);
   }

   return success;
}

#endif // ifndef NO_MULTIMON


/**
 * Given a width and height, find the biggest resolution that will "fit".
 * This is called as a result of the resolution set request from the vmx.
 *
 * @param[in] width 
 * @param[in] height
 * @return TRUE if we are able to set to the exact size requested, FALSE otherwise.
 */

Bool
SelectResolution(uint32 width,
                 uint32 height)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   XRRScreenConfiguration* xrrConfig;
   XRRScreenSize *xrrSizes;
   Rotation xrrCurRotation;
   uint32  xrrNumSizes;
   uint32 i;
   uint32 bestFitIndex = 0;
   uint64 bestFitSize = 0;
   uint64 potentialSize;

#ifndef NO_MULTIMON
   if (resInfoX->canUseRandR12) {
      if (resInfoX->topologyDisplays != 1 ||
          resInfoX->topologyWidth != width ||
          resInfoX->topologyHeight != height) {
         xXineramaScreenInfo display;

         display.x_org = 0;
         display.y_org = 0;
         display.width = width;
         display.height = height;

         return RandR12_SetTopology(1, &display, width, height);
      }

      return TRUE;
   }
#endif

   xrrConfig = XRRGetScreenInfo(resInfoX->display, resInfoX->rootWindow);
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
      XRRSetScreenConfig(resInfoX->display, xrrConfig, resInfoX->rootWindow,
                         bestFitIndex, xrrCurRotation, GDK_CURRENT_TIME);
   } else {
      Debug("Can't find a suitable guest resolution, ignoring request for %dx%d\n",
            width, height);
   }

   XRRFreeScreenConfigInfo(xrrConfig);
   return xrrSizes[bestFitIndex].width == width &&
          xrrSizes[bestFitIndex].height == height;
}

/**
 * Obtain a "handle", which for X11, is a display pointer. 
 *
 * @note We will have to move this out of the resolution plugin soon, I am
 * just landing this here now for convenience as I port resolution set over 
 * to the new service architecture.
 *
 * @return X server display 
 */

InitHandle
ResolutionToolkitInit(void)
{
   int argc = 1;
   char *argv[] = {"", NULL};
   GtkWidget *wnd;
   Display *display;
   gtk_init(&argc, (char ***) &argv);
   wnd = gtk_invisible_new();
   display = GDK_WINDOW_XDISPLAY(wnd->window);
   return (InitHandle) display;
}

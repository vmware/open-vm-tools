/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
#include <fcntl.h>
#include <unistd.h>

#include "resolutionInt.h"
#include "resolutionRandR12.h"

#include <X11/extensions/Xrandr.h>
#ifndef NO_MULTIMON
#include <X11/extensions/Xinerama.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "vmware.h"
#include "debug.h"
#include "libvmwarectrl.h"
#include "str.h"
#include "strutil.h"
#include "util.h"
#include "posix.h"

#define VMWAREDRV_PATH_64   "/usr/X11R6/lib64/modules/drivers/vmware_drv.o"
#define VMWAREDRV_PATH      "/usr/X11R6/lib/modules/drivers/vmware_drv.o"
#define VERSION_STRING      "VMware Guest X Server"

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
      g_warning("%s: Called with invalid X display!\n", __func__);
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
   Bool ret;

   ASSERT(resolutionInfo.canSetResolution);

   XGrabServer(resInfoX->display);
   if (resInfoX->canUseVMwareCtrl) {
      /*
       * If so, use the VMWARE_CTRL extension to provide a custom resolution
       * which we'll find as an exact match from XRRConfigSizes() (unless
       * the resolution is too large).
       *
       * As such, we don't care if this succeeds or fails, we'll make a best
       * effort attempt to change resolution anyway.
       *
       * On vmwgfx, this is routed through the X server down to the
       * kernel modesetting system to provide a preferred mode with
       * correcte width and height.
       */
      VMwareCtrl_SetRes(resInfoX->display, DefaultScreen(resInfoX->display),
			width, height);
   }

   /*
    * Use legacy RandR (vmwlegacy) or RandR12 (vmwgfx) to select the
    * desired mode.
    */
   ret = SelectResolution(width, height);
   XUngrabServer(resInfoX->display);
   XFlush(resInfoX->display);

   return ret;
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
   int minX = 0x7FFF;
   int minY = 0x7FFF;

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
      g_warning("The bounding box of the display topology does not have an "
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

   /*
    * Grab server to avoid potential races between setting GUI topology
    * and setting FB topology.
    */
   XGrabServer(resInfoX->display);

   /*
    * First, call vmwarectrl to update the connection info
    * and resolution capabilities of connected monitors,
    * according to the host GUI layout on vmwgfx. On vmwlegacy this
    * sets the driver's exported Xinerama topology.
    *
    * For vmwgfx, this might be replaced with a direct kernel driver call
    * in upcoming versions.
    */
   if (resInfoX->canUseVMwareCtrlTopologySet) {
      if (!VMwareCtrl_SetTopology(resInfoX->display,
				  DefaultScreen(resInfoX->display),
                                  displays, ndisplays)) {
         g_debug("Failed to set topology in the driver.\n");
         goto out;
      }
   }

   if (resInfoX->canUseRandR12) {
       /*
	* For vmwgfx, use RandR12 to set the FB layout to a 1:1 mapping
	* of the host GUI layout.
	*/
      success = RandR12_SetTopology(resInfoX->display,
                                    DefaultScreen(resInfoX->display),
                                    resInfoX->rootWindow,
                                    ndisplays, displays,
                                    maxX - minX, maxY - minY);
   } else if (resInfoX->canUseVMwareCtrlTopologySet) {
      /*
       * For vmwlegacy, use legacy RandR to set the backing framebuffer
       * size. We don't do this unless we were able to set a new
       * topology using vmwarectrl.
       */
      if (!SelectResolution(maxX - minX, maxY - minY)) {
         g_debug("Failed to set new resolution.\n");
         goto out;
      }

      success = TRUE;
   }

out:
   XUngrabServer(resInfoX->display);
   XFlush(resInfoX->display);

   free(displays);
   return success;
#endif
}


/*
 * Local function definitions
 */


/**
 * Does VMware SVGA driver support resolution changing? We check by
 * testing RandR version and the availability of VMWCTRL extension. It
 * also check the output names for RandR 1.2 and above which is used for
 * the vmwgfx driver. Finally it searches the driver binary for a known
 * version string.
 *
 * resInfoX->canUseRandR12 will be set if RandR12 is usable.
 *
 * @return TRUE if the driver version is high enough, FALSE otherwise.
 */

static Bool
ResolutionCanSet(void)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;
   int fd = -1;
   off_t filePos = 0;
   Bool keepSearching = TRUE;
   Bool found = FALSE;
   char buf[sizeof VERSION_STRING + 10]; // size of VERSION_STRING plus some extra for the version number
   const char versionString[] = VERSION_STRING;
   ssize_t bytesRead;
   int32 major, minor, level;
   unsigned int tokPos;

   /* See if the randr X module is loaded */
   if (!XRRQueryVersion(resInfoX->display, &major, &minor) ) {
      return FALSE;
   }

#ifndef NO_MULTIMON
   /*
    * See if RandR >= 1.2 can be used: The extension version is high enough and
    * all output names match the expected format.
    */
   if (major > 1 || (major == 1 && minor >= 2)) {
      XRRScreenResources* xrrRes;
      XRROutputInfo* xrrOutput;
      unsigned int num;
      int i;

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
            g_debug("RandR >= 1.2 not usable\n");
         }

         XRRFreeScreenResources(xrrRes);
      }

      if (resInfoX->canUseRandR12) {
         return TRUE;
      }
   }

#endif // ifndef NO_MULTIMON

   /*
    * See if the VMWARE_CTRL extension is supported.
    */

   if (resInfoX->canUseVMwareCtrl) {
      return TRUE;
   }

   /*
    * XXX: This check does not work with XOrg 6.9/7.0 for two reasons: Both
    * versions now use .so for the driver extension and 7.0 moves the drivers
    * to a completely different directory. As long as we ship a driver for
    * 6.9/7.0, we can instead just use the VMWARE_CTRL check.
    */

   buf[sizeof buf - 1] = '\0';
   fd = Posix_Open(VMWAREDRV_PATH_64, O_RDONLY);
   if (fd == -1) {
      fd = Posix_Open(VMWAREDRV_PATH, O_RDONLY);
   }
   if (fd != -1) {
      /*
       * One of the opens succeeded, so start searching thru the file.
       */
      while (keepSearching) {
         bytesRead = read(fd, buf, sizeof buf - 1);
         if (bytesRead == -1 || bytesRead < sizeof buf -1 ) {
            keepSearching = FALSE;
         } else {
            if (Str_Strncmp(versionString, buf, sizeof versionString - 1) == 0) {
               keepSearching = FALSE;
               found = TRUE;
            }
         }
         filePos = lseek(fd, filePos+1, SEEK_SET);
         if (filePos == -1) {
            keepSearching = FALSE;
         }
      }
      close(fd);
      if (found) {
         /*
          * We NUL-terminated buf earlier, but Coverity really wants it to
          * be NUL-terminated after the call to read (because
          * read doesn't NUL-terminate). So we'll do it again.
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
 * resInfoX->canUseVMwareCtrlTopologySet will be set to TRUE if we should
 * use the old driver path when setting topology.
 *
 * @return TRUE if we're able to reset topology, otherwise FALSE.
 * @note resInfoX->canUseVMwareCtrlTopologySet will be set to TRUE on success.
 */

static Bool
TopologyCanSet(void)
{
   ResolutionInfoX11Type *resInfoX = &resolutionInfoX11;

   /**
    * Note: For some strange reason, an early call to XineramaQueryVersion in
    * in this function stops vmtoolsd from deadlocking and freezing the X
    * display. Might be a call to XGrabServer() in and X library init
    * function that is called when we've already grabbed the server....
    */

#ifdef NO_MULTIMON
   resInfoX->canUseVMwareCtrlTopologySet = FALSE;
   return FALSE;
#else
   int major;
   int minor;

   if (resInfoX->canUseVMwareCtrl && XineramaQueryVersion(resInfoX->display, &major,
                                                          &minor)) {
      /*
       * We need both a new enough VMWARE_CTRL and Xinerama for this to work.
       */
      resInfoX->canUseVMwareCtrlTopologySet = (major > 0) || (major == 0 && minor >= 2);
   } else {
      resInfoX->canUseVMwareCtrlTopologySet = FALSE;
   }

   return resInfoX->canUseVMwareCtrlTopologySet ||
      (resInfoX->canUseRandR12 && resInfoX->canUseVMwareCtrl);
#endif
}

/**
 * Given a width and height, find the biggest resolution that will "fit".
 * This is called as a result of the resolution set request from the vmx.
 *
 * @param[in] width
 * @param[in] height
 *
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
   Bool perfectMatch;

#ifndef NO_MULTIMON
   if (resInfoX->canUseRandR12) {
      xXineramaScreenInfo display;

      display.x_org = 0;
      display.y_org = 0;
      display.width = width;
      display.height = height;

      return RandR12_SetTopology(resInfoX->display,
                                 DefaultScreen(resInfoX->display),
                                 resInfoX->rootWindow,
                                 1, &display, width, height);
   }
#endif

   xrrConfig = XRRGetScreenInfo(resInfoX->display, resInfoX->rootWindow);
   xrrSizes = XRRConfigSizes(xrrConfig, &xrrNumSizes);
   bestFitIndex = XRRConfigCurrentConfiguration(xrrConfig, &xrrCurRotation);

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
      Status rc;

      g_debug("Setting guest resolution to: %dx%d (requested: %d, %d)\n",
              xrrSizes[bestFitIndex].width, xrrSizes[bestFitIndex].height, width, height);
      rc = XRRSetScreenConfig(resInfoX->display, xrrConfig, resInfoX->rootWindow,
                              bestFitIndex, xrrCurRotation, GDK_CURRENT_TIME);
      g_debug("XRRSetScreenConfig returned %d (result: %dx%d)\n", rc,
              xrrSizes[bestFitIndex].width, xrrSizes[bestFitIndex].height);
   } else {
      g_debug("Can't find a suitable guest resolution, ignoring request for %dx%d\n",
              width, height);
   }

   perfectMatch = xrrSizes[bestFitIndex].width == width &&
                  xrrSizes[bestFitIndex].height == height;
   XRRFreeScreenConfigInfo(xrrConfig);

   return perfectMatch;
}

/*
 *-----------------------------------------------------------------------------
 *
 * ResolutionX11ErrorHandler --
 *
 *      Logs X non-fatal error events. This backend assumes that
 *      errors are checked within the functions that may generate
 *      them, not relying on X error events. Thus we just log and
 *      discard the events to prevent the tools daemon from crashing.
 *
 * Results:
 *      Logs error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
ResolutionX11ErrorHandler(Display *d,      // IN: Pointer to display connection
			  XErrorEvent *e)  // IN: Pointer to the error event
{
   char msg[200];

   XGetErrorText(d, e->error_code, msg, sizeof(msg));

   g_warning("X Error %d (%s): request %d.%d\n",
	     e->error_code, msg, e->request_code, e->minor_code);

   return 0;
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

   XSetErrorHandler(ResolutionX11ErrorHandler);
   gtk_init(&argc, (char ***) &argv);
   wnd = gtk_invisible_new();
#ifndef GTK3
   display = GDK_WINDOW_XDISPLAY(wnd->window);
#else
   display = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(wnd));
#endif
   return (InitHandle) display;
}

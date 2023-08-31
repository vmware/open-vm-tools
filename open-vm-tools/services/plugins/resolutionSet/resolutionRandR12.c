/*********************************************************
 * Copyright (c) 2010-2017,2019-2022 VMware, Inc. All rights reserved.
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
 * resolutionRandR12.c --
 *
 * Set of functions to handle RandR12 guest screen resizing and topology change
 * for vmusr
 *
 */

/*
 *
 * The RandR12 API lacks good documentation. To avoid poor bug fixes, please
 * refer to the Xrandr.h header file and perhaps
 *
 * http://www.x.org/wiki/Development/Documentation/HowVideoCardsWork
 *
 * And become familiar with the following concepts:
 *
 * * Output  An output is a physical monitor connector on the machine, and
 *           the associated physical device. For the vmwgfx driver,
 *           it's a logical entry point to which a VMware display may be
 *           attached.
 *
 * * Mode    A mode describes a resolution and associated timing information.
 *           The timing information is never used for the vmwgfx display driver
 *           itself, but may be used by the X server to purge modes whose
 *           timing limits lie outside of its specification. The
 *           X server keeps a global list of modes, and each output carries
 *           a list of a subset of these modes that are suitable for that
 *           output.
 *
 * * Crtc    In a physical machine, a crtc is the device that scans out
 *           data from a given portion of display memory and feeds it to
 *           one or more outputs. The crtc and its outputs need to agree about
 *           timing and they are therefore programmed with the same mode.
 *           In the vmwgfx driver, there is one and only one output per
 *           logical crtc and the crtc and its output may be viewed as a
 *           single entity.
 *
 * * Fb      Or framebuffer is the display storage area from which the
 *           crtcs scan out. It needs to be at least the size of the union
 *           of all crtc scanout areas, but may be larger.
 */

#ifndef NO_MULTIMON

#include "resolutionInt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xmd.h>
#include <X11/extensions/panoramiXproto.h>
#include <X11/extensions/Xrandr.h>

#include "resolutionRandR12.h"
#include "str.h"
#include "strutil.h"
#include "util.h"

#define RR12_MODE_FORMAT "vmw-autofit-%ux%u"
#define RR12_MODE_MAXLEN (sizeof RR12_MODE_FORMAT + 2 * (10 - 2) + 1)
#define RR12_DEFAULT_DPI (96.0)
#define MILLIS_PER_INCH (25.4)


/*
 * Local data
 */

/*
 * The output structure.
 * Contains detailed information about each output and its connectivity.
 */

typedef struct RandR12Output {
   XRROutputInfo *output;       // Pointer to detailed info
   RROutput id;                 // XID of the output
   int crtc;                    // crtc entry in the RandR12Info array
   RRMode mode;                 // XID of current mode
} RandR12Output;

/*
 * The RandR12Info context. Contains info about the current topology state
 * and enough information to revert to the previous state.
 */

typedef struct RandR12Info {
   unsigned int nCrtc;          // Number of crtcs in the crtcs array
   unsigned int nOutput;        // Number of outputs in the outputs array
   unsigned int nNewModes;      // Number of new modes in the newModes array
   XRRCrtcInfo **crtcs;
   RandR12Output *outputs;
   XRRModeInfo **newModes;      // Newly created autofit modes
   XRRScreenResources *xrrRes;  // Screen resuources obtained from the server
   unsigned int xdpi;           // Current dpi in x direction
   unsigned int ydpi;           // Current dpi in y direction
   unsigned int origWidth;      // Used for reverting on failure
   unsigned int origHeight;     // Used for reverting on failure
   int event_base;              // For the RandR extension
   int error_base;
} RandR12Info;

/*
 * Optionally dump RandR12 log messages to a local logfile rather than to the
 * gtk logfile.
 */

/* #define _LOCAL_LOG_ "/tmp/randr12.log" */
#ifdef _LOCAL_LOG_
static FILE *_ofile;

#define LOG_START _ofile = fopen(_LOCAL_LOG_, "a")
#define g_debug(...) do {                       \
      fprintf(_ofile, "Debug:   " __VA_ARGS__); \
      fflush(_ofile); } while (0)
#define g_warning(...) do {                     \
      fprintf(_ofile, "Warning: " __VA_ARGS__); \
      fflush(_ofile); } while (0)
#define LOG_STOP fclose(_ofile)
#else
#define LOG_START
#include <glib.h>
#define LOG_STOP
#endif


/*
 * Local functions
 */

static void RandR12FreeInfo(RandR12Info *info);
static RandR12Info *RandR12GetInfo(Display *display, Window rootWin);
static Bool RandR12CrtcDisable(Display *display, unsigned int ndisplays,
                               RandR12Info *info, unsigned int width,
                               unsigned int height);
static unsigned int RandR12Dpi(unsigned int pixels, unsigned int mm);
static void RandR12CurrentSize(Display *display, int screen,
                               XRRScreenSize *cSize);
static Bool RandR12SetSizeVerify(Display *display,  Window rootWin,
                                 int screen, RandR12Info *info,
                                 int width, int height);
static Bool RandR12OutputHasMode(XRROutputInfo *output,
                                 XRRModeInfo *modeInfo);
static XRRModeInfo *RandR12MatchMode(Display *display, Window rootWin,
                                     RandR12Output *rrOutput,
                                     RandR12Info *info,
                                     int width, int height);
static Bool RandR12SetupOutput(Display *display, Window rootWin,
                               RandR12Info *info, RandR12Output *rrOutput,
                               int x, int y, int width, int height);
static void RandR12DeleteModes(Display *display, RandR12Info *info);


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12FreeInfo --
 *
 *      Free all allocations associated with a RandR12Info context
 *
 * Results:
 *      The RandR12Info context is freed, and the pointer may no
 *      longer be dereferenced.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RandR12FreeInfo(RandR12Info *info)     // IN/OUT the RandR12Info context
{
   unsigned int i;

   if (!info) {
      return;
   }

   if (!info->xrrRes) {
      goto out_freeinfo;
   }

   for (i = 0; i < info->nNewModes; ++i) {
      XRRFreeModeInfo(info->newModes[i]);
   }

   for (i = 0; i < info->nCrtc; ++i) {
      if (info->crtcs[i]) {
         XRRFreeCrtcInfo(info->crtcs[i]);
      }
   }

   for (i = 0; i < info->nOutput; ++i) {
      if (info->outputs[i].output) {
         XRRFreeOutputInfo(info->outputs[i].output);
      }
   }

   free(info->newModes);
   free(info->outputs);
   free(info->crtcs);
   XRRFreeScreenResources(info->xrrRes);
 out_freeinfo:
   free(info);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12GetInfo --
 *
 *      Allocate and initialize a RandR12Info context.
 *      Get the current X server configuration and info about outputs and
 *      crtcs. When done with the context, it should be freed using
 *      RandR12FreeInfo.
 *
 * Results:
 *      A pointer to a RandR12Info context on success. NULL on failure.
 *
 * Side effects:
 *      The info structure is setup for subsequent use of other functions.
 *      Space is allocated as needed and outputs are ordered in the
 *      info->outputs array with LVDS0 first and LVDS8 last.
 *
 *-----------------------------------------------------------------------------
 */

static RandR12Info *
RandR12GetInfo(Display *display,     // IN: Pointer to our display connection
               Window rootWin)       // IN: ID of the root window
{
   unsigned int i, j, num, numVMWCrtc;
   XRROutputInfo *output;
   RandR12Output *rrOutput;
   XRRCrtcInfo *crtc;
   XRRScreenResources *xrrRes;
   unsigned int nVMWOutput = 0;
   RandR12Info *info = Util_SafeCalloc(1, sizeof *info);

   /*
    * XRRQueryExtension is only used to get info->event_base,
    */

   if (!XRRQueryExtension(display, &info->event_base, &info->error_base)) {
      g_warning("%s: XRRQueryExtension failed.\n", __func__);
      goto out_err;
   }

   info->xrrRes = xrrRes = XRRGetScreenResources(display, rootWin);
   if (!xrrRes) {
      goto out_err;
   }

   info->nCrtc = xrrRes->ncrtc;
   info->nOutput = xrrRes->noutput;
   info->crtcs = Util_SafeCalloc(info->nCrtc, sizeof *info->crtcs);
   info->outputs = Util_SafeCalloc(info->nOutput, sizeof *info->outputs);
   info->newModes = Util_SafeCalloc(info->nOutput, sizeof *info->newModes);

   for (i = 0; i < info->nOutput; ++i) {
      output = XRRGetOutputInfo(display, xrrRes, xrrRes->outputs[i]);
      if (!output) {
         goto out_err;
      }

      if (sscanf(output->name, RR12_OUTPUT_FORMAT, &num) != 1) {
         XRRFreeOutputInfo(output);
         continue;
      }

      if (num > info->nOutput) {
         XRRFreeOutputInfo(output);
         goto out_err;
      }

      info->outputs[num - 1].output = output;
      info->outputs[num - 1].id = xrrRes->outputs[i];
      info->outputs[num - 1].crtc = -1;
      if (num > nVMWOutput) {
         nVMWOutput = num;
      }
   }

   /*
    * Confidence checks. This should never really happen with current drivers.
    */

   if (nVMWOutput != info->nOutput) {
      g_warning("%s: Not all outputs were VMW outputs.\n", __func__);
      goto out_err;
   }

   for (i = 0; i < nVMWOutput; ++i) {
      if (!info->outputs[i].output) {
         g_warning("%s: Missing output. %d\n", __func__, i);
         goto out_err;
      }
   }

   numVMWCrtc = 0;
   for (i = 0; i < info->nCrtc; ++i) {
      crtc = XRRGetCrtcInfo(display, xrrRes, xrrRes->crtcs[i]);
      if (!crtc) {
         goto out_err;
      }

      info->crtcs[i] = crtc;

      for (j = 0; j < nVMWOutput; ++j) {
         rrOutput = &info->outputs[j];
         if (crtc->npossible > 0 &&
             crtc->possible[0] == rrOutput->id && rrOutput->crtc == -1) {
            rrOutput->crtc = i;
            rrOutput->mode = crtc->mode;
            numVMWCrtc++;
            break;
         }
      }
   }

   /*
    * Confidence check. This should never really happen with our drivers.
    */

   if (numVMWCrtc != nVMWOutput) {
      g_warning("%s: Crtc / Output number mismatch.\n", __func__);
      goto out_err;
   }

   return info;
 out_err:
   RandR12FreeInfo(info);
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12CrtcDisable --
 *
 *      Deactivate crtcs and associated outputs before an fb size change.
 *      The function deactivates crtcs and associated outputs
 *      1) whose scanout area is too big for the new fb size.
 *      2) that are going to be deactivated with the new topology.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      The RandR12info context is modified.
 *      The current mode of deactivated outputs is set to "None".
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandR12CrtcDisable(Display *display,       // IN: Pointer to display
                                           // connection
                   unsigned int ndisplays, // IN: Number of VMware display in
                                           // the new topology
                   RandR12Info *info,      // IN/OUT: The RandR12Info context
                   unsigned int width,     // IN: Width of new topology
                   unsigned int height)    // IN: Height of new topology
{
   XRRScreenResources *xrrRes = info->xrrRes;
   XRRCrtcInfo *crtc;
   unsigned int i;

   for (i = 0; i < info->nCrtc; ++i) {
      crtc = info->crtcs[i];

      if (crtc->mode != None &&
          (crtc->x + crtc->width > width || crtc->y + crtc->height > height)) {

         if (XRRSetCrtcConfig(display, xrrRes, xrrRes->crtcs[i],
                              CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0) !=
             Success) {
            return FALSE;
         }
      }
   }

   for (i = ndisplays; i < info->nOutput; ++i) {
      crtc = info->crtcs[info->outputs[i].crtc];

      if (crtc->mode != None &&
          XRRSetCrtcConfig(display, xrrRes, xrrRes->crtcs[i],
                           CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0) !=
          Success) {
         return FALSE;
      }
      info->outputs[i].mode = None;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12Dpi --
 *
 *      Given a number of pixels and a width in mm, compute the DPI value.
 *      If input or output looks suspicious (zero), revert to a default
 *      DPI value.
 *
 * Results:
 *      Returns the DPI value.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static unsigned int
RandR12Dpi(unsigned int pixels,  // IN: Dimension in pixels
           unsigned int mm)      // IN: Dimension in mm
{
   unsigned int dpi = 0;

   if (mm > 0) {
      dpi = (unsigned int)((double)pixels * MILLIS_PER_INCH /
                           (double)mm + 0.5);
   }

   return (dpi > 0) ? dpi : RR12_DEFAULT_DPI;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12CurrentSize --
 *
 *      Return the current dimensions of the fb, as cached in the display
 *      structure.
 *
 * Results:
 *      cSize is filled with the dimensions in pixels and mm.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RandR12CurrentSize(Display *display,      // IN: Pointer to the display
                                          // connection
                   int screen,            // IN: The X screen
                   XRRScreenSize *cSize)  // OUT: The fb size
{
   memset(cSize, 0, sizeof *cSize);

   cSize->width = DisplayWidth(display, screen);
   cSize->mwidth = DisplayWidthMM(display, screen);
   cSize->height = DisplayHeight(display, screen);
   cSize->mheight = DisplayHeightMM(display, screen);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12GetDpi --
 *
 *      Save the width, height and dpi of the current fb setup.
 *      This is used when reverting on failure and the dpi is used
 *      to calculate the new fb dimesions in mm.
 *
 * Results:
 *      The info structure is populated with the computed values.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RandR12GetDpi(Display *display,   // IN: Pointer to the display connection
              int screen,         // IN: The X server screen
              RandR12Info *info)  // OUT: The RandR12Info context
{
   XRRScreenSize cSize;

   RandR12CurrentSize(display, screen, &cSize);

   info->origWidth = cSize.width;
   info->origHeight = cSize.height;
   info->xdpi = RandR12Dpi(cSize.width, cSize.mwidth);
   info->ydpi = RandR12Dpi(cSize.height, cSize.mheight);

   g_debug("%s: DPI is %u %u\n", __func__, info->xdpi, info->ydpi);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12SetSizeVerify --
 *
 *      Set a new fb size, verify that the change went through and update
 *      the display structure.
 *
 * Results:
 *      Returns TRUE if function succeeds, FALSE otherwise. Upon failure,
 *      the function will make an attempt to restore the previous dimensions.
 *      The display structure is updated to hold the new dimensions.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandR12SetSizeVerify(Display *display,  // IN/OUT: Pointer to the display
                                        // connection
                     Window rootWin,    // IN: ID of root window
                     int screen,        // IN: ID of X screen
                     RandR12Info *info, // IN: The RandR12Info context
                     int width,         // IN: New width
                     int height)        // IN: New height
{
   XRRScreenSize cSize;
   XEvent configEvent;
   unsigned int xmm;
   unsigned int ymm;
   Bool event = FALSE;

   xmm = (int)(MILLIS_PER_INCH * width / ((double)info->xdpi) + 0.5);
   ymm = (int)(MILLIS_PER_INCH * height / ((double)info->ydpi) + 0.5);

   g_debug("%s: Setting screenSize to %d %d %d %d\n", __func__,
           width, height, xmm, ymm);

   XRRSelectInput(display, rootWin, RRScreenChangeNotifyMask);
   XRRSetScreenSize(display, rootWin, width, height, xmm, ymm);

   /*
    * We need to sync and parse these events to update our display
    * structure with the new size. Nobody else does this for us.
    */

   XSync(display, FALSE);
   while (XCheckTypedEvent(display, RRScreenChangeNotify + info->event_base,
                           &configEvent)) {
      (void)XRRUpdateConfiguration(&configEvent);
      event = TRUE;
   }
   XRRSelectInput(display, rootWin, 0);

   if (!event) {
      g_warning("%s: Received no size change events.\n", __func__);
   }

   RandR12CurrentSize(display, screen, &cSize);
   if (cSize.width == width && cSize.height == height) {
      return TRUE;
   }

   /*
    * On failure, try to revert to the original size in preparation for
    * also reverting the Crtcs.
    */

   if (cSize.width != info->origWidth || cSize.height != info->origHeight) {

      xmm = (int)(MILLIS_PER_INCH * info->origWidth /
                  ((double)info->xdpi) + 0.5);
      ymm = (int)(MILLIS_PER_INCH * info->origHeight /
                  ((double)info->ydpi) + 0.5);

      XRRSelectInput(display, rootWin, RRScreenChangeNotifyMask);
      XRRSetScreenSize(display, rootWin, info->origWidth,
                       info->origHeight, xmm, ymm);
      XSync(display, FALSE);
      while (XCheckTypedEvent(display, RRScreenChangeNotify + info->event_base,
                              &configEvent)) {
         (void)XRRUpdateConfiguration(&configEvent);
      }
      XRRSelectInput(display, rootWin, 0);
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12OutputHasMode --
 *
 *      Determine whether a mode is registered with an output.
 *
 * Results:
 *      TRUE if the mode is registered, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandR12OutputHasMode(XRROutputInfo *output,       // IN: Output
                     XRRModeInfo *modeInfo)       // IN: Mode.
{
   unsigned int i;

   for (i = 0; i < output->nmode; ++i) {
      if (output->modes[i] == modeInfo->id) {
         return TRUE;
      }
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12MatchMode --
 *
 *      Lookup an already exising mode, or register a new mode for the
 *      given size and the given output.
 *
 * Results:
 *      Returns a pointer to the mode's XRRModeInfo structure on success.
 *      NULL on failure.
 *
 * Side effects:
 *      If a new mode is created, it is registered with the RandR12Info
 *      structure for cached lookup, and with the X server.
 *
 *-----------------------------------------------------------------------------
 */

static XRRModeInfo *
RandR12MatchMode(Display *display,        // IN: Pointer to display connection
                 Window rootWin,          // IN: ID of root window
                 RandR12Output *rrOutput, // IN: The output
                 RandR12Info *info,       // IN/OUT: RandR12Info context
                 int width,               // IN: Width of sought mode
                 int height)              // IN: Height of sought mode.
{
   XRROutputInfo *output = rrOutput->output;
   XRRScreenResources *xrrRes = info->xrrRes;
   XRRModeInfo *modeInfo = NULL;
   char name[RR12_MODE_MAXLEN];
   int i;
   RRMode newMode;

   g_debug("%s: Trying to find a mode for resolution %dx%d.\n",
           __func__, width, height);

   for (i = 0; i < xrrRes->nmode; ++i) {
      modeInfo = &xrrRes->modes[i];
      if (modeInfo->width == width && modeInfo->height == height) {
         unsigned int w, h;

         /*
          * An autofit mode will work with any output
          */

         if (sscanf(modeInfo->name, RR12_MODE_FORMAT, &w, &h) == 2) {
            return modeInfo;
         }

         /*
          * Otherwise, make sure the mode is registered with the given output,
          * to avoid issues with timing incompatibilities.
          */

         if (RandR12OutputHasMode(output, modeInfo)) {
            g_debug("%s: Found an existing mode. Mode name is %s\n",
                    __func__, modeInfo->name);
            return modeInfo;
         }
      }
   }

   /*
    * Check for recent autofit modes. If the mode is not in the
    * output's modelist, then add it.
    */

   for (i = 0; i < info->nNewModes; ++i) {
      modeInfo = info->newModes[i];
      if (modeInfo->width == width && modeInfo->height == height) {

         if (!RandR12OutputHasMode(output, modeInfo)) {
            XRRAddOutputMode(display, rrOutput->id, modeInfo->id);
         }
         g_debug("%s: Found a recent autofit mode. Mode name is %s\n",
                 __func__, modeInfo->name);
         return modeInfo;
      }
   }

   /*
    * Create a new mode.
    */

   snprintf(name, sizeof name, RR12_MODE_FORMAT, width, height);
   modeInfo = XRRAllocModeInfo(name, strlen(name));
   modeInfo->width = width;
   modeInfo->height = height;
   newMode = XRRCreateMode(display, rootWin, modeInfo);
   if (newMode == None) {
      XRRFreeModeInfo(modeInfo);
      return NULL;
   }
   modeInfo->id = newMode;
   info->newModes[info->nNewModes++] = modeInfo;
   XRRAddOutputMode(display, rrOutput->id, modeInfo->id);

   g_debug("%s: Set up a new mode. Mode name is %s\n",
           __func__, modeInfo->name);

   return modeInfo;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12SetupOutput --
 *
 *      Set up an output and it's associated CRTC to scanout and show a
 *      specified region of the frame-buffer.
 *
 * Results:
 *      Returns TRUE on success. FALSE on failure.
 *      May add new modes to the RandR12Info context.
 *      Sets up rrOutput->mode to point to the new mode.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RandR12SetupOutput(Display *display,        // IN: The display connection
                   Window rootWin,          // IN: ID of root window
                   RandR12Info *info,       // IN/OUT: RandR12Info context
                   RandR12Output *rrOutput, // IN/OUT: Identifies the output
                   int x,                   // IN: X coordinate of upper
                                            // left corner of scanout area
                   int y,                   // IN: Corresponding Y coordinate
                   int width,               // IN: Width of scanout area
                   int height)              // IN: Height of scanout area
{
   RRCrtc crtcID = info->xrrRes->crtcs[rrOutput->crtc];
   XRRCrtcInfo *crtcInfo = info->crtcs[rrOutput->crtc];
   XRRModeInfo *mode;
   Status ret;

   mode = RandR12MatchMode(display, rootWin, rrOutput, info, width, height);

   g_debug("%s: Setting up RandR Crtc %d. %dx%d@%d,%d: \"%s\"\n",
           __func__, (int)crtcID, width, height, x, y,
           (mode) ? mode->name : "NULL");

   if (!mode) {
      return FALSE;
   }
   if (!crtcInfo) {
       g_warning("%s: Wasn't able to find crtc info for crtc id %d.\n", __func__,
                   (int)crtcID);
       return FALSE;
   }

   ret = XRRSetCrtcConfig(display, info->xrrRes, crtcID, CurrentTime, x, y,
                          mode->id, crtcInfo->rotation, &rrOutput->id, 1);
   if (ret == Success) {
      rrOutput->mode = mode->id;
      return TRUE;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12DeleteModes --
 *
 *      Delete unused autofit modes from outputs not using them and
 *      unregister those modes from the X server if no utput is using
 *      them.
 *
 * Results:
 *      Nothing immediately visible to the caller.
 *
 * Side effects:
 *      Invalidates the RandR12Info context for subsequent mode lookups.
 *      The RandR12Info context should be destroyed after this operation.
 *
 *-----------------------------------------------------------------------------
 */

static void
RandR12DeleteModes(Display *display,  // IN: The display connection
                   RandR12Info *info) // IN: RandR12Info context
{
   XRRScreenResources *xrrRes = info->xrrRes;
   unsigned int i, j;
   unsigned int w, h;
   Bool used;

   /*
    * Loop over the global X server mode list skipping modes that are not
    * our autofit modes.
    */

   for (i = 0; i < xrrRes->nmode; ++i) {
      XRRModeInfo *modeInfo = &xrrRes->modes[i];

      if (sscanf(modeInfo->name, RR12_MODE_FORMAT, &w, &h) != 2) {
         continue;
      }

      used = FALSE;

      /*
       * Loop over all outputs and see if the outfit mode is used by any
       * output. In that case mark it as used,
       * otherwise check if the mode is in the output's mode list.
       * In that case remove it from the output mode list.
       */

      for (j = 0; j < info->nOutput; ++j) {
         RandR12Output *rrOutput = &info->outputs[j];

         if (rrOutput->mode != modeInfo->id) {
            if (RandR12OutputHasMode(rrOutput->output, modeInfo)) {
                  g_debug("%s: Deleting mode %s.\n", __func__,
                          modeInfo->name);
                  XRRDeleteOutputMode(display, rrOutput->id, modeInfo->id);
            }
         } else {
            used = TRUE;
         }
      }

      /*
       * If the mode wasn't used by any output, remove it from the X server's
       * global modelist.
       */

      if (!used) {
         g_debug("%s: Destroying mode %s.\n", __func__, modeInfo->name);
         XRRDestroyMode(display, modeInfo->id);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12Revert --
 *
 *      Attempt to revert crtcs and outputs to the previous topology.
 *      Delete unused autofit modes.
 *
 * Results:
 *      Nothing immediately visible to the caller.
 *      The RandR12Info context pointer may be replaced with a
 *      pointer to a new context. In that case the old context
 *      will have been freed.
 *
 * Side effects:
 *      The RandR12Info context will be invalidated for subsequent
 *      mode lookups and should be destroyed after this operation.
 *
 *-----------------------------------------------------------------------------
 */

static void
RandR12Revert(Display *display,    // IN: The display connection
              Window rootWin,      // IN: ID of the root window
              RandR12Info **pInfo) // IN/OUT: RandR12Info context pointer
{
   unsigned int i;
   RandR12Info *info = *pInfo;
   XRRScreenResources *xrrRes = info->xrrRes;
   XRRCrtcInfo *crtc;
   RRCrtc crtcID;

   g_debug("%s: Reverting to original setup.\n", __func__);

   for (i = 0; i < info->nOutput; ++i) {

      RandR12Output *rrOutput = &info->outputs[i];
      crtc = info->crtcs[rrOutput->crtc];
      crtcID = xrrRes->crtcs[rrOutput->crtc];

      if (XRRSetCrtcConfig(display, info->xrrRes, crtcID,
                           CurrentTime, crtc->x, crtc->y,
                           crtc->mode, crtc->rotation,
                           crtc->outputs, crtc->noutput) != Success) {

         g_warning("%s: Reverting crtc id %d failed.\n", __func__,
                   (int)crtcID);
      } else {
         rrOutput->mode = crtc->mode;
      }
   }

   *pInfo = RandR12GetInfo(display, rootWin);
   if (*pInfo) {
      RandR12FreeInfo(info);
      info = *pInfo;
      RandR12DeleteModes(display, info);
   } else {
      *pInfo = info;
      g_warning("%s: Deleting unused modes after revert failed.\n", __func__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RandR12_SetTopology --
 *
 *      Employs the RandR 1.2 extension to set a new display topology.
 *      This is for the new vmwgfx X driver, which uses RandR 1.2 to
 *      program multiple outputs.
 *      Delete unused autofit modes.
 *
 * Results:
 *      Returns TRUE on success, FALSE on failure.
 *      The Display structure will be updated with the new fb dimensions.
 *      On failure, the function will have made an attempt to restore the
 *      old dimensions and topology.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
RandR12_SetTopology(Display *dpy,           // IN/OUT: The display connection
                    int screen,             // IN: The X screen
                    Window rootWin,         // IN: ID of root window
                    unsigned int ndisplays, // IN: Number of VMware displays
                                            // in the topology.
                    xXineramaScreenInfo *displays, // IN: Array describing the
                                            // topology
                    unsigned int width,     // IN: Topology width
                    unsigned int height)    // IN: Topology height
{
   int minWidth, minHeight, maxWidth, maxHeight;
   RandR12Info *info;
   Bool retVal = FALSE;
   unsigned int i;
   static unsigned long sequence = 0;

   LOG_START;

   g_debug("%s: New request. Sequence is %lu\n", __func__, sequence++);

   if (!XRRGetScreenSizeRange(dpy, rootWin, &minWidth, &minHeight, &maxWidth,
                              &maxHeight) ||
       width < minWidth || height < minHeight ||
       width > maxWidth || height > maxHeight) {
      g_warning("%s: Invalid size request.\n", __func__);
      LOG_STOP;
      return FALSE;
   }

   info = RandR12GetInfo(dpy, rootWin);
   if (!info) {
      g_warning("%s: Setup info struct failed.\n", __func__);
      return FALSE;
   }

   RandR12GetDpi(dpy, screen, info);

   if (!RandR12CrtcDisable(dpy, ndisplays, info, width, height)) {
      g_warning("%s: Failed disabling unused crtcs.\n", __func__);
      RandR12Revert(dpy, rootWin, &info);
      goto out_ungrab;
   }

   if (!RandR12SetSizeVerify(dpy, rootWin, screen, info, width, height)) {
      g_warning("%s: Failed setting new framebuffer size.\n", __func__);
      RandR12Revert(dpy, rootWin, &info);
      goto out_ungrab;
   }

   g_debug("%s: Setting up %d VMware displays.\n", __func__, ndisplays);
   for (i = 0; i < ndisplays; ++i) {
      xXineramaScreenInfo *vmwin = &displays[i];

      if (i >= info->nOutput)
         break;

      if (!RandR12SetupOutput(dpy, rootWin, info, &info->outputs[i],
                              vmwin->x_org, vmwin->y_org, vmwin->width,
                              vmwin->height)) {

         /*
          * If this fails, something is seriously wrong, so
          * we don't try to revert at this point.
          */

         g_warning("%s: Setup VMware display %d failed, "
                   "but we're not reverting the operation.\n", __func__, i);
      }
   }

   retVal = TRUE;

 out_ungrab:

   g_debug("%s: Deleting unused autofit modes.\n", __func__);
   RandR12DeleteModes(dpy, info);

   XSync(dpy, FALSE);
   RandR12FreeInfo(info);

   LOG_STOP;
   return retVal;
}

#endif // ifndef NO_MULTIMON

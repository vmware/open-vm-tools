/*********************************************************
 * Copyright (c) 2016-2021 VMware, Inc. All rights reserved.
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
/* Authors:
 * Thomas Hellstrom <thellstrom@vmware.com>
 */

#define G_LOG_DOMAIN "resolutionCommon"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "resolutionDL.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "resolutionCommon.h"

#ifdef ENABLE_RESOLUTIONKMS

/* The DRM device we are looking for */
#define RESOLUTION_VENDOR     "0x15ad"
#define RESOLUTION_SVGA2_DEVICE     "0x0405"
#define RESOLUTION_SVGA3_DEVICE     "0x0406"
#define RESOLUTION_KERNELNAME "vmwgfx"

/* Required DRM version for resolutionKMS */
#define RESOLUTION_DRM_MAJOR  2
#define RESOLUTION_DRM_MINOR  14


/*
 *-----------------------------------------------------------------------------
 *
 * resolutionOpenDRM --
 *
 *     Opens a file descriptor on the indicated node to the first SVGA2 device.
 *
 * Results:
 *     Returns a positive file descriptor on success. Otherwise returns -1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
static int
resolutionOpenDRM(const char *node) // IN: Device node base name.
{
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    int fd = -1;
    int drmFd;
    const char *devNode = NULL;

    /* Force load the kernel module. */
    drmFd = drmOpen(RESOLUTION_KERNELNAME, NULL);
    if (drmFd >= 0) {
       (void) drmDropMaster(drmFd);
    }

    udev = udev_new();
    if (!udev) {
        goto outNoUdev;
    }

    /*
     * Udev error return codes that are not caught immediately are
     * typically caught in the input argument check in the udev
     * function calls following the failing call!
     */
    enumerate = udev_enumerate_new(udev);
    if (udev_enumerate_add_match_subsystem(enumerate, "drm"))
       goto outErr;
    if (udev_enumerate_add_match_property(enumerate, "DEVTYPE", "drm_minor"))
       goto outErr;
    if (udev_enumerate_scan_devices(enumerate))
       goto outErr;

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(devListEntry, devices) {
       const char *path, *vendor, *device;
       struct udev_device *parent;

       path = udev_list_entry_get_name(devListEntry);
       if (!path)
          continue;
       if (!strstr(path, node))
          continue;

       dev = udev_device_new_from_syspath(udev, path);
       if (!dev)
          goto outErr;

       parent = udev_device_get_parent_with_subsystem_devtype(dev,
                                                              "pci",
                                                              NULL);
       if (!parent)
          goto skipCheck;

       vendor = udev_device_get_sysattr_value(parent, "vendor");
       device = udev_device_get_sysattr_value(parent, "device");
       if (!vendor || !device)
          goto skipCheck;

       if (strcmp(vendor, RESOLUTION_VENDOR) != 0 ||
           (strcmp(device, RESOLUTION_SVGA2_DEVICE) != 0 &&
             strcmp(device, RESOLUTION_SVGA3_DEVICE) != 0))
          goto skipCheck;

       devNode = udev_device_get_devnode(dev);
       if (!devNode)
          goto outFound;

       fd = open(devNode, O_RDWR);
       udev_device_unref(dev);
       break;

skipCheck:
       udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    if (drmFd >= 0) {
       drmClose(drmFd);
    }

    return fd;

  outFound:
    udev_device_unref(dev);
  outErr:
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
  outNoUdev:
    if (drmFd >= 0) {
       drmClose(drmFd);
    }

    return -1;
}

/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDRMCheckVersion --
 *
 *     Check that the drm version supports GUI topology communication.
 *
 *     Checks that the DRM device supports setting GUI topology from the
 *     control node, and also that the topology is communicated on the
 *     modesetting connectors.
 *
 * Results:
 *     0 if DRM device is usable for communicating GUI topology.
 *     -1 otherwise.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
static int
resolutionDRMCheckVersion(int fd)  // IN: An open DRM file descriptor.
{
   drmVersionPtr ver = drmGetVersion(fd);

   if (!ver) {
      g_debug("%s: Failed to get DRM version.\n", __func__);
      return -1;
   }

   if (ver->version_major != RESOLUTION_DRM_MAJOR ||
       ver->version_minor < RESOLUTION_DRM_MINOR) {
      g_debug("%s: Insufficient DRM version %d.%d for resolutionKMS.\n",
              __func__, ver->version_major, ver->version_minor);
      drmFreeVersion(ver);
      return -1;
   }
   g_debug("%s: DRM version %d.%d.\n",
           __func__, ver->version_major, ver->version_minor);

   drmFreeVersion(ver);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDRMRPrimaryCheckOpen --
 *
 *     First tries to open a render node to DRM, and if that fails opens a
 *     primary node, and drops master. Then checks that drm supports GUI
 *     topology communication.
 *
 * Results:
 *     If succesful returns a positive open file descriptor. Otherwise
 *     returns -1.
 *
 * Side effects:
 *      May temporarily become drm master if render nodes are not available,
 *      and thus race with the X server.
 *
 *-----------------------------------------------------------------------------
 */
static int
resolutionDRMRPrimaryCheckOpen(void)
{
   int fd = -1;

   fd = resolutionOpenDRM("renderD");
   if (fd < 0) {
      g_debug("%s: Failed to open DRM render node.\n", __func__);
      fd = resolutionOpenDRM("card");
      if (fd >= 0)
         (void) drmDropMaster(fd);
   }
   if (fd < 0) {
      g_debug("%s: Failed to open DRM card node.\n", __func__);
      goto outErr;
   }

   if (!resolutionDRMCheckVersion(fd)) {
      return fd;
   }

   close(fd);
outErr:
   return -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * resolutionCheckForKMS --
 *
 *     Checks whether the vmwgfx DRM is present and supports exposing
 *     layout information through connector properties and preferred modes.
 *
 * Results:
 *     If succesful returns a positive number representing an open file
 *     descriptor to the node indicated by the control argument. If
 *     unsuccessful returns -1.
 *
 * Side effects:
 *     Opens a file to DRM. The file descriptor should be closed with
 *     resolutionDRMClose() when needed.
 *
 *-----------------------------------------------------------------------------
 */
int
resolutionCheckForKMS(ToolsAppCtx *ctx)  // IN: The ToolsAppCtx for
		                         // configuration db access.
{
   GError *err = NULL;
   gboolean doResolutionKMS;
   int fd;

   doResolutionKMS = g_key_file_get_boolean(ctx->config, "resolutionKMS",
                                            "enable", &err);
   if (err) {
      g_clear_error(&err);
      /*
       * We are going to try to see if we can load a valid vmwgfx
       */
      doResolutionKMS = TRUE;
   } else {
      g_debug("%s: ResolutionKMS %s using configuration file info.\n",
              __func__, (doResolutionKMS) ? "enabled" : "disabled");
   }

   if (!doResolutionKMS)
      return -1;

   if (resolutionDLOpen()) {
      g_warning("%s: Failed to find needed system libraries for "
                "resolutionKMS.\n", __func__);
      return -1;
   } else {
      g_message("%s: dlopen succeeded.\n", __func__);
   }

   fd = resolutionDRMRPrimaryCheckOpen();

   if (fd < 0)
      g_warning("%s: No system support for resolutionKMS.\n", __func__);
   else
      g_message("%s: System support available for resolutionKMS.\n",
                __func__);

   return fd;
}

/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDRMClose --
 *
 *     Scans for VMWare Xorg driver files and tries to determine the Xorg
 *     driver version.
 *
 * Results:
 *     If succesful returns zero and outputs the driver version in the
 *     parameters major, minor and level. If not successful, returns -1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
void
resolutionDRMClose(int fd)
{
   close(fd);
   resolutionDLClose();
}

#endif /* ENABLE_RESOLUTIONKMS */

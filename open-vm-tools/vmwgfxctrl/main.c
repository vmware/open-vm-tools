/*********************************************************
 * Copyright (C) 2021 VMware, Inc. All rights reserved.
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
 * main.c --
 *
 *      vmwgfxctrl is a utility application used to control the
 *      vmwgfx drm linux kernel driver.
 *
 *
 * To print the information about the current display topology
 *   vmwgfxctrl --print-topology
 *
 * Setting topology will most likely require root privileges
 *   sudo vmwgfxctrl --set-topology 1024x768+0+0
 *
 * The format to set-topology is
 *   sudo vmwgfxctrl --set-topology WxH+x+y
 *
 * with "WxH+x+y" repeated as many times as the number of screens
 *  being set e.g.
 *   sudo vmwgfxctrl --set-topology 800x600+0+0 800x600+800+0
 * will set two screen right next to each other (the second starts at
 * x=800, where the first one ends), both with height of 600.
 *
 */

#include "vm_basic_defs.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vmwgfx_drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* The DRM device we are looking for */
#define VMWGFXCTRL_VENDOR     "0x15ad"
#define VMWGFXCTRL_DEVICE     "0x0405"
#define VMWGFXCTRL_KERNELNAME "vmwgfx"

/* Required DRM version for resolutionKMS */
#define VMWGFXCTRL_DRM_MAJOR  2
#define VMWGFXCTRL_DRM_MINOR  14

static int
vmwgfxOpenDRM(const char *node) // IN: Device node base name.
{
   struct udev *udev;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *devices, *devListEntry;
   struct udev_device *dev;
   int fd = -1;
   int drmFd;
   const char *devNode = NULL;

   /* Force load the kernel module. */
   drmFd = drmOpen(VMWGFXCTRL_KERNELNAME, NULL);
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
   if (udev_enumerate_add_match_subsystem(enumerate, "drm")) {
      goto outErr;
   }
   if (udev_enumerate_add_match_property(enumerate, "DEVTYPE", "drm_minor")) {
      goto outErr;
   }
   if (udev_enumerate_scan_devices(enumerate)) {
      goto outErr;
   }

   devices = udev_enumerate_get_list_entry(enumerate);
   udev_list_entry_foreach(devListEntry, devices) {
      const char *path, *vendor, *device;
      struct udev_device *parent;

      path = udev_list_entry_get_name(devListEntry);
      if (!path) {
         continue;
      }
      if (!strstr(path, node)) {
         continue;
      }

      dev = udev_device_new_from_syspath(udev, path);
      if (!dev) {
         goto outErr;
      }

      parent = udev_device_get_parent_with_subsystem_devtype(dev,
                                                             "pci",
                                                             NULL);
      if (!parent) {
         goto skipCheck;
      }

      vendor = udev_device_get_sysattr_value(parent, "vendor");
      device = udev_device_get_sysattr_value(parent, "device");
      if (!vendor || !device) {
         goto skipCheck;
      }

      if (strcmp(vendor, VMWGFXCTRL_VENDOR) != 0 ||
          strcmp(device, VMWGFXCTRL_DEVICE) != 0) {
         goto skipCheck;
      }

      devNode = udev_device_get_devnode(dev);
      if (!devNode) {
         goto outFound;
      }

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


static bool
checkDRMVersion(int fd)  // IN: An open DRM file descriptor.
{
   drmVersionPtr ver = drmGetVersion(fd);

   if (!ver) {
      fprintf(stderr, "%s: Failed to get DRM version.\n", __func__);
      return false;
   }

   if (ver->version_major != VMWGFXCTRL_DRM_MAJOR ||
       ver->version_minor < VMWGFXCTRL_DRM_MINOR) {
      fprintf(stderr, "%s: Insufficient DRM version %d.%d for resolutionKMS.\n",
              __func__, ver->version_major, ver->version_minor);
      drmFreeVersion(ver);
      return false;
   }
   printf("DRM version %d.%d.\n", ver->version_major, ver->version_minor);

   drmFreeVersion(ver);
   return true;
}

static int
vmwgfxOpen(bool use_render_node)
{
   int fd = -1;

   if (use_render_node) {
      fd = vmwgfxOpenDRM("renderD");
      if (fd < 0) {
         fprintf(stderr, "%s: Failed to open DRM render node.\n", __func__);
      }
   }
   if (fd < 0) {
      fd = vmwgfxOpenDRM("card");
      if (fd >= 0) {
         (void) drmDropMaster(fd);
      }
   }
   if (fd < 0) {
      fprintf(stderr, "%s: Failed to open DRM card node.\n", __func__);
      goto outErr;
   }

   if (checkDRMVersion(fd)) {
      return fd;
   }

   close(fd);
outErr:
   return -1;
}

static bool
setTopology(int fd,
            const struct drm_vmw_rect *rects,
            unsigned int num_rects)
{
   struct drm_vmw_update_layout_arg arg;
   int ret;

   memset(&arg, 0, sizeof arg);
   arg.num_outputs = num_rects;
   arg.rects = (unsigned long) rects;

   ret = drmCommandWrite(fd, DRM_VMW_UPDATE_LAYOUT, &arg, sizeof arg);
   if (ret < 0) {
      fprintf(stderr, "%s, Error: write to kernel failed: %d\n",
              __func__, ret);
      return false;
   }

   return true;
}

static bool
parseRects(uint32_t num_rects,
           int arg,
           char **argv,
           struct drm_vmw_rect **rects)
{
   uint32_t j;
   /*
     * The argument string will look something like:
     *   WxH+x+y * count.
     *
     * e.g.
     *    640x480+0+0 640x480+640+0 640x480+1280+0
     * will set three 640x480 screens horizontally next to
     * each other (i.e. one 1920x480 viewport)
     */

   *rects = malloc(sizeof *rects[0] * num_rects);
   if (!*rects) {
      fprintf(stderr,
              "Failed to alloc buffer for display info");
      return false;
   }

   for (j = 0; j < num_rects; ++j) {
      char *p = argv[arg++];
      if (sscanf(p, "%ux%u+%d+%d", &(*rects)[j].w, &(*rects)[j].h,
                 &(*rects)[j].x, &(*rects)[j].y) != 4) {
         fprintf(stderr,
                 "Couldn't parse screen dimensions for topology #%d: '%s'. Expected WxH+x+y format (no spaces).\n",
                 j, p);
         return false;
      }
   }
   return true;
}

static const char *connector_type_names[] = {
   "unknown",
   "VGA",
   "DVI-I",
   "DVI-D",
   "DVI-A",
   "composite",
   "s-video",
   "LVDS",
   "component",
   "9-pin DIN",
   "DP",
   "HDMI-A",
   "HDMI-B",
   "TV",
   "eDP",
   "Virtual",
   "DSI",
   "DPI",
   "WRITEBACK"
};



static void
printProperty(int fd, int idx, drmModePropertyPtr props, uint64_t value)
{
   int j;

   printf("\t   %d: %s  (id=%i, flags=%i, count_values=%d)\n", idx, props->name,
          props->prop_id, props->flags, props->count_values);

   if (props->count_values) {
      printf("\t\tvalues       :");
      for (j = 0; j < props->count_values; j++) {
         printf(" %" PRIu64, props->values[j]);
      }
      printf("\n");
   }


   printf("\t\tcount_enums  : %d\n", props->count_enums);

   if (props->flags & DRM_MODE_PROP_BLOB) {
      drmModePropertyBlobPtr blob;

      blob = drmModeGetPropertyBlob(fd, value);
      if (blob) {
         printf("\t\tblob is %d length, %08X\n", blob->length, *(uint32_t *)blob->data);
         drmModeFreePropertyBlob(blob);
      } else {
         printf("\t\terror getting blob %" PRIu64 "\n", value);
      }

   } else {
      const char *name = NULL;
      for (j = 0; j < props->count_enums; j++) {
         printf("\t\t\t%lld = %s\n", props->enums[j].value, props->enums[j].name);
         if (props->enums[j].value == value) {
            name = props->enums[j].name;
         }
      }

      if (props->count_enums && name) {
         printf("\t\tcon_value    : %s\n", name);
      } else {
         printf("\t\tcon_value    : %" PRIu64 "\n", value);
      }
   }
}


static void
printMode(const struct drm_mode_modeinfo *mode, int idx, bool verbose)
{
   if (verbose) {
      printf("\t  %d: %s\n", idx, mode->name);
      printf("\t\tclock       : %i\n", mode->clock);
      printf("\t\thdisplay    : %i\n", mode->hdisplay);
      printf("\t\thsync_start : %i\n", mode->hsync_start);
      printf("\t\thsync_end   : %i\n", mode->hsync_end);
      printf("\t\thtotal      : %i\n", mode->htotal);
      printf("\t\thskew       : %i\n", mode->hskew);
      printf("\t\tvdisplay    : %i\n", mode->vdisplay);
      printf("\t\tvsync_start : %i\n", mode->vsync_start);
      printf("\t\tvsync_end   : %i\n", mode->vsync_end);
      printf("\t\tvtotal      : %i\n", mode->vtotal);
      printf("\t\tvscan       : %i\n", mode->vscan);
      printf("\t\tvrefresh    : %i\n", mode->vrefresh);
      printf("\t\tflags       : %i\n", mode->flags);
   } else {
      printf("\t  %d: \"%s\" %ix%i %i\n", idx,
             mode->name, mode->hdisplay,
             mode->vdisplay, mode->vrefresh);
   }
}


static const char *
drmModeConnectionToString(drmModeConnection modeConnection)
{
   switch (modeConnection) {
   case DRM_MODE_CONNECTED:
      return "DRM_MODE_CONNECTED";
   case DRM_MODE_DISCONNECTED:
      return "DRM_MODE_DISCONNECTED";
   case DRM_MODE_UNKNOWNCONNECTION:
      return "DRM_MODE_UNKNOWNCONNECTION";
   default:
      return "invalid";
   }
}


static void
printTopology(void)
{
   int i, j;
   int fd = vmwgfxOpen(false);
   if (fd < 0) {
      fprintf(stderr, "Wasn't able to open the drm device\n");
      exit(1);
   }

   drmModeResPtr res = drmModeGetResources(fd);
   if (res == 0) {
      printf("Failed to get resources from card\n");
      drmClose(fd);
      return;
   }

   printf("Resources\n");
   printf("  count_connectors : %i\n", res->count_connectors);
   printf("  count_encoders   : %i\n", res->count_encoders);
   printf("  count_crtcs      : %i\n", res->count_crtcs);
   printf("  count_fbs        : %i\n", res->count_fbs);
   printf("  min_size         : [%u, %u]\n", res->min_width, res->min_height);
   printf("  max_size         : [%u, %u]\n", res->max_width, res->max_height);
   printf("\n");

   for (i = 0; i < res->count_connectors; i++) {
      drmModeConnectorPtr connector = drmModeGetConnector(fd, res->connectors[i]);
      if (!connector) {
         printf("Could not get connector %i\n", res->connectors[i]);
         continue;
      }

      assert(ARRAYSIZE(connector_type_names) > connector->connector_type);
      const char *connector_type_name =
            connector_type_names[connector->connector_type];
      printf("Connector: %s-%d (%s)\n", connector_type_name,
             connector->connector_type_id,
             drmModeConnectionToString(connector->connection));

      if (connector->connection == DRM_MODE_CONNECTED) {
         printf("\tencoder id     : %i\n", connector->encoder_id);
         printf("\tsize           : %ix%i (mm)\n", connector->mmWidth, connector->mmHeight);
         printf("\tcount_modes    : %i\n", connector->count_modes);
         printf("\tcount_props    : %i\n", connector->count_props);

         if (connector->count_props) {
            printf("\tProperties:\n");
         }
         for (j = 0; j < connector->count_props; j++) {
            drmModePropertyPtr props = drmModeGetProperty(fd, connector->props[j]);
            if (props) {
               printProperty(fd, j, props, connector->prop_values[j]);
               drmModeFreeProperty(props);
            }
         }
         if (connector->count_modes) {
            printf("\tModes:\n");
         }
         for (j = 0; j < connector->count_modes; j++) {
            struct drm_mode_modeinfo *mode = (struct drm_mode_modeinfo *)&connector->modes[j];
            printMode(mode, j, false);
         }
      }

      drmModeFreeConnector(connector);
   }
   printf("\n");

   drmModeFreeResources(res);

   close(fd);
}

static void
run(int argc, char **argv)
{
   int i, j;
   for (i = 1; i < argc; ++i) {
      if (strcmp(argv[i], "--help") == 0) {
         printf("%s: \n", argv[0]);
         printf("\t--help prints out the help screen\n");
         printf("\t--print-topology prints out the currently set topology\n");
         printf("\t--set-topology WxH+x+y  (WxH+x+y repeated for each screen)\n");
         printf("\t\te.g. 640x480+0+0 800x480+640+0 640x800+0+480\n");
         return;
      } else if (strcmp(argv[i], "--set-topology") == 0) {
         struct drm_vmw_rect *rects;
         uint32_t num_rects = argc - i - 1;

         if (num_rects == 0) {
            fprintf(stderr, "%s: set-topology is missing the dimensions\n",
                    argv[0]);
            exit(1);
         }
         if (!parseRects(num_rects, i + 1, argv, &rects)) {
            exit(1);
         }

         printf("Setting topology for %d screens", num_rects);
         for (j = 0; j < num_rects; ++j) {
            printf(", [%d, %d, %u, %u]",
                   rects[j].x, rects[j].y,
                   rects[j].w, rects[j].h);
         }
         printf("\n");

         int fd = vmwgfxOpen(true);
         if (fd < 0) {
            fprintf(stderr, "Wasn't able to open the drm device\n");
            free(rects);
            exit(1);
         }
         setTopology(fd, rects, num_rects);
         close(fd);
         free(rects);
         return;
      } else if (strcmp(argv[i], "--print-topology") == 0) {
         printTopology();
         return;
      } else {
         fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
         exit(1);
      }
   }
}

int main(int argc, char **argv)
{
   run(argc, argv);

   return 0;
}

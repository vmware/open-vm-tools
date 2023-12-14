/*********************************************************
 * Copyright (C) 2016-2021 VMware, Inc. All rights reserved.
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

#ifdef ENABLE_RESOLUTIONKMS
#ifndef HAVE_LIBUDEV

#include "resolutionDL.h"
#include "vmware.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>

#define G_LOG_DOMAIN "resolutionCommon"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"


struct FuncToResolv {
   size_t offset;
   const char *name;
};

#define UDEV_RESOLV(_name) \
   {.offset = offsetof(struct Udev1Interface, _name), \
    .name = "udev"#_name}

#define LIBDRM_RESOLV(_name) \
   {.offset = offsetof(struct Drm2Interface, _name), \
    .name = "drm"#_name}


static struct FuncToResolv udev1Table[] = {
   UDEV_RESOLV(_device_get_devnode),
   UDEV_RESOLV(_device_get_parent_with_subsystem_devtype),
   UDEV_RESOLV(_device_get_sysattr_value),
   UDEV_RESOLV(_device_new_from_syspath),
   UDEV_RESOLV(_device_unref),
   UDEV_RESOLV(_enumerate_add_match_property),
   UDEV_RESOLV(_enumerate_add_match_subsystem),
   UDEV_RESOLV(_enumerate_get_list_entry),
   UDEV_RESOLV(_enumerate_new),
   UDEV_RESOLV(_enumerate_scan_devices),
   UDEV_RESOLV(_enumerate_unref),
   UDEV_RESOLV(_list_entry_get_name),
   UDEV_RESOLV(_list_entry_get_next),
   UDEV_RESOLV(_new),
   UDEV_RESOLV(_unref)
};

static struct FuncToResolv drm2Table[] = {
   LIBDRM_RESOLV(Open),
   LIBDRM_RESOLV(Close),
   LIBDRM_RESOLV(GetVersion),
   LIBDRM_RESOLV(FreeVersion),
   LIBDRM_RESOLV(DropMaster),
   LIBDRM_RESOLV(CommandWrite)
};

struct Udev1Interface *udevi = NULL;
struct Drm2Interface *drmi = NULL;

static void *dlhandle;

/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDLClose --
 *
 *     Removes any dynamic library reference and frees any resource
 *     allocated by resolutionDLOpen.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
void
resolutionDLClose(void)
{
   if (udevi) {
      free(udevi);
      udevi = NULL;
   }

   if (drmi) {
      free(drmi);
      drmi = NULL;
   }

   if (dlhandle) {
      dlclose(dlhandle);
      dlhandle = NULL;
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDLResolve --
 *
 *     Tries to open and resolve needed symbols of a single shared library.
 *
 * Results:
 *     If succesful returns zero, otherwise returns -1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
static int
resolutionDLResolve(void **ptr,                        // OUT: pointer to
                                                       // function table.
                    size_t size,                       // IN: Size of ft.
                    const char name[],                 // IN: Library name.
                    const struct FuncToResolv table[], // IN: Table of name-
                                                       // offset pairs
                    int numEntries)                    // IN: Num entries in
                                                       // table.
{
   void **func_ptr;
   int i;

   if (*ptr)
      return 0;

   *ptr = malloc(size);
   if (!*ptr)
      return -1;

   dlhandle = dlopen(name, RTLD_NOW);
   if (!dlhandle) {
      g_debug("%s: Failed to open shared library \"%s\".\n", __func__,
              name);
      goto out_err;
   }

   for (i = 0; i < numEntries; ++i) {
      func_ptr = (void *) ((unsigned long) *ptr + table[i].offset);
      *func_ptr = dlsym(dlhandle, table[i].name);
      if (!*func_ptr) {
         g_debug("%s: Failed to resolve %s symbol \"%s\".\n", __func__,
                 name,table[i].name);
         goto out_err;
      }
   }

   return 0;

out_err:
   resolutionDLClose();

   return -1;
}

/*
 *-----------------------------------------------------------------------------
 *
 * resolutionDLOpen --
 *
 *     Tries to open and create a reference to distribution shared libraries
 *     needed for the resolutionKMS functionality.
 *
 * Results:
 *     If succesful returns zero, otherwise returns -1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */
int
resolutionDLOpen(void)
{
   /* We support libudev major versions 0 and 1 for now. */
   if (resolutionDLResolve((void **)&udevi, sizeof(*udevi), "libudev.so.1",
                           udev1Table, ARRAYSIZE(udev1Table)) &&
       resolutionDLResolve((void **)&udevi, sizeof(*udevi), "libudev.so.0",
                           udev1Table, ARRAYSIZE(udev1Table)))
      return -1;

   if (resolutionDLResolve((void **)&drmi, sizeof(*drmi), "libdrm.so.2",
                           drm2Table, ARRAYSIZE(drm2Table)))
      return -1;

   return 0;
}

#endif /* !HAVE_LIBUDEV */
#endif /* ENABLE_RESOLUTIONKMS */

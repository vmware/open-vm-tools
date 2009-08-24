/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * module.c --
 *
 *   Module loading/unloading functions.
 *
 */

#include "driver-config.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/limits.h>
#include <linux/errno.h>

#include "vmblockInt.h"
#include "vmblock_version.h"

/* Module parameters */
#ifdef VMX86_DEVEL /* { */
int LOGLEVEL_THRESHOLD = 4;
module_param(LOGLEVEL_THRESHOLD, int, 0600);
MODULE_PARM_DESC(LOGLEVEL_THRESHOLD, "Logging level (0 means no log, "
                 "10 means very verbose, 4 is default)");
#endif /* } */

static char *root = "/tmp/VMwareDnD";
module_param(root, charp, 0600);
MODULE_PARM_DESC(root, "The directory the file system redirects to.");

/* Module information */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Blocking File System");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMBLOCK_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");

/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInit --
 *
 *    Module entry point and initialization.
 *
 * Results:
 *    Zero on success, negative value on failure.
 *
 * Side effects:
 *    /proc entries are available and file system is registered with kernel and
 *    ready to be mounted.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockInit(void)
{
   int ret;

   ret = VMBlockInitControlOps();
   if (ret < 0) {
      goto error;
   }

   ret = VMBlockInitFileSystem(root);
   if (ret < 0) {
      VMBlockCleanupControlOps();
      goto error;
   }

   LOG(4, "module loaded\n");
   return 0;

error:
   Warning("VMBlock: could not initialize module\n");
   return ret;
}

module_init(VMBlockInit);


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockExit --
 *
 *    Unloads module from kernel and removes associated state.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Opposite of VMBlockInit(): /proc entries go away and file system is
 *    unregistered.
 *
 *----------------------------------------------------------------------------
 */

static void
VMBlockExit(void)
{
   VMBlockCleanupControlOps();
   VMBlockCleanupFileSystem();

   LOG(4, "module unloaded\n");
}

module_exit(VMBlockExit);

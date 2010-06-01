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
 * Module-specific components of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include "compat_module.h"

#include "filesystem.h"
#include "module.h"
#include "vmhgfs_version.h"

#ifdef VMX86_DEVEL
/*
 * Logging is available only in devel build.
 */

int LOGLEVEL_THRESHOLD = 4;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param(LOGLEVEL_THRESHOLD, int, 0444);
#else
MODULE_PARM(LOGLEVEL_THRESHOLD, "i");
#endif

MODULE_PARM_DESC(LOGLEVEL_THRESHOLD, "Set verbosity (0 means no log, 10 means very verbose, 4 is default)");
#endif

char *HOST_IP = NULL;
int HOST_PORT = 2000; /* Defaulted to 2000. */
int HOST_VSOCKET_PORT = 0; /* Disabled by default. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param(HOST_IP, charp, 0444);
module_param(HOST_PORT, int, 0444);
module_param(HOST_VSOCKET_PORT, int, 0444);
#else
MODULE_PARM(HOST_IP, "s");
MODULE_PARM(HOST_PORT, "i");
MODULE_PARM(HOST_VSOCKET_PORT, "i");
#endif

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Host/Guest File System");
MODULE_VERSION(VMHGFS_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");


/*
 *----------------------------------------------------------------------
 *
 * init_module --
 *
 *    linux module entry point. Called by /sbin/insmod command.
 *    Sets up internal state and registers the hgfs filesystem
 *    with the kernel.
 *
 * Results:
 *    Returns 0 on success, an error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
init_module(void)
{
   return HgfsInitFileSystem() ? 0 : -EBUSY;
}


/*
 *----------------------------------------------------------------------
 *
 * cleanup_module --
 *
 *    Called by /sbin/rmmod. Unregisters filesystem with kernel,
 *    cleans up internal state, and unloads module.
 *
 *    Note: for true kernel 2.4 compliance, this should be
 *    "module_exit".
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
cleanup_module(void)
{
   HgfsCleanupFileSystem();
}

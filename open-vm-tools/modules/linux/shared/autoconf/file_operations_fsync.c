/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * Linux v3.1 added 2 params to fsync for fine-grained locking control.
 * But SLES11 SP2 has backported the change to its 3.0 kernel,
 * so we can't rely solely on kernel version to determine number of
 * arguments.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0)
#   error This compile test intentionally fails.
#else

#include <linux/fs.h>
#include <linux/types.h>  /* loff_t */

static int TestFsync(struct file *file,
                     loff_t start, loff_t end,
                      int datasync)
{
   return 0;
}

struct file_operations testFO = {
   .fsync = TestFsync,
};

#endif

/*********************************************************
 * Copyright (C) 2013-2014 VMware, Inc. All rights reserved.
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
 * Linux v2.6.18 added an owner parameter to flush.
 * But SLES10 has backported the change to its 2.6.16.60 kernel,
 * so we can't rely solely on kernel version to determine number of
 * arguments.
 *
 * This test will fail on a kernel with such a patch.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#error This compile test intentionally fails on 2.6.18 and newer kernels.
#else

#include <linux/fs.h>

static int TestFlush(struct file *file);
{
   return 0;
}

struct file_operations testFO = {
   .flush = TestFlush,
};

#endif

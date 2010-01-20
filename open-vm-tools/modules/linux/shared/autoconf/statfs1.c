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

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#include <linux/fs.h>

/*
 * Around 2.6.18, the super_block pointer in statfs was changed to a dentry
 * pointer. Red Hat backported this behavior into a 2.6.17 kernel.
 *
 * This test will fail on a kernel with such a patch.
 */
static int LinuxDriverStatFs(struct super_block *sb,
                             struct kstatfs *stat) 
{
   return 0;
}

struct super_operations super_ops = {
   .statfs = LinuxDriverStatFs
};
#else
#error "This test intentionally fails on 2.6.19 and newer kernels."
#endif

/*********************************************************
 * Copyright (C) 2015-2016 VMware, Inc. All rights reserved.
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/types.h>  /* loff_t */

/*
 * After 3.12.0, truncate_pagecache changed its interface to just use
 * the new file size only. Red Hat backported this behavior into a 3.10.0
 * kernel.
 *
 * This test will fail on a kernel with such a patch.
 */

void test(void)
{
   struct inode inode;
   loff_t oldSize = 0;
   loff_t newSize = 4096;

   truncate_pagecache(&inode, oldSize, newSize);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
#error "This test intentionally fails on 3.12.0 and newer kernels."
#else
/*
 * It must be older than 2.6.32 in which case we assume success.
 * So not 3.12 compatible. There is no function for these versions.
 */
#endif

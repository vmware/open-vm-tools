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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
#include <linux/fs.h>
#include <linux/types.h>  /* loff_t */
#include <linux/stddef.h> /* NULL */

/*
 * After 2.6.18, filldir and statfs were changed to send 64-bit inode
 * numbers to user space. Red Hat backported this behavior into a 2.6.17
 * kernel.
 *
 * This test will fail on a kernel with such a patch.
 */
static int LinuxDriverFilldir(void *buf,
                              const char *name,
                              int namelen,
                              loff_t offset,
                              ino_t ino,
                              unsigned int d_type)
{
   return 0;
}

void test(void)
{
   vfs_readdir(NULL, LinuxDriverFilldir, NULL);
}
#else
#error "This test intentionally fails on 2.6.20 and newer kernels."
#endif

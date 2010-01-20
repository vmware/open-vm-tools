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
#include <linux/stddef.h> /* NULL */


/*
 * After 2.6.18, inodes were "slimmed". This involved removing the union
 * that encapsulates inode private data (and using i_private instead), as well
 * as removing i_blksize. Red Hat backported this behavior into a 2.6.17
 * kernel.
 *
 * This test will fail on a kernel with such a patch.
 */
void test(void)
{
   struct inode inode;

   inode.u.generic_ip = NULL;
}
#else
#error "This test intentionally fails on 2.6.20 and newer kernels."
#endif

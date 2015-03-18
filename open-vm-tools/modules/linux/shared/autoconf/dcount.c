/*********************************************************
 * Copyright (C) 2014 VMware, Inc. All rights reserved.
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
#include <linux/dcache.h>

/*
 * After 3.11.0, the dentry d_count field was removed. Red Hat
 * backported this behavior into a 3.10.0 kernel.
 *
 * This test will fail on a kernel with such a patch.
 */
void test(void)
{
   struct dentry dentry;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
   dentry.d_count = 1;
#else
   atomic_set(&dentry.d_count, 1);
#endif
}
#else
#error "This test intentionally fails on 3.11.0 or newer kernels."
#endif

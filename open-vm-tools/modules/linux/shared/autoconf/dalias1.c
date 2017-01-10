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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
#include <linux/dcache.h>
#include <linux/list.h>

/*
 * After 3.19.0, the dentry d_alias field was moved. Fedora
 * backported this behavior into earlier kernels.
 * The type of the d_alias field changed from 3.6 onwards
 * which was a list head to being a list node. The check
 * for 3.6 onwards is done separately.
 *
 * This test will fail on a kernel with such a patch.
 */
void test(void)
{
   struct dentry aliasDentry;

   INIT_LIST_HEAD(&aliasDentry.d_alias);
}

#else
/*
 * Intentionally passes for earlier than 3.2.0 kernels as d_alias is valid.
 *
 * Intentionally passes for 3.6.0 or later kernels as d_alias is a different type.
 * A separate test with the different type is run for those kernel versions.
 */
#endif

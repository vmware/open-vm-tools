/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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

/*
 * In 2.4.3, the s_maxbytes field was added to struct super_block.
 * However, we can't simply condition on 2.4.3 as the field's starting point
 * because the 2.4.2-2 kernel in RH7.1 also contained it, and if we don't set 
 * it, the generic write path in the page cache will fail.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 3)

#include <linux/fs.h>

void test(void)
{
   struct super_block sb;

   sb.s_maxbytes = 0;
}
#endif

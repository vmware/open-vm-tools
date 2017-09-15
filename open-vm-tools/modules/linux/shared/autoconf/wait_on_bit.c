/*********************************************************
 * Copyright (C) 2016 VMware, Inc. All rights reserved.
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)

#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>

unsigned long test_bits;

/*
 * After 3.17.0, wait_on_bit changed its interface to remove the action
 * callback argument and this was backported to some Linux kernel versions
 * such as 3.10 for the RHEL 7.3 version.
 *
 * This test will fail on a kernel with such a patch.
 */

int test(void)
{

   return wait_on_bit(&test_bits,
                      0,
                      NULL,
                      TASK_UNINTERRUPTIBLE);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
#error "This test intentionally fails on 3.17.0 and newer kernels."
#else
/*
 * It must be older than 2.6.13 in which case we don't use the function.
 */
#endif

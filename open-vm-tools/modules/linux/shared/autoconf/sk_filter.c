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

/*
 * Detect whether the old or new sk_filter() interface is used.  This was
 * changed in 2.4.21, but it's backported to some distro kernels.
 *
 * This test will fail to build on kernels with the new interface.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

/*
 * We'd restrict this test to 2.4.21 and earlier kernels, but Mandrake's
 * enterprise-2.4.21-013mdk-9.1 appears to really be 2.4.20 with some patches,
 * and not the patches we care about, so let's test on 2.4.21 kernels too.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 22)
#include <linux/stddef.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/filter.h>

struct sk_buff test_skbuff;
struct sk_filter test_filter;

int
sk_filter_test(void)
{
   struct sk_buff *skb = &test_skbuff;
   struct sk_filter *filter = &test_filter;

   return sk_filter(skb, filter);
}
#else
#error "This test intentionally fails on 2.4.22 or newer kernels."
#endif

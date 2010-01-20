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

/*
 * Detect whether skb_linearize takes one or two arguments.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
/*
 * Since 2.6.18 all kernels have single-argument skb_linearize.  For
 * older kernels use autodetection.  Not using autodetection on newer
 * kernels saves us from compile failure on some post 2.6.18 kernels
 * which do not have selfcontained skbuff.h.
 */

#include <linux/skbuff.h>

int test_skb_linearize(struct sk_buff *skb)
{
   return skb_linearize(skb);
}

#endif

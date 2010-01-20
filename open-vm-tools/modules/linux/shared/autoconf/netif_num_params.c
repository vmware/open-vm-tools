/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * Detect whether netif_rx_complete (and netif_rx_schedule) take a single
 * napi_struct argument. The foundation was laid whith introducing Generic
 * Receive Offload infrastructure but dropping unneeded net_device argument
 * did not happen till few commits later so we can't simply test for presence
 * of NETIF_F_GRO.
 *
 * Test succeeds if netif_rx_complete takes dev & napi arguments, or if it
 * takes dev argument only (kernels before 2.6.24).  Test fails if netif_rx_complete
 * takes only single napi argument.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
#   error This compile test intentionally fails.
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
#include <linux/netdevice.h>

#ifdef NETIF_F_GRO
void test_netif_rx_complete(struct net_device *dev, struct napi_struct *napi)
{
   netif_rx_complete(dev, napi);
}
#endif

#endif

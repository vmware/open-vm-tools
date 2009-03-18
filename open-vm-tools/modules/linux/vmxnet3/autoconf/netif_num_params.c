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
 * Receive Offload infrastructure but sropping unneeded net_device argument
 * did not happen till few commits later so we can't simply test for presence
 * of NETIF_F_GRO.
 */

#include <linux/autoconf.h>
#include <linux/netdevice.h>

#ifndef NETIF_F_GRO
#   error This compile test intentionally fails.
#endif

void test_netif_rx_complete(struct napi_struct *napi)
{
   netif_rx_complete(napi);
}

/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

/************************************************************
 *
 *   net.h
 *
 *   This file should contain all network global defines.
 *   No vlance/vmxnet/vnet/vmknet specific stuff should be
 *   put here only defines used/usable by all network code.
 *   --gustav
 *
 ************************************************************/

#ifndef VMWARE_DEVICES_NET_H
#define VMWARE_DEVICES_NET_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMEXT
#include "includeCheck.h"
#include "vm_device_version.h"

#define ETHERNET_MTU         1518
#define ETH_MIN_FRAME_LEN      60

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN          6  /* length of MAC address */
#endif
#define ETH_HEADER_LEN	       14  /* length of Ethernet header */
#define IP_ADDR_LEN	        4  /* length of IPv4 address */
#define IP_HEADER_LEN	       20  /* minimum length of IPv4 header */

#define ETHER_MAX_QUEUED_PACKET 1600


/* 
 * State's that a NIC can be in currently we only use this 
 * in VLance but if we implement/emulate new adapters that
 * we also want to be able to morph a new corresponding 
 * state should be added.
 */

#define LANCE_CHIP  0x2934
#define VMXNET_CHIP 0x4392

/* 
 * Size of reserved IO space needed by the LANCE adapter and
 * the VMXNET adapter. If you add more ports to Vmxnet than
 * there is reserved space you must bump VMXNET_CHIP_IO_RESV_SIZE.
 * The sizes must be powers of 2.
 */

#define LANCE_CHIP_IO_RESV_SIZE  0x20 
#define VMXNET_CHIP_IO_RESV_SIZE 0x40

#define MORPH_PORT_SIZE 4

#endif // VMWARE_DEVICES_NET_H


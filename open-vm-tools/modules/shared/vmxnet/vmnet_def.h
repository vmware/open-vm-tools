/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * vmnet_def.h 
 *
 *     - definitions which are (mostly) not vmxnet or vlance specific
 */

#ifndef _VMNET_DEF_H_
#define _VMNET_DEF_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#define VMNET_NAME_BUFFER_LEN  128 /* Increased for i18n. */
#define VMNET_COAL_SCHEME_NAME_LEN 16


/*
 * capabilities - not all of these are implemented in the virtual HW
 *                (eg VLAN support is in the virtual switch)  so even vlance 
 *                can use them
 */
#define VMNET_CAP_SG	        0x0001	/* Can do scatter-gather transmits. */
#define VMNET_CAP_IP4_CSUM      0x0002	/* Can checksum only TCP/UDP over IPv4. */
#define VMNET_CAP_HW_CSUM       0x0004	/* Can checksum all packets. */
#define VMNET_CAP_HIGH_DMA      0x0008	/* Can DMA to high memory. */
#define VMNET_CAP_TOE	        0x0010	/* Supports TCP/IP offload. */
#define VMNET_CAP_TSO	        0x0020	/* Supports TCP Segmentation offload */
#define VMNET_CAP_SW_TSO        0x0040	/* Supports SW TCP Segmentation */
#define VMNET_CAP_VMXNET_APROM  0x0080	/* Vmxnet APROM support */
#define VMNET_CAP_HW_TX_VLAN    0x0100  /* Can we do VLAN tagging in HW */
#define VMNET_CAP_HW_RX_VLAN    0x0200  /* Can we do VLAN untagging in HW */
#define VMNET_CAP_SW_VLAN       0x0400  /* Can we do VLAN tagging/untagging in SW */
#define VMNET_CAP_WAKE_PCKT_RCV 0x0800  /* Can wake on network packet recv? */
#define VMNET_CAP_ENABLE_INT_INLINE 0x1000  /* Enable Interrupt Inline */
#define VMNET_CAP_ENABLE_HEADER_COPY 0x2000  /* copy header for vmkernel */
#define VMNET_CAP_TX_CHAIN      0x4000  /* Guest can use multiple tx entries for a pkt */
#define VMNET_CAP_RX_CHAIN      0x8000  /* a pkt can span multiple rx entries */
#define VMNET_CAP_LPD           0x10000 /* large pkt delivery */
#define VMNET_CAP_BPF           0x20000 /* BPF Support in VMXNET Virtual Hardware */
#define VMNET_CAP_SG_SPAN_PAGES 0x40000	/* Can do scatter-gather span multiple pages transmits. */
#define VMNET_CAP_IP6_CSUM      0x80000	/* Can do IPv6 csum offload. */
#define VMNET_CAP_TSO6         0x100000	/* Can do TSO segmentation offload for IPv6 pkts. */
#define VMNET_CAP_TSO256k      0x200000	/* Can do TSO segmentation offload for pkts up to 256kB. */
#define VMNET_CAP_UPT          0x400000	/* Support UPT */
#define VMNET_CAP_RDONLY_INETHDRS 0x800000 /* Modifies inet headers for TSO/CSUm */
#define VMNET_CAP_NPA         0x1000000	/* Support NPA */
#define VMNET_CAP_DCB         0x2000000	/* Support DCB */
#define VMNET_CAP_OFFLOAD_8OFFSET 0x4000000 /* supports 8bit parameterized offsets */ 
#define VMNET_CAP_OFFLOAD_16OFFSET 0x8000000 /* supports 16bit parameterized offsets */ 
#define VMNET_CAP_IP6_CSUM_EXT_HDRS 0x10000000 /* support csum of ip6 ext hdrs */
#define VMNET_CAP_TSO6_EXT_HDRS 0x20000000 /* support TSO for ip6 ext hdrs */
#define VMNET_CAP_SCHED       0x40000000 /* compliant with network scheduling */
#endif // _VMNET_DEF_H_

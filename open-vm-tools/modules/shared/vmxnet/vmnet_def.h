/*********************************************************
 * Copyright (C) 2004-2014,2017-2019 VMware, Inc. All rights reserved.
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
#define VMNET_COAL_STRING_LEN  128


/*
 * capabilities - not all of these are implemented in the virtual HW
 *                (eg VLAN support is in the virtual switch)  so even vlance
 *                can use them
 */
#define VMNET_CAP_SG                   CONST64U(0x0001)             /* Can do scatter-gather transmits. */
#define VMNET_CAP_IP4_CSUM             CONST64U(0x0002)             /* Can checksum only TCP/UDP over IPv4. */
#define VMNET_CAP_HW_CSUM              CONST64U(0x0004)             /* Can checksum all packets. */
#define VMNET_CAP_HIGH_DMA             CONST64U(0x0008)             /* Can DMA to high memory. */
#define VMNET_CAP_TOE                  CONST64U(0x0010)             /* Supports TCP/IP offload. */
#define VMNET_CAP_TSO                  CONST64U(0x0020)             /* Supports TCP Segmentation offload */
#define VMNET_CAP_SW_TSO               CONST64U(0x0040)             /* Supports SW TCP Segmentation */
#define VMNET_CAP_VMXNET_APROM         CONST64U(0x0080)             /* Vmxnet APROM support */
#define VMNET_CAP_HW_TX_VLAN           CONST64U(0x0100)             /* Can we do VLAN tagging in HW */
#define VMNET_CAP_HW_RX_VLAN           CONST64U(0x0200)             /* Can we do VLAN untagging in HW */
#define VMNET_CAP_SW_VLAN              CONST64U(0x0400)             /* Can we do VLAN tagging/untagging in SW */
#define VMNET_CAP_WAKE_PCKT_RCV        CONST64U(0x0800)             /* Can wake on network packet recv? */
#define VMNET_CAP_ENABLE_INT_INLINE    CONST64U(0x1000)             /* Enable Interrupt Inline */
#define VMNET_CAP_ENABLE_HEADER_COPY   CONST64U(0x2000)             /* copy header for vmkernel */
#define VMNET_CAP_TX_CHAIN             CONST64U(0x4000)             /* Guest can use multiple tx entries for a pkt */
#define VMNET_CAP_RX_CHAIN             CONST64U(0x8000)             /* a pkt can span multiple rx entries */
#define VMNET_CAP_LPD                  CONST64U(0x10000)            /* large pkt delivery */
#define VMNET_CAP_BPF                  CONST64U(0x20000)            /* BPF Support in VMXNET Virtual Hardware */
#define VMNET_CAP_SG_SPAN_PAGES        CONST64U(0x40000)            /* Can do scatter-gather span multiple pages transmits. */
#define VMNET_CAP_IP6_CSUM             CONST64U(0x80000)            /* Can do IPv6 csum offload. */
#define VMNET_CAP_TSO6                 CONST64U(0x100000)           /* Can do TSO segmentation offload for IPv6 pkts. */
#define VMNET_CAP_TSO256k              CONST64U(0x200000)           /* Can do TSO segmentation offload for pkts up to 256kB. */
#define VMNET_CAP_UPT                  CONST64U(0x400000)           /* Support UPT */
#define VMNET_CAP_RDONLY_INETHDRS      CONST64U(0x800000)           /* Modifies inet headers for TSO/CSUm */
#define VMNET_CAP_ENCAP                CONST64U(0x1000000)          /* NPA not used, so redefining for ENCAP support */
#define VMNET_CAP_DCB                  CONST64U(0x2000000)          /* Support DCB */
#define VMNET_CAP_OFFSET_BASED_OFFLOAD CONST64U(0x4000000)       /* Support offload based offload */
#define VMNET_CAP_GENEVE_OFFLOAD       CONST64U(0x8000000)          /* Support Geneve encapsulation offload */
#define VMNET_CAP_IP6_CSUM_EXT_HDRS    CONST64U(0x10000000)         /* support csum of ip6 ext hdrs */
#define VMNET_CAP_TSO6_EXT_HDRS        CONST64U(0x20000000)         /* support TSO for ip6 ext hdrs */
#define VMNET_CAP_SCHED                CONST64U(0x40000000)         /* compliant with network scheduling */
#define VMNET_CAP_SRIOV                CONST64U(0x80000000)         /* Supports SR-IOV */

#define VMNET_CAP_SG_TX                VMNET_CAP_SG
#define VMNET_CAP_SG_RX                CONST64U(0x200000000)        /* Scatter-gather receive capability */
#define VMNET_CAP_PRIV_STATS           CONST64U(0x400000000)        /* Driver supports accessing private stats */
#define VMNET_CAP_LINK_STATUS_SET      CONST64U(0x800000000)        /* Driver supports changing link status */
#define VMNET_CAP_MAC_ADDR_SET         CONST64U(0x1000000000)       /* Driver supports changing the interface MAC address */
#define VMNET_CAP_COALESCE_PARAMS      CONST64U(0x2000000000)       /* Driver supports changing interrupt coalescing parameters */
#define VMNET_CAP_VLAN_FILTER          CONST64U(0x4000000000)       /* VLAN Filtering capability */
#define VMNET_CAP_WAKE_ON_LAN          CONST64U(0x8000000000)       /* Wake-On-LAN capability */
#define VMNET_CAP_NETWORK_DUMP         CONST64U(0x10000000000)      /* Network core dumping capability */
#define VMNET_CAP_MULTI_QUEUE          CONST64U(0x20000000000)      /* Multiple queue capability */
#define VMNET_CAP_EEPROM               CONST64U(0x40000000000)      /* EEPROM dump capability */
#define VMNET_CAP_REGDUMP              CONST64U(0x80000000000)      /* Register dump capability */
#define VMNET_CAP_SELF_TEST            CONST64U(0x100000000000)     /* Self-test capability */
#define VMNET_CAP_PAUSE_PARAMS         CONST64U(0x200000000000)     /* Pause frame parameter adjusting */
#define VMNET_CAP_RESTART_NEG          CONST64U(0x400000000000)     /* Ability to restart negotiation of link speed/duplexity */
#define VMNET_CAP_LRO                  CONST64U(0x800000000000)     /* Hardware supported LRO */
#define VMNET_CAP_OFFLOAD_ALIGN_ANY    CONST64U(0x1000000000000)    /* Nic requires no header alignment */
#define VMNET_CAP_GENERIC_OFFLOAD      CONST64U(0x2000000000000)    /* Generic hardware offloading (eg. vxlan encap offload and offset based offload) */
#define VMNET_CAP_CABLE_TYPE           CONST64U(0x4000000000000)    /* Uplink supports getting and setting cable type. */
#define VMNET_CAP_PHY_ADDRESS          CONST64U(0x8000000000000)    /* Uplink supports getting and setting PHY address. */
#define VMNET_CAP_TRANSCEIVER_TYPE     CONST64U(0x10000000000000)   /* Uplink supports getting and setting transceiver type. */
#define VMNET_CAP_MESSAGE_LEVEL        CONST64U(0x20000000000000)   /* Uplink supports getting and setting message level. */
#define VMNET_CAP_RING_PARAMS          CONST64U(0x40000000000000)   /* Support getting/setting RX/TX ring size parameters */
#define VMNET_CAP_ADVERTISE_MODES      CONST64U(0x80000000000000)   /* Support getting/setting interconnect modes */
#define VMNET_CAP_HW_DCB               CONST64U(0x100000000000000)  /* DCB capability in hardware */
#define VMNET_CAP_RX_SW_LRO            CONST64U(0x200000000000000)  /* Support SW LRO */
#define VMNET_CAP_ENS                  CONST64U(0x400000000000000)  /* Support ENS */
#define VMNET_CAP_FPO                  CONST64U(0x800000000000000)  /* Support FPO */
#define VMNET_CAP_LEGACY               CONST64U(0x8000000000000000) /* Uplink is compatible with vmklinux drivers */

#endif // _VMNET_DEF_H_

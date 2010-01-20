/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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

#ifndef _VMXNET_DEF_H_
#define _VMXNET_DEF_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "net_sg.h"
#include "vmnet_def.h"


/*
 *   Vmxnet I/O ports, used by both the vmxnet driver and 
 *   the device emulation code.
 */

#define VMXNET_INIT_ADDR		0x00
#define VMXNET_INIT_LENGTH		0x04
#define VMXNET_TX_ADDR		        0x08
#define VMXNET_COMMAND_ADDR		0x0c
#define VMXNET_MAC_ADDR			0x10
#define VMXNET_LOW_VERSION		0x18
#define VMXNET_HIGH_VERSION		0x1c
#define VMXNET_STATUS_ADDR		0x20
#define VMXNET_TOE_INIT_ADDR            0x24
#define VMXNET_APROM_ADDR               0x28
#define VMXNET_INT_ENABLE_ADDR          0x30
#define VMXNET_WAKE_PKT_PATTERNS        0x34

/*
 * Vmxnet command register values.
 */
#define VMXNET_CMD_INTR_ACK		0x0001
#define VMXNET_CMD_UPDATE_LADRF		0x0002
#define VMXNET_CMD_UPDATE_IFF		0x0004
#define VMXNET_CMD_UNUSED 1		0x0008
#define VMXNET_CMD_UNUSED_2		0x0010
#define VMXNET_CMD_INTR_DISABLE  	0x0020
#define VMXNET_CMD_INTR_ENABLE   	0x0040
#define VMXNET_CMD_UNUSED_3		0x0080
#define VMXNET_CMD_CHECK_TX_DONE	0x0100
#define VMXNET_CMD_GET_NUM_RX_BUFFERS	0x0200
#define VMXNET_CMD_GET_NUM_TX_BUFFERS	0x0400
#define VMXNET_CMD_PIN_TX_BUFFERS	0x0800
#define VMXNET_CMD_GET_CAPABILITIES	0x1000
#define VMXNET_CMD_GET_FEATURES		0x2000
#define VMXNET_CMD_SET_POWER_FULL       0x4000
#define VMXNET_CMD_SET_POWER_LOW        0x8000

/*
 * Vmxnet status register values.
 */
#define VMXNET_STATUS_CONNECTED		0x0001
#define VMXNET_STATUS_ENABLED		0x0002
#define VMXNET_STATUS_TX_PINNED         0x0004

/*
 * Values for the interface flags.
 */
#define VMXNET_IFF_PROMISC		0x01
#define VMXNET_IFF_BROADCAST		0x02
#define VMXNET_IFF_MULTICAST		0x04
#define VMXNET_IFF_DIRECTED             0x08

/*
 * Length of the multicast address filter.
 */
#define VMXNET_MAX_LADRF		2

/*
 * Size of Vmxnet APROM. 
 */
#define VMXNET_APROM_SIZE 6

/*
 * An invalid ring index.
 */
#define VMXNET_INVALID_RING_INDEX	(-1)

/*
 * Features that are implemented by the driver.  These are driver
 * specific so not all features will be listed here.  In addition not all
 * drivers have to pay attention to these feature flags.
 *
 *  VMXNET_FEATURE_ZERO_COPY_TX 	The driver won't do any copies as long as
 *					the packet length is > 
 *					Vmxnet_DriverData.minTxPhysLength.
 * 
 *  VMXNET_FEATURE_TSO                  The driver will use the TSO capabilities
 *                                      of the underlying hardware if available 
 *                                      and enabled.
 *
 *  VMXNET_FEATURE_JUMBO_FRAME          The driver can send/rcv jumbo frame 
 *
 *  VMXNET_FEATURE_LPD                  The backend can deliver large pkts
 */
#define VMXNET_FEATURE_ZERO_COPY_TX             0x01
#define VMXNET_FEATURE_TSO                      0x02
#define VMXNET_FEATURE_JUMBO_FRAME              0x04
#define VMXNET_FEATURE_LPD                      0x08

/*
 * Define the set of capabilities required by each feature above
 */
#define VMXNET_FEATURE_ZERO_COPY_TX_CAPS        VMXNET_CAP_SG
#define VMXNET_FEATURE_TSO_CAPS                 VMXNET_CAP_TSO
#define VMXNET_HIGHEST_FEATURE_BIT              VMXNET_FEATURE_TSO

#define VMXNET_INC(val, max)     \
   val++;                        \
   if (UNLIKELY(val == max)) {   \
      val = 0;                   \
   }

/*
 * code that just wants to switch on the different versions of the
 * guest<->implementation protocol can cast driver data to this.
 */
typedef uint32 Vmxnet_DDMagic;

/*
 * Wake packet pattern commands sent through VMXNET_WAKE_PKT_PATTERNS port
 */

#define VMXNET_PM_OPCODE_START 3 /* args: cnt of wake packet patterns */
#define VMXNET_PM_OPCODE_LEN   2 /* args: index of wake packet pattern */
                                 /*       number of pattern byte values */
#define VMXNET_PM_OPCODE_DATA  1 /* args: index of wake packet pattern */
                                 /*       offset in pattern byte values list */
                                 /*       packet byte offset */
                                 /*       packet byte value */
#define VMXNET_PM_OPCODE_END   0 /* args: <none> */

typedef union Vmxnet_WakePktCmd {
   uint32 pktData : 32;
   struct {
      unsigned cmd : 2; /* wake packet pattern cmd [from list above] */
      unsigned cnt : 3; /* cnt wk pkt pttrns 1..MAX_NUM_FILTER_PTTRNS */
      unsigned ind : 3; /* ind wk pkt pttrn 0..MAX_NUM_FILTER_PTTRNS-1 */
      unsigned lenOff : 8; /* num pttrn byte vals 1..MAX_PKT_FILTER_SIZE */
                           /* OR offset in pattern byte values list */
                           /* 0..MAX_PKT_FILTER_SIZE-1 */
      unsigned byteOff : 8; /* pkt byte offset 0..MAX_PKT_FILTER_SIZE-1 */
      unsigned byteVal : 8; /* packet byte value 0..255 */
   } pktPttrn;
} Vmxnet_WakePktCmd;

#endif /* _VMXNET_DEF_H_ */

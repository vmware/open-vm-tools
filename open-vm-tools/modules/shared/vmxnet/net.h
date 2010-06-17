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
#define INCLUDE_ALLOW_VMCORE

#include "includeCheck.h"
#include "vm_device_version.h"

#ifdef VMCORE
#include "config.h"
#include "str.h"
#include "strutil.h"
#endif

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

#ifdef VMCORE
typedef struct Net_AdapterCount {
   uint8 vlance;
   uint8 vmxnet2;
   uint8 vmxnet3;
   uint8 e1000;
   uint8 e1000e;
} Net_AdapterCount;
#endif

#ifdef USERLEVEL

/*
 *----------------------------------------------------------------------------
 *
 * Net_AddAddrToLADRF --
 *
 *      Given a MAC address, sets the corresponding bit in the LANCE style
 *      Logical Address Filter 'ladrf'.
 *      The caller should have initialized the ladrf to all 0's, as this
 *      function only ORs on a bit in the array.
 *      'addr' is presumed to be ETHER_ADDR_LEN in size;
 *      'ladrf' is presumed to point to a 64-bit vector.
 *
 *      Derived from a long history of derivations, originally inspired by
 *      sample code from the AMD "Network Products: Ethernet Controllers 1998
 *      Data Book, Book 2", pages 1-53..1-55.
 *
 * Returns:
 *      None.
 *
 * Side effects:
 *      Updates 'ladrf'.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
Net_AddAddrToLadrf(const uint8 *addr,  // IN: pointer to MAC address
                   uint8 *ladrf)       // IN/OUT: pointer to ladrf
{
#define CRC_POLYNOMIAL_BE 0x04c11db7UL	/* Ethernet CRC, big endian */

   uint16 hashcode;
   int32 crc = 0xffffffff;		/* init CRC for each address */
   int32 j;
   int32 bit;
   int32 byte;

   ASSERT(addr);
   ASSERT(ladrf);

   for (byte = 0; byte < ETHER_ADDR_LEN; byte++) {  /* for each address byte */
      /* process each address bit */
      for (bit = *addr++, j = 0;
           j < 8;
           j++, bit >>= 1) {
	 crc = (crc << 1) ^ ((((crc < 0 ? 1 : 0) ^ bit) & 0x01) ?
               CRC_POLYNOMIAL_BE : 0);
      }
   }
   hashcode = (crc & 1);	       /* hashcode is 6 LSb of CRC ... */
   for (j = 0; j < 5; j++) {	       /* ... in reverse order. */
      hashcode = (hashcode << 1) | ((crc>>=1) & 1);
   }

   ladrf[hashcode >> 3] |= 1 << (hashcode & 0x07);
}
#endif // USERLEVEL

#ifdef VMCORE
/*
 *----------------------------------------------------------------------
 *
 * Net_GetNumAdapters --
 *
 *      Returns the number of each type of network adapter configured in this 
 *      VM.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
Net_GetNumAdapters(Net_AdapterCount *counts)
{
   uint32 i;

   counts->vlance = 0;
   counts->vmxnet2 = 0;
   counts->vmxnet3 = 0;
   counts->e1000 = 0;
   counts->e1000e = 0;

   for (i = 0; i < MAX_ETHERNET_CARDS; i++) {
      char* adapterStr;

      if (!Config_GetBool(FALSE, "ethernet%d.present", i)) {
	 continue;
      }
      adapterStr = Config_GetString("vlance", "ethernet%d.virtualDev", i);
      if (Str_Strcasecmp(adapterStr, "vmxnet3") == 0) {
         counts->vmxnet3++;
      } else if (Str_Strcasecmp(adapterStr, "vlance") == 0) {
         counts->vlance++;
      } else if (Str_Strcasecmp(adapterStr, "vmxnet") == 0) {
         counts->vmxnet2++;
      } else if (Str_Strcasecmp(adapterStr, "e1000") == 0) {
         counts->e1000++;
      } else if (Str_Strcasecmp(adapterStr, "e1000e") == 0) {
         counts->e1000e++;
      } else {
         LOG_ONCE(("%s: unknown adapter: %s\n", __FUNCTION__, adapterStr));
      }
      free(adapterStr);      
   }
}

#endif // VMCORE

#endif // VMWARE_DEVICES_NET_H

/*********************************************************
 * Copyright (C) 2005-2014,2017-2019 VMware, Inc. All rights reserved.
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
 * eth.h  --
 *
 *    Virtual ethernet.
 */

#ifndef _ETH_PUBLIC_H_
#define _ETH_PUBLIC_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMK_MODULE

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_defs.h"

#if defined __cplusplus
extern "C" {
#endif


#define ETH_LADRF_LEN      2
#define ETH_ADDR_LENGTH    6

typedef uint8 Eth_Address[ETH_ADDR_LENGTH];

// printf helpers
#define ETH_ADDR_FMT_STR     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx"
#define ETH_ADDR_FMT_ARGS(a) ((uint8 *)a)[0], ((uint8 *)a)[1], ((uint8 *)a)[2], \
                             ((uint8 *)a)[3], ((uint8 *)a)[4], ((uint8 *)a)[5]
#define ETH_ADDR_PTR_FMT_ARGS(a) &((uint8 *)a)[0], &((uint8 *)a)[1], \
                                 &((uint8 *)a)[2], &((uint8 *)a)[3], \
                                 &((uint8 *)a)[4], &((uint8 *)a)[5]

#define ETH_MAX_EXACT_MULTICAST_ADDRS 32

typedef enum Eth_RxMode {
   ETH_FILTER_UNICAST   = 0x0001,   // pass unicast (directed) frames
   ETH_FILTER_MULTICAST = 0x0002,   // pass some multicast frames
   ETH_FILTER_ALLMULTI  = 0x0004,   // pass *all* multicast frames
   ETH_FILTER_BROADCAST = 0x0008,   // pass broadcast frames
   ETH_FILTER_PROMISC   = 0x0010,   // pass all frames (ie no filter)
   ETH_FILTER_USE_LADRF = 0x0020,   // use the LADRF for multicast filtering
   ETH_FILTER_SINK      = 0x10000   // pass not-matched unicast frames
} Eth_RxMode;

// filter flags printf helpers
#define ETH_FILTER_FLAG_FMT_STR     "%s%s%s%s%s%s%s"
#define ETH_FILTER_FLAG_FMT_ARGS(f) (f) & ETH_FILTER_UNICAST   ? "  UNICAST"   : "", \
                                    (f) & ETH_FILTER_MULTICAST ? "  MULTICAST" : "", \
                                    (f) & ETH_FILTER_ALLMULTI  ? "  ALLMULTI"  : "", \
                                    (f) & ETH_FILTER_BROADCAST ? "  BROADCAST" : "", \
                                    (f) & ETH_FILTER_PROMISC   ? "  PROMISC"   : "", \
                                    (f) & ETH_FILTER_USE_LADRF ? "  USE_LADRF" : "", \
                                    (f) & ETH_FILTER_SINK      ? "  SINK"      : ""

// Ethernet header type
typedef enum {
   ETH_HEADER_TYPE_DIX,
   ETH_HEADER_TYPE_802_1PQ,
   ETH_HEADER_TYPE_802_3,
   ETH_HEADER_TYPE_802_1PQ_802_3,
   ETH_HEADER_TYPE_NESTED_802_1PQ,
} Eth_HdrType;

// DIX type fields we care about
typedef enum {
   ETH_TYPE_IPV4        = 0x0800,
   ETH_TYPE_IPV6        = 0x86DD,
   ETH_TYPE_ARP         = 0x0806,
   ETH_TYPE_RARP        = 0x8035,
   ETH_TYPE_LLDP        = 0x88CC,
   ETH_TYPE_CDP         = 0x2000,
   ETH_TYPE_AKIMBI      = 0x88DE,
   ETH_TYPE_VMWARE      = 0x8922,
   ETH_TYPE_1588        = 0x88F7,
   ETH_TYPE_NSH         = 0x894F,
   ETH_TYPE_802_1PQ     = 0x8100,  // not really a DIX type, but used as such
   ETH_TYPE_QINQ        = 0x88A8,
   ETH_TYPE_LLC         = 0xFFFF,  // 0xFFFF is IANA reserved, used to mark LLC
} Eth_DixType;
typedef enum {
   ETH_TYPE_IPV4_NBO    = 0x0008,
   ETH_TYPE_IPV6_NBO    = 0xDD86,
   ETH_TYPE_ARP_NBO     = 0x0608,
   ETH_TYPE_RARP_NBO    = 0x3580,
   ETH_TYPE_LLDP_NBO    = 0xCC88,
   ETH_TYPE_CDP_NBO     = 0x0020,
   ETH_TYPE_AKIMBI_NBO  = 0xDE88,
   ETH_TYPE_VMWARE_NBO  = 0x2289,
   ETH_TYPE_1588_NBO    = 0xF788,
   ETH_TYPE_NSH_NBO     = 0x4F89,
   ETH_TYPE_802_1PQ_NBO = 0x0081,  // not really a DIX type, but used as such
   ETH_TYPE_QINQ_NBO    = 0xA888,
   ETH_TYPE_802_3_PAUSE_NBO = 0x0888,  // pause frame based ethernet flow control
} Eth_DixTypeNBO;

// low two bits of the LLC control byte
typedef enum {
   ETH_LLC_CONTROL_IFRAME  = 0x0, // both 0x0 and 0x2, only low bit of 0 needed
   ETH_LLC_CONTROL_SFRAME  = 0x1,
   ETH_LLC_CONTROL_UFRAME  = 0x3,
} Eth_LLCControlBits;

#define ETH_LLC_CONTROL_UFRAME_MASK (0x3)

typedef
#include "vmware_pack_begin.h"
struct Eth_DIX {
   uint16  typeNBO;     // indicates the higher level protocol
}
#include "vmware_pack_end.h"
Eth_DIX;

/*
 * LLC header come in two varieties:  8 bit control and 16 bit control.
 * when the lower two bits of the first byte's control are '11', this
 * indicated the 8 bit control field.
 */

typedef 
#include "vmware_pack_begin.h"
struct Eth_LLC8 {
   uint8   dsap;
   uint8   ssap;
   uint8   control;
}
#include "vmware_pack_end.h"
Eth_LLC8;

typedef
#include "vmware_pack_begin.h"
struct Eth_LLC16 {
   uint8   dsap;
   uint8   ssap;
   uint16  control;
}
#include "vmware_pack_end.h"
Eth_LLC16;

typedef
#include "vmware_pack_begin.h"
struct Eth_SNAP {
   uint8   snapOrg[3];
   Eth_DIX snapType;
} 
#include "vmware_pack_end.h"
Eth_SNAP;

typedef
#include "vmware_pack_begin.h"
struct Eth_802_3 {  
   uint16   lenNBO;      // length of the frame
   Eth_LLC8 llc;         // LLC header
   Eth_SNAP snap;        // SNAP header
} 
#include "vmware_pack_end.h"
Eth_802_3;

// 802.1p QOS/priority tags
// 
enum {
   ETH_802_1_P_BEST_EFFORT          = 0,
   ETH_802_1_P_BACKGROUND           = 1,
   ETH_802_1_P_EXCELLENT_EFFORT     = 2,
   ETH_802_1_P_CRITICAL_APPS        = 3,
   ETH_802_1_P_VIDEO                = 4,
   ETH_802_1_P_VOICE                = 5,
   ETH_802_1_P_INTERNETWORK_CONROL  = 6,
   ETH_802_1_P_NETWORK_CONTROL      = 7
};

typedef
#include "vmware_pack_begin.h"
struct Eth_802_1pq_Tag {
   uint16 typeNBO;            // always ETH_TYPE_802_1PQ
   uint16 vidHi:4,            // 802.1q vlan ID high nibble
          canonical:1,        // bit order? (should always be 0)
          priority:3,         // 802.1p priority tag
          vidLo:8;            // 802.1q vlan ID low byte
}
#include "vmware_pack_end.h"
Eth_802_1pq_Tag;

typedef
#include "vmware_pack_begin.h"
struct Eth_802_1pq {
   Eth_802_1pq_Tag tag;       // VLAN/QOS tag
   union {
      Eth_DIX      dix;       // DIX header follows
      Eth_802_3    e802_3;    // or 802.3 header follows 
      struct {
         Eth_802_1pq_Tag   tag;        // inner VLAN/QOS tag
         Eth_DIX           dix;        // DIX header follows
      } nested802_1pq;
   }; 
}
#include "vmware_pack_end.h"
Eth_802_1pq; 

typedef
#include "vmware_pack_begin.h"
struct Eth_Header {
   Eth_Address     dst;       // all types of ethernet frame have dst first
   Eth_Address     src;       // and the src next (at least all the ones we'll see)
   union {
      Eth_DIX      dix;       // followed by a DIX header...
      Eth_802_3    e802_3;    // ...or an 802.3 header
      Eth_802_1pq  e802_1pq;  // ...or an 802.1[pq] tag and a header
   };
}
#include "vmware_pack_end.h"
Eth_Header;

/*
 * Official VMware ethertype header format and types
 */
#define ETH_VMWARE_FRAME_MAGIC   0x026f7564

enum {
   ETH_VMWARE_FRAME_TYPE_INVALID    = 0,
   ETH_VMWARE_FRAME_TYPE_BEACON     = 1,
   ETH_VMWARE_FRAME_TYPE_COLOR      = 2,
   ETH_VMWARE_FRAME_TYPE_ECHO       = 3,
   ETH_VMWARE_FRAME_TYPE_LLC        = 4, // XXX: Just re-use COLOR?
};

typedef
#include "vmware_pack_begin.h"
struct Eth_VMWareFrameHeader {
   uint32         magic;
   uint16         lenNBO;
   uint8          type;
}
#include "vmware_pack_end.h"
Eth_VMWareFrameHeader;

typedef Eth_Header Eth_802_1pq_Header; // for sizeof

#define ETH_BROADCAST_ADDRESS { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }

extern Eth_Address netEthBroadcastAddr;

/*
 * simple predicate for 1536 boundary.
 * the parameter is a network ordered uint16, which is compared to 0x06,
 * testing for "length" values greater than or equal to 0x0600 (1536)
 */

#define ETH_TYPENOT8023(x)      (((x) & 0xff) >= 0x06)

/*
 * header length macros
 *
 * first two are typical: ETH_HEADER_LEN_DIX, ETH_HEADER_LEN_802_1PQ
 * last two are suspicious, due to 802.3 incompleteness
 */

#define ETH_HEADER_LEN_DIX           (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_DIX))
#define ETH_HEADER_LEN_802_1PQ       (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_802_1pq_Tag) + \
                                      sizeof(Eth_DIX))
#define ETH_HEADER_LEN_802_2_LLC     (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(uint16) + \
                                      sizeof(Eth_LLC8))
#define ETH_HEADER_LEN_802_2_LLC16   (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(uint16) + \
                                      sizeof(Eth_LLC16))
#define ETH_HEADER_LEN_802_3         (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_802_3))
#define ETH_HEADER_LEN_802_1PQ_LLC   (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_802_1pq_Tag) + \
                                      sizeof(uint16) + \
                                      sizeof(Eth_LLC8))
#define ETH_HEADER_LEN_802_1PQ_LLC16 (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_802_1pq_Tag) + \
                                      sizeof(uint16) + \
                                      sizeof(Eth_LLC16))
#define ETH_HEADER_LEN_802_1PQ_802_3 (sizeof(Eth_Address) + \
                                      sizeof(Eth_Address) + \
                                      sizeof(Eth_802_1pq_Tag) + \
                                      sizeof(Eth_802_3))
#define ETH_HEADER_LEN_NESTED_802_1PQ  (sizeof(Eth_Address) + \
                                        sizeof(Eth_Address) + \
                                        sizeof(Eth_802_1pq_Tag) + \
                                        sizeof(Eth_802_1pq_Tag) + \
                                        sizeof(Eth_DIX))

#define ETH_MIN_HEADER_LEN   (ETH_HEADER_LEN_DIX)
#define ETH_MAX_HEADER_LEN   (ETH_HEADER_LEN_802_1PQ_802_3)

#define ETH_MIN_FRAME_LEN                    60
#define ETH_MAX_STD_MTU                      1500
#define ETH_MAX_STD_FRAMELEN                 (ETH_MAX_STD_MTU + ETH_MAX_HEADER_LEN)
/*
 * ENS_MBUF_SLAB_9K_ALLOC_SIZE and PKT_SLAB_JUMBO_SIZE both use 9216 for L2
 * MTU. And ETH_MAX_JUMBO_MTU is L3 MTU. It is required to have
 * (ETH_MAX_JUMBO_MTU + ETH_MAX_HEADER_LEN) <= 9216 and ETH_MAX_HEADER_LEN is
 * 26. So the maximal possible ETH_MAX_JUMBO_MTU = 9216 - 26 = 9190.
 */
#define ETH_MAX_JUMBO_MTU                    9190
#define ETH_MAX_JUMBO_FRAMELEN               (ETH_MAX_JUMBO_MTU + ETH_MAX_HEADER_LEN)

#define ETH_DEFAULT_MTU                      1500

#define ETH_FCS_LEN                          4
#define ETH_VLAN_LEN                         sizeof(Eth_802_1pq_Tag)

/*
 *----------------------------------------------------------------------
 *
 * Eth_IsAddrMatch --
 *
 *      Do the two ethernet addresses match?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsAddrMatch(const Eth_Address addr1, const Eth_Address addr2)
{
#ifdef __GNUC__
   /* string.h may not exist for kernel modules, so cannot use memcmp() */
   return !__builtin_memcmp(addr1, addr2, ETH_ADDR_LENGTH);
#else
   return !memcmp(addr1, addr2, ETH_ADDR_LENGTH);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsBroadcastAddr --
 *
 *      Is the address the broadcast address?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsBroadcastAddr(const Eth_Address addr) 
{
   return Eth_IsAddrMatch(addr, netEthBroadcastAddr);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsUnicastAddr --
 *
 *      Is the address a unicast address?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsUnicastAddr(const Eth_Address addr) 
{
   // broadcast and multicast frames always have the low bit set in byte 0 
   return !(((char *)addr)[0] & 0x1);
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_IsNullAddr --
 *
 *      Is the address the all-zeros address?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsNullAddr(const Eth_Address addr) 
{
   return ((addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]) == 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_HeaderType --
 *
 *      return an Eth_HdrType depending on the eth header
 *      contents.  will not work in all cases, especially since it
 *      requres ETH_HEADER_LEN_802_1PQ bytes to determine the type
 *
 *      HeaderType isn't sufficient to determine the length of
 *      the eth header.  for 802.3 header, its not clear without
 *      examination, whether a SNAP is included
 *
 *      returned type:
 *
 *      ETH_HEADER_TYPE_DIX: typical 14 byte eth header
 *      ETH_HEADER_TYPE_802_1PQ: DIX+vlan tagging
 *      ETH_HEADER_TYPE_NESTED_802_1PQ: DIX+vlan tagging+DIX+vlan tagging
 *      ETH_HEADER_TYPE_802_3: 802.3 eth header
 *      ETH_HEADER_TYPE_802_1PQ_802_3: 802.3 + vlan tag 
 *
 *      the test for DIX was moved from a 1500 boundary to a 1536
 *      boundary, since the vmxnet2 MTU was updated to 1514.  when
 *      W2K8 attempted to send LLC frames, these were interpreted
 *      as DIX frames instead of the correct 802.3 type
 *
 *      these links may help if they're valid:
 *
 *      http://standards.ieee.org/regauth/ethertype/type-tut.html
 *      http://standards.ieee.org/regauth/ethertype/type-pub.html
 *      
 *
 * Results: 
 *	Eth_HdrType value
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Eth_HdrType
Eth_HeaderType(const Eth_Header *eh)
{
   /*
    * we use 1536 (IEEE 802.3-std mentions 1536, but iana indicates
    * type of 0-0x5dc are 802.3) instead of some #def symbol to prevent
    * inadvertant reuse of the same macro for buffer size decls.
    */

   if (ETH_TYPENOT8023(eh->dix.typeNBO)) {
      if (eh->dix.typeNBO != ETH_TYPE_802_1PQ_NBO) {

         /*
          * typical case
          */

         return ETH_HEADER_TYPE_DIX;
      } 

      /*
       * some type of 802.1pq tagged frame
       */

      if (ETH_TYPENOT8023(eh->e802_1pq.dix.typeNBO)) {

         /*
          * vlan tagging with dix style type
          */

         if (UNLIKELY(eh->e802_1pq.dix.typeNBO == ETH_TYPE_802_1PQ_NBO)) {
            return ETH_HEADER_TYPE_NESTED_802_1PQ;
         }

         return ETH_HEADER_TYPE_802_1PQ;
      }

      /*
       * vlan tagging with 802.3 header
       */

      return ETH_HEADER_TYPE_802_1PQ_802_3;
   }
   
   /*
    * assume 802.3
    */

   return ETH_HEADER_TYPE_802_3;
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_EncapsulatedPktType --
 *
 *      Get the encapsulated (layer 3) frame type. 
 *      for LLC frames without SNAP, we don't have
 *      an encapsulated type, and return ETH_TYPE_LLC.
 *
 *      IANA reserves 0xFFFF, which we reuse to indicate
 *      ETH_TYPE_LLC.  
 *      
 *
 * Results: 
 *	NBO frame type.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Eth_EncapsulatedPktType(const Eth_Header *eh)
{
   Eth_HdrType type = Eth_HeaderType(eh);  

   switch (type) {
   case ETH_HEADER_TYPE_DIX :
      return eh->dix.typeNBO;

   case ETH_HEADER_TYPE_802_1PQ :
      return eh->e802_1pq.dix.typeNBO;

   case ETH_HEADER_TYPE_NESTED_802_1PQ:
      return eh->e802_1pq.nested802_1pq.dix.typeNBO;

   case ETH_HEADER_TYPE_802_3 :
      /*
       * Documentation describes SNAP headers as having ONLY
       * 0x03 as the control fields, not just the lower two bits
       * This prevents the use of Eth_IsLLCControlUFormat.
       */
      if ((eh->e802_3.llc.dsap == 0xaa) &&
           (eh->e802_3.llc.ssap == 0xaa) &&
           (eh->e802_3.llc.control == ETH_LLC_CONTROL_UFRAME)) {
               return eh->e802_3.snap.snapType.typeNBO;
      } else {
         // LLC, no snap header, then no type
         return ETH_TYPE_LLC;
      }

   case ETH_HEADER_TYPE_802_1PQ_802_3 :
      if ((eh->e802_1pq.e802_3.llc.dsap == 0xaa) &&
           (eh->e802_1pq.e802_3.llc.ssap == 0xaa) &&
           (eh->e802_1pq.e802_3.llc.control == ETH_LLC_CONTROL_UFRAME)) {
               return eh->e802_1pq.e802_3.snap.snapType.typeNBO;
      } else {
         // tagged LLC, no snap header, then no type
         return ETH_TYPE_LLC;
      }
   }

   ASSERT(FALSE);
   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_IsDixType --
 *
 *      Is the frame of the requested protocol type or is it an 802.1[pq]
 *      encapsulation of such a frame?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsDixType(const Eth_Header *eh, const Eth_DixTypeNBO type) 
{
   return Eth_EncapsulatedPktType(eh) == type;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsBeaconSap --
 *
 *      test to validate a frame to determine if its a beacon
 *      (ncp) frame.  sap is passed in.
 *
 *      ncp beacon/color frames are LLC frames with a DSAP/SSAP
 *      set based on a config value.  non-zero llc length is 
 *      tested here to prevent the predicate from interfering
 *      with testworld etherswitch tests
 *
 *
 * Results:
 *      TRUE or FALSE.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsBeaconSap(const Eth_Header *eh, const uint8 sap)
{
   Eth_HdrType type = Eth_HeaderType(eh);

   if (type == ETH_HEADER_TYPE_802_3) {
      if ((eh->e802_3.llc.dsap == sap) && (eh->e802_3.llc.ssap == sap)) {
         if (eh->e802_3.lenNBO != 0) {
            return TRUE;
         }
      }
   } else if (type == ETH_HEADER_TYPE_802_1PQ_802_3) {
      if ((eh->e802_1pq.e802_3.llc.dsap == sap) &&
          (eh->e802_1pq.e802_3.llc.ssap == sap)) {
            if (eh->e802_1pq.e802_3.lenNBO != 0) {
               return TRUE;
            }
      }
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsIPV4 --
 *
 *      Is the frame an IPV4 frame?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsIPV4(const Eth_Header *eh) 
{
   return Eth_IsDixType(eh, ETH_TYPE_IPV4_NBO);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsIPV6 --
 *
 *      Is the frame an IPV6 frame?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsIPV6(const Eth_Header *eh) 
{
   return Eth_IsDixType(eh, ETH_TYPE_IPV6_NBO);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsVMWare --
 *
 *      Is the frame a VMWare frame?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsVMWare(const Eth_Header *eh) 
{
   return Eth_IsDixType(eh, ETH_TYPE_VMWARE_NBO);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsARP --
 *
 *      Is the frame an ARP frame?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsARP(const Eth_Header *eh) 
{
   return Eth_IsDixType(eh, ETH_TYPE_ARP_NBO);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsFrameTagged --
 *
 *      Does the frame contain an 802.1[pq] tag?
 *
 * Results: 
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsFrameTagged(const Eth_Header *eh) 
{
   return (eh->dix.typeNBO == ETH_TYPE_802_1PQ_NBO);
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_IsPauseFrame --
 *
 *      Is the frame 802.3 pause frame ?
 *
 * Results:
 *	TRUE or FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsPauseFrame(const Eth_Header *eh)
{
   return (eh->dix.typeNBO == ETH_TYPE_802_3_PAUSE_NBO);
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_FillVlanTag --
 *
 *      Populate the fields of a vlan tag
 *
 * Results: 
 *	The populated vlan tag
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Eth_802_1pq_Tag *
Eth_FillVlanTag(Eth_802_1pq_Tag *tag,
                const uint32 vlanId,
                const uint32 priority)
{

   ASSERT(vlanId < 4096);
   ASSERT(priority < 8);

   tag->typeNBO = ETH_TYPE_802_1PQ_NBO;
   tag->priority = (uint16)priority;
   tag->canonical = 0;                  // bit order (should be 0)
   tag->vidHi = vlanId >> 8;
   tag->vidLo = vlanId & 0xff;


   return tag;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_VlanTagGetVlanID --
 *
 *      Extract the VLAN ID from the vlanTag.
 *
 * Results: 
 *	The VLAN ID.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Eth_VlanTagGetVlanID(const Eth_802_1pq_Tag *tag)
{
   return (tag->vidHi << 8) | tag->vidLo;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_FrameGetVlanID --
 *
 *      Extract the VLAN ID from the frame's 802.1[pq] tag.
 *
 * Results: 
 *	The VLAN ID.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Eth_FrameGetVlanID(const Eth_Header *eh) 
{
   ASSERT(Eth_IsFrameTagged(eh));

   return Eth_VlanTagGetVlanID(&eh->e802_1pq.tag);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_FrameSetVlanID --
 *
 *      Set the VLAN ID in the frame's 802.1[pq] tag.
 *
 * Results: 
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
Eth_FrameSetVlanID(Eth_Header *eh, uint16 vid) 
{
   ASSERT(Eth_IsFrameTagged(eh));

   eh->e802_1pq.tag.vidHi = vid >> 8;
   eh->e802_1pq.tag.vidLo = vid & 0xff;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_FrameGetPriority --
 *
 *      Extract the priority from the frame's 802.1[pq] tag.
 *
 * Results: 
 *	The priority.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint8
Eth_FrameGetPriority(const Eth_Header *eh) 
{
   ASSERT(Eth_IsFrameTagged(eh));

   return (uint8)eh->e802_1pq.tag.priority;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_FrameSetPriority --
 *
 *      Set the priority in the frame's 802.1[pq] tag.
 *
 * Results: 
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
Eth_FrameSetPriority(Eth_Header *eh, const uint8 prio) 
{
   ASSERT(Eth_IsFrameTagged(eh));
   ASSERT(prio <= 7);

   eh->e802_1pq.tag.priority = prio;
}


/*
 *----------------------------------------------------------------------
 * Eth_IsLLCControlUFormat --
 *
 *      The LLC Control fields determines the lengeth of the LLC
 *      field, selecting 8 bit of 16 bit.  Thies predicate indicates
 *      whether the LLC frame is a U-Format frames.  The U-Format
 *      frame is the only LLC frame header which is 8 bits long.  
 *
 * Results
 *      Bool
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsLLCControlUFormat(const uint8 control)
{
   return (control & ETH_LLC_CONTROL_UFRAME_MASK) == ETH_LLC_CONTROL_UFRAME;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_HeaderLength_802_3 --
 *
 *      Returns the length of an 802_3 eth header without
 *      any vlan tagging.  factored out for Eth_HeaderComplete()
 *
 * Results:
 *      uint16 length
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint16
Eth_HeaderLength_802_3(const Eth_Header *eh)
{
   /*
    * Documentation describes SNAP headers as having ONLY
    * 0x03 as the control fields, not just the lower two bits
    * This prevents the use of Eth_IsLLCControlUFormat.
    */
   if ((eh->e802_3.llc.dsap == 0xaa) &&
        (eh->e802_3.llc.ssap == 0xaa) &&
        (eh->e802_3.llc.control == ETH_LLC_CONTROL_UFRAME)) {
            return ETH_HEADER_LEN_802_3;
   }
   // LLC, no snap header
   if (Eth_IsLLCControlUFormat(eh->e802_3.llc.control)) {
      return ETH_HEADER_LEN_802_2_LLC;
   }
   // Eth_LLC with a two byte control field
   return ETH_HEADER_LEN_802_2_LLC16;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_HeaderLength_802_1PQ_802_3 --
 *
 *      Returns the length of an 802_3 eth header with 
 *      vlan tagging.  factored out for Eth_HeaderComplete()
 *
 * Results:
 *      uint16 length
 *
 * Side Effects:
 *      None.
 *
 *
 *----------------------------------------------------------------------
 */
static INLINE uint16
Eth_HeaderLength_802_1PQ_802_3(const Eth_Header *eh)
{
   if ((eh->e802_1pq.e802_3.llc.dsap == 0xaa) &&
        (eh->e802_1pq.e802_3.llc.ssap == 0xaa) &&
        (eh->e802_1pq.e802_3.llc.control == ETH_LLC_CONTROL_UFRAME)) {
            return ETH_HEADER_LEN_802_1PQ_802_3;
   }
   // tagged LLC, no snap header
   if (Eth_IsLLCControlUFormat(eh->e802_1pq.e802_3.llc.control)) {
      return ETH_HEADER_LEN_802_1PQ_LLC;
   }
   // Eth_LLC with a two byte control field
   return ETH_HEADER_LEN_802_1PQ_LLC16;
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_HeaderLength --
 *
 *      Return the length of the header, taking into account
 *      different header variations.  for LLC headers,  determine
 *      whether the eth header has an associated SNAP header,
 *
 *      all references to the eth header assume the complete
 *      frame is mapped withing the frame mapped length
 *
 *      To determine the length, 17 bytes must be readable.
 *      LLC requires three bytes (after the 802.3 length) to 
 *      identify SNAP frames.
 *
 *      when the header isn't complete:
 *      Eth_HeaderType needs ETH_HEADER_LEN_DIX + sizeof(Eth_802_1pq_Tag)
 *      to completely distinguish types (18 bytes), 
 *      Eth_HeaderType correctly identifies basic untagged DIX frames
 *      with ETH_HEADER_LEN_DIX (14 bytes) bytes.
 *      Eth_HeaderLength will correctly return length of untagged LLC frames 
 *      with ETH_HEADER_LEN_DIX + sizeof(Eth_LLC) (17 bytes), but if the
 *      frame is tagged, it will need
 *      ETH_HEADER_LEN_DIX + sizeof(Eth_802_1pq_Tag) + sizeof(Eth_LLC)
 *      (21 bytes)
 *
 *
 * Results:
 *      uint16 size of the header
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Eth_HeaderLength(const Eth_Header *eh)
{
   Eth_HdrType type = Eth_HeaderType(eh);  

   switch (type) {
   case ETH_HEADER_TYPE_DIX :
      return ETH_HEADER_LEN_DIX;

   case ETH_HEADER_TYPE_802_1PQ :
      return ETH_HEADER_LEN_802_1PQ;

   case ETH_HEADER_TYPE_NESTED_802_1PQ :
      return ETH_HEADER_LEN_NESTED_802_1PQ;

   case ETH_HEADER_TYPE_802_3 :
      return Eth_HeaderLength_802_3(eh);

   case ETH_HEADER_TYPE_802_1PQ_802_3 :
      return Eth_HeaderLength_802_1PQ_802_3(eh);
   }
   
   ASSERT(FALSE);
   return 0;   
}

/*
 *----------------------------------------------------------------------
 *
 * Eth_GetPayloadWithLen --
 *
 *      Simple cover to comput the payload's address given the length
 *
 * Results:
 *      pointer to the frame's payload
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void *
Eth_GetPayloadWithLen(const void *frame, const uint16 ehHdrLen)
{
   return (void *)((uint8 *)frame + ehHdrLen);
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_GetPayload --
 *
 *      Return the address of the frame's payload, taking into account
 *      different header variations.  Code assumes a complete ether
 *      header is mapped at the frame, assumption exists due to
 *      the use of Eth_HeaderLength, which isn't provided with
 *      the frame's length (how much data it can examine to determine
 *      the ethernet header length)
 *
 * Results:
 *      pointer to the frame's payload
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void *
Eth_GetPayload(const void *frame)
{
   return Eth_GetPayloadWithLen(frame, Eth_HeaderLength((Eth_Header *)frame));
}


/*
 *----------------------------------------------------------------------
 *
 * Eth_IsFrameHeaderComplete -- 
 *
 *      Predicate to determine whether the frame has enough length
 *      to contain a complete eth header of the correct type.
 *      If you already know/expect the length to exceed
 *      ETH_MAX_HEADER_LEN then for performance reasons you should
 *      explicitly check for that before calling this function.
 *
 * Results:
 *      returns true when a complete eth header is available
 *
 * Side Effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Eth_IsFrameHeaderComplete(const Eth_Header *eh,
                          const uint32 len,
                          uint16 *ehHdrLen)
{
   uint16 ehLen;

   if (UNLIKELY(eh == NULL)) {
      return FALSE;
   }
   /*
    * Hard to know what's the optimal order for these checks
    * perform the most likely case first.
    * Since its not directly obvious, hex 0x06 -> 1536, 
    * (see ETH_TYPENOT8023() for 1536 details)
    */
   if (LIKELY((len >= ETH_HEADER_LEN_DIX) &&
              ETH_TYPENOT8023(eh->dix.typeNBO) &&
              (eh->dix.typeNBO != ETH_TYPE_802_1PQ_NBO))) {
   
      if (ehHdrLen != NULL) {
         *ehHdrLen = ETH_HEADER_LEN_DIX;
      }
      return TRUE;
   }
   if (len >= ETH_HEADER_LEN_802_1PQ) {
      /*
       * Eth_HeaderType will correctly enumerate all types once
       * at least ETH_HEADER_LEN_802_1PQ bytes are available except nested
       * 802.1pq tag.
       */
      Eth_HdrType type = Eth_HeaderType(eh);

      switch (type) {
      case ETH_HEADER_TYPE_802_1PQ:
         if (ehHdrLen != NULL) {
            *ehHdrLen = ETH_HEADER_LEN_802_1PQ;
         }
         return TRUE;

      case ETH_HEADER_TYPE_NESTED_802_1PQ:
         if (ehHdrLen != NULL) {
            *ehHdrLen = ETH_HEADER_LEN_NESTED_802_1PQ;
         }
         return len >= ETH_HEADER_LEN_NESTED_802_1PQ;

      case ETH_HEADER_TYPE_802_3:
         /*
          * Length could be shorter LLC or LLC+SNAP.
          * ETH_HEADER_LEN_802_2_LLC bytes are needed to disambiguate.
          * note: ASSERT_ON_COMPILE fails windows builds.
          */
         ASSERT(ETH_HEADER_LEN_802_1PQ > ETH_HEADER_LEN_802_2_LLC);
         ehLen = Eth_HeaderLength_802_3(eh);
         /* continue to common test */
         break;

      case ETH_HEADER_TYPE_802_1PQ_802_3:
         if (len < ETH_HEADER_LEN_802_1PQ_LLC) {
            return FALSE;
         }
         ehLen = Eth_HeaderLength_802_1PQ_802_3(eh);
         /* continue to common test */
         break;

      default:
         /*
          * This else clause is unreachable, but if removed
          * compiler complains about ehLen possibly being
          * uninitialized.  this else is marginally preferred
          * over an unnecessary initialization
          */
         ASSERT(type != ETH_HEADER_TYPE_DIX);
         // NOT_REACHED()
         return FALSE;
      }

      /* common test */
      if (len >= ehLen) {
         if (ehHdrLen != NULL) {
            *ehHdrLen = ehLen;
         }
         return TRUE;
      }
      return FALSE;
   }

   /*
    * Corner case...  not enough len to use Eth_HeaderType,
    * since len is shorter than ETH_HEADER_LEN_802_1PQ bytes. 
    * but with ETH_HEADER_LEN_802_2_LLC bytes, and an 802.3
    * frame, a U Format LLC frame indicates TRUE 
    * with a header length of ETH_HEADER_LEN_802_2_LLC bytes.
    *
    * The additional test for ETH_TYPENOT8023() is necessary
    * for the case where the dix frame failed due to the
    * vlan tagging test early in this procedure.
    */
   if ((len == ETH_HEADER_LEN_802_2_LLC) &&
       !ETH_TYPENOT8023(eh->dix.typeNBO) &&
       Eth_IsLLCControlUFormat(eh->e802_3.llc.control)) {

      if (ehHdrLen != NULL) {
         *ehHdrLen = ETH_HEADER_LEN_802_2_LLC;
      }
      return TRUE;
   }
   return FALSE;
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // _ETH_PUBLIC_H_

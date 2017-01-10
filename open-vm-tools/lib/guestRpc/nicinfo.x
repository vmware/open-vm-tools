/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * nicinfo.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    provide information about the guest NICs.
 */

/*
 * Enumerates the different versions of the messages. Starting at 2, since
 * version one is legacy code we can't change.
 */
enum NicInfoVersion {
   NIC_INFO_V1 = 1,     /* XXX Not represented here. */
   NIC_INFO_V2 = 2,
   NIC_INFO_V3 = 3
};

/*
 * These are arbitrary limits to avoid possible DoS attacks.
 * The IP limit is large enough to hold an IP address (either v4 or v6).
 */
const NICINFO_MAX_IP_LEN   = 64;
const NICINFO_MAX_IPS      = 2048;
const NICINFO_MAX_NICS     = 16;

/* MAC Addresses are "AA:BB:CC:DD:EE:FF" = 18 bytes. */
const NICINFO_MAC_LEN      = 18;

/*
 * Corresponds to public/guestInfo.h::GuestInfoIPAddressFamilyType.
 */
enum NicInfoAddrType {
   NICINFO_ADDR_IPV4 = 0,
   NICINFO_ADDR_IPV6 = 1
};

struct VmIpAddress {
   NicInfoAddrType addressFamily;
   Bool        dhcpEnabled;
   char        ipAddress[NICINFO_MAX_IP_LEN];
   /*
    * For IPv4, may be either a hexadecimal mask ("0xffffff00") or CIDR-style
    * prefix length ("24").  For IPv6, will only be a prefix length.
    */
   char        subnetMask[NICINFO_MAX_IP_LEN];
};

struct GuestNic {
   char                 macAddress[NICINFO_MAC_LEN];
   struct VmIpAddress   ips<NICINFO_MAX_IPS>;
};

/*
 * This structure is not entirely necessary, but it makes the generated
 * code nicer to code to.
 */
struct GuestNicList {
   struct GuestNic nics<NICINFO_MAX_NICS>;
};


/*
 *-----------------------------------------------------------------------------
 *
 * NIC Info version 3.
 *
 *      V3 adds routing, DNS, and WINS information to the NIC list.
 *
 *-----------------------------------------------------------------------------
 */


/*
 * IP v4 and v6 addressing.
 *
 * These types were redefined primarily to allow much more efficient wire
 * encoding.
 */


/*
 * See RFC 4001, InetAddress.
 */
const INET_ADDRESS_MAX_LEN = 255;
typedef opaque InetAddress<INET_ADDRESS_MAX_LEN>;


/*
 * See RFC 4001, InetAddressType.
 */
enum InetAddressType {
   IAT_UNKNOWN  = 0,
   IAT_IPV4     = 1,
   IAT_IPV6     = 2,
   IAT_IPV4Z    = 3,
   IAT_IPV6Z    = 4,
   IAT_DNS      = 16
};


/*
 * See RFC 4001, InetAddressPrefixLength.
 */
typedef unsigned int InetAddressPrefixLength;


/*
 * See RFC 4293, IpAddressOriginTC.
 */
enum IpAddressOrigin {
   IAO_OTHER            = 1,
   IAO_MANUAL           = 2,
   IAO_DHCP             = 4,
   IAO_LINKLAYER        = 5,
   IAO_RANDOM           = 6
};


/*
 * See RFC 4293, IpAddressStatusTC.
 *
 * "The status of an address.  Most of the states correspond to states
 * from the IPv6 Stateless Address Autoconfiguration protocol.
 *    ...
 * "In the absence of other information, an IPv4 address is always
 * preferred(1)." 
 */
enum IpAddressStatus {
   IAS_PREFERRED        = 1,
   IAS_DEPRECATED       = 2,
   IAS_INVALID          = 3,
   IAS_INACCESSIBLE     = 4,
   IAS_UNKNOWN          = 5,
   IAS_TENTATIVE        = 6,
   IAS_DUPLICATE        = 7,
   IAS_OPTIMISTIC       = 8
};


/*
 * Convenient type for lists of addresses.
 */
struct TypedIpAddress {
   InetAddressType      ipAddressAddrType;
   InetAddress          ipAddressAddr;
};


/*
 * See RFC 4293, IpAddressEntry.
 *
 * "An address mapping for a particular interface."
 *
 * We deviate from the RFC in a few places:
 *  - The prefix length is listed explicitly here rather than reference
 *    a separate prefix table.
 *  - Interface indices aren't included as this structure is dependent
 *    upon/encapsulated by a GuestNicV3 structure.
 */
struct IpAddressEntry {
   TypedIpAddress               ipAddressAddr;
   InetAddressPrefixLength      ipAddressPrefixLength;
   IpAddressOrigin              *ipAddressOrigin;
   IpAddressStatus              *ipAddressStatus;
};


/*
 * Runtime DNS resolver state.
 */


/*
 * Quoth RFC 2181 §11 (Name Syntax):
 *    "The length of any one label is limited to between 1 and 63 octets.  A
 *    full domain name is limited to 255 octets (including the separators)."
 */
const DNSINFO_MAX_ADDRLEN = 255;
typedef string DnsHostname<DNSINFO_MAX_ADDRLEN>;


/*
 * Arbitrary limits.
 */
const DNSINFO_MAX_SUFFIXES = 10;
const DNSINFO_MAX_SERVERS = 16;

struct DnsConfigInfo {
   DnsHostname  *hostName;
   DnsHostname  *domainName;
   TypedIpAddress serverList<DNSINFO_MAX_SERVERS>;
   DnsHostname  searchSuffixes<DNSINFO_MAX_SUFFIXES>;
};


/*
 * Runtime WINS resolver state.  Addresses are assumed to be IPv4 only.
 */

struct WinsConfigInfo {
   TypedIpAddress       primary;
   TypedIpAddress       secondary;
};


/*
 * Runtime routing tables.
 */


/*
 * Arbitrary limit.
 */
const NICINFO_MAX_ROUTES = 100;


/*
 * See RFC 4292 for details.
 */
enum InetCidrRouteType {
   ICRT_OTHER   = 1,
   ICRT_REJECT  = 2,
   ICRT_LOCAL   = 3,
   ICRT_REMOTE  = 4
};

struct InetCidrRouteEntry {
   TypedIpAddress               inetCidrRouteDest;
   InetAddressPrefixLength      inetCidrRoutePfxLen;

   /*
    * Next hop isn't mandatory.
    */
   TypedIpAddress               *inetCidrRouteNextHop;

   /*
    * inetCidrRouteProto is omitted as we're concerned only with static/
    * netmgmt routes.
    */

   /* This is an array index into GuestNicListV3::nics. */
   uint32                       inetCidrRouteIfIndex;

   /* XXX Do we really need this? */
   InetCidrRouteType            inetCidrRouteType;

   /* -1 == unused. */
   uint32                       inetCidrRouteMetric;
};


/*
 * Fun with DHCP
 */

const DHCP_MAX_CONFIG_SIZE = 16384;

struct DhcpConfigInfo {
   bool         enabled;
   string       dhcpSettings<DHCP_MAX_CONFIG_SIZE>;
};


/*
 * Top-level containers.
 */


/*
 * Describes a single NIC.
 */

struct GuestNicV3 {
   string               macAddress<NICINFO_MAC_LEN>;
   IpAddressEntry       ips<NICINFO_MAX_IPS>;
   DnsConfigInfo        *dnsConfigInfo;
   WinsConfigInfo       *winsConfigInfo;
   DhcpConfigInfo       *dhcpConfigInfov4;
   DhcpConfigInfo       *dhcpConfigInfov6;
};


/*
 * Attempts to model general network state.
 */

struct NicInfoV3 {
   GuestNicV3           nics<NICINFO_MAX_NICS>;
   InetCidrRouteEntry   routes<NICINFO_MAX_ROUTES>;
   DnsConfigInfo        *dnsConfigInfo;
   WinsConfigInfo       *winsConfigInfo;
   DhcpConfigInfo       *dhcpConfigInfov4;
   DhcpConfigInfo       *dhcpConfigInfov6;
};


/*
 * This defines the protocol for a "nic info" message. The union allows
 * us to create new versions of the protocol later by creating new values
 * in the NicInfoVersion enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail
 * if an unknown version is provided on the wire.
 */
union GuestNicProto switch (NicInfoVersion ver) {
case NIC_INFO_V2:
   struct GuestNicList *nicsV2;
case NIC_INFO_V3:
   struct NicInfoV3 *nicInfoV3;
};

/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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

/**
 * @file compareNicInfo.c
 *
 * Comparison routines for NicInfo types.  Handy for caching, unit testing.
 *
 * @todo Each byte of a MAC address is assumed to be represented by two
 * characters.  So, as far as these routines are concerned, 0:1:2:3:4:5
 * != 00:01:02:03:04:05.  Is this a problem?
 */

#include <string.h>

#include "vmware.h"
#include "xdrutil.h"

#include "nicInfoInt.h"


/**
 * Common comparison prefix routine.
 */
#define RETURN_EARLY_CMP_PTRS(a, b)  do {                               \
   if (!(a) && !(b)) {                                                  \
      return TRUE;                                                      \
   } else if ((!(a) && (b)) || ((a) && !(b))) {                         \
      return FALSE;                                                     \
   }                                                                    \
} while (0)


/*
 ******************************************************************************
 * GuestInfo_IsEqual_DhcpConfigInfo --                                   */ /**
 *
 * Compares a pair of DhcpConfigInfos.
 *
 * @param[in] a DhcpConfigInfo number 1.  May be NULL.
 * @param[in] b DhcpConfigInfo number 2.  May be NULL.
 *
 * @retval TRUE  DhcpConfigInfos are equivalent.
 * @retval FALSE DhcpConfigInfos differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_DhcpConfigInfo(const DhcpConfigInfo *a,
                                 const DhcpConfigInfo *b)
{
   RETURN_EARLY_CMP_PTRS(a, b);

   return a->enabled == b->enabled &&
          strcmp(a->dhcpSettings, b->dhcpSettings) == 0;
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_DnsConfigInfo --                                    */ /**
 *
 * Compares a pair of DnsConfigInfos.
 *
 * @param[in] a DnsConfigInfo number 1.  May be NULL.
 * @param[in] b DnsConfigInfo number 2.  May be NULL.
 *
 * @retval TRUE  DnsConfigInfos are equivalent.
 * @retval FALSE DnsConfigInfos differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_DnsConfigInfo(const DnsConfigInfo *a,
                                const DnsConfigInfo *b)
{
   u_int ai;
   u_int bi;

   RETURN_EARLY_CMP_PTRS(a, b);

   if (!GuestInfo_IsEqual_DnsHostname(a->hostName, b->hostName) ||
       !GuestInfo_IsEqual_DnsHostname(a->domainName, b->domainName) ||
       a->serverList.serverList_len != b->serverList.serverList_len ||
       a->searchSuffixes.searchSuffixes_len != b->searchSuffixes.searchSuffixes_len) {
      return FALSE;
   }

   /*
    * Since the lists' lengths match, search in b for each item in a.  We'll
    * assume that we don't have any duplicates in a s.t. unique(a) is a proper
    * subset of b.
    *
    * Bail if we can't find an entry.
    */

   XDRUTIL_FOREACH(ai, a, serverList) {
      TypedIpAddress *aServer = XDRUTIL_GETITEM(a, serverList, ai);

      XDRUTIL_FOREACH(bi, b, serverList) {
         TypedIpAddress *bServer = XDRUTIL_GETITEM(b, serverList, bi);

         if (GuestInfo_IsEqual_TypedIpAddress(aServer, bServer)) {
            break;
         }
      }

      if (bi == b->serverList.serverList_len) {
         /* Exhausted b's list, didn't find aServer. */
         return FALSE;
      }
   }

   XDRUTIL_FOREACH(ai, a, searchSuffixes) {
      DnsHostname *aSuffix = XDRUTIL_GETITEM(a, searchSuffixes, ai);

      XDRUTIL_FOREACH(bi, b, searchSuffixes) {
         DnsHostname *bSuffix = XDRUTIL_GETITEM(b, searchSuffixes, bi);

         if (GuestInfo_IsEqual_DnsHostname(aSuffix, bSuffix)) {
            break;
         }
      }

      if (bi == b->searchSuffixes.searchSuffixes_len) {
         /* Exhausted b's list, didn't find aSuffix. */
         return FALSE;
      }
   }

   return TRUE;
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_DnsHostname --                                      */ /**
 *
 * Compares a pair of DnsHostnames.
 *
 * @param[in] a DnsHostname number 1.  May be NULL.
 * @param[in] b DnsHostname number 2.  May be NULL.
 *
 * @retval TRUE  DnsHostnames are equivalent.
 * @retval FALSE DnsHostnames differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_DnsHostname(const DnsHostname *a,
                              const DnsHostname *b)
{
   RETURN_EARLY_CMP_PTRS(a, b);
   return strcasecmp(*a, *b) == 0 ? TRUE : FALSE;
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_GuestNicV3 --                                       */ /**
 *
 * Compares two GuestNicV3s.
 *
 * @param[in] a GuestNicV3 number 1.  May be NULL.
 * @param[in] b GuestNicV3 number 2.  May be NULL.
 *
 * @retval TRUE  GuestNicV3s are equivalent.
 * @retval FALSE GuestNicV3s differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_GuestNicV3(const GuestNicV3 *a,
                             const GuestNicV3 *b)
{
   u_int ai;
   u_int bi;

   RETURN_EARLY_CMP_PTRS(a, b);

   /* Not optional fields. */
   ASSERT(a->macAddress);
   ASSERT(b->macAddress);

   if (strcasecmp(a->macAddress, b->macAddress) != 0) {
      return FALSE;
   }

   /*
    * Compare the IP lists.
    */

   if (a->ips.ips_len != b->ips.ips_len) {
      return FALSE;
   }

   XDRUTIL_FOREACH(ai, a, ips) {
      IpAddressEntry *aEntry = XDRUTIL_GETITEM(a, ips, ai);

      XDRUTIL_FOREACH(bi, b, ips) {
         IpAddressEntry *bEntry = XDRUTIL_GETITEM(b, ips, bi);

         if (GuestInfo_IsEqual_IpAddressEntry(aEntry, bEntry)) {
            break;
         }
      }

      if (bi == b->ips.ips_len) {
         /* Exhausted b's list, didn't find aEntry. */
         return FALSE;
      }
   }

   return
      GuestInfo_IsEqual_DnsConfigInfo(a->dnsConfigInfo, b->dnsConfigInfo) &&
      GuestInfo_IsEqual_WinsConfigInfo(a->winsConfigInfo, b->winsConfigInfo) &&
      GuestInfo_IsEqual_DhcpConfigInfo(a->dhcpConfigInfov4, b->dhcpConfigInfov4) &&
      GuestInfo_IsEqual_DhcpConfigInfo(a->dhcpConfigInfov6, b->dhcpConfigInfov6);
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_InetCidrRouteEntry --                               */ /**
 *
 * Compares two InetCidrRouteEntrys.
 *
 * @param[in] a     InetCidrRouteEntry number 1.  May be NULL.
 * @param[in] b     InetCidrRouteEntry number 2.  May be NULL.
 * @param[in] aInfo a's NicInfo container.  If a != NULL, may NOT be NULL.
 * @param[in] bInfo b's NicInfo container.  If b != NULL, may NOT be NULL.
 *
 * @retval TRUE  InetCidrRouteEntrys are equivalent.
 * @retval FALSE InetCidrRouteEntrys differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_InetCidrRouteEntry(const InetCidrRouteEntry *a,
                                     const InetCidrRouteEntry *b,
                                     const NicInfoV3 *aInfo,
                                     const NicInfoV3 *bInfo)
{
   RETURN_EARLY_CMP_PTRS(a, b);

   ASSERT(aInfo);
   ASSERT(bInfo);

   return
      GuestInfo_IsEqual_TypedIpAddress(&a->inetCidrRouteDest,
                                       &b->inetCidrRouteDest) &&
      a->inetCidrRoutePfxLen == b->inetCidrRoutePfxLen &&
      GuestInfo_IsEqual_TypedIpAddress(a->inetCidrRouteNextHop,
                                       b->inetCidrRouteNextHop) &&
      strcasecmp(aInfo->nics.nics_val[a->inetCidrRouteIfIndex].macAddress,
                 bInfo->nics.nics_val[b->inetCidrRouteIfIndex].macAddress) == 0 &&
      a->inetCidrRouteType == b->inetCidrRouteType &&
      a->inetCidrRouteMetric == b->inetCidrRouteMetric;
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_IpAddressEntry --                                   */ /**
 *
 * Compares two IpAddressEntrys.
 *
 * @param[in] a IpAddressEntry number 1.  May be NULL.
 * @param[in] b IpAddressEntry number 2.  May be NULL.
 *
 * @retval TRUE  IpAddressEntrys are equivalent.
 * @retval FALSE IpAddressEntrys differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_IpAddressEntry(const IpAddressEntry *a,
                                 const IpAddressEntry *b)
{
   RETURN_EARLY_CMP_PTRS(a, b);

   return
      GuestInfo_IsEqual_TypedIpAddress(&a->ipAddressAddr, &b->ipAddressAddr) &&
      a->ipAddressPrefixLength == b->ipAddressPrefixLength &&
      ((a->ipAddressOrigin == NULL && b->ipAddressOrigin == NULL) ||
       (a->ipAddressOrigin != NULL && b->ipAddressOrigin != NULL &&
        *a->ipAddressOrigin == *b->ipAddressOrigin)) &&
      ((a->ipAddressStatus == NULL && b->ipAddressStatus == NULL) ||
       (a->ipAddressStatus != NULL && b->ipAddressStatus != NULL &&
        *a->ipAddressStatus == *b->ipAddressStatus));
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_NicInfoV3 --                                        */ /**
 *
 * Compares two NicInfoV3s.
 *
 * @param[in] a NicInfoV3 number 1.  May be NULL.
 * @param[in] b NicInfoV3 number 2.  May be NULL.
 *
 * @retval TRUE  NicInfoV3s are equivalent.
 * @retval FALSE NicInfoV3s differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_NicInfoV3(const NicInfoV3 *a,
                            const NicInfoV3 *b)
{
   u_int ai;
   u_int bi;

   RETURN_EARLY_CMP_PTRS(a, b);

   /*
    * Compare the NIC lists.
    */

   if (a->nics.nics_len != b->nics.nics_len) {
      return FALSE;
   }

   XDRUTIL_FOREACH(ai, a, nics) {
      GuestNicV3 *eachNic = XDRUTIL_GETITEM(a, nics, ai);
      GuestNicV3 *cmpNic = GuestInfoUtilFindNicByMac(b, eachNic->macAddress);

      if (cmpNic == NULL ||
          !GuestInfo_IsEqual_GuestNicV3(eachNic, cmpNic)) {
         return FALSE;
      }
   }

   /*
    * Compare routes.
    */

   if (a->routes.routes_len != b->routes.routes_len) {
      return FALSE;
   }

   XDRUTIL_FOREACH(ai, a, routes) {
      InetCidrRouteEntry *aRoute = XDRUTIL_GETITEM(a, routes, ai);

      XDRUTIL_FOREACH(bi, b, routes) {
         InetCidrRouteEntry *bRoute = XDRUTIL_GETITEM(b, routes, bi);

         if (GuestInfo_IsEqual_InetCidrRouteEntry(aRoute, bRoute, a, b)) {
            break;
         }
      }

      if (bi == b->routes.routes_len) {
         /* Exhausted b's list, didn't find aRoute. */
         return FALSE;
      }
   }

   /*
    * Compare the stack settings:
    *    . DnsConfigInfo
    *    . WinsConfigInfo
    *    . DhcpConfigInfov4
    *    . DhcpConfigInfov6
    */

   return
      GuestInfo_IsEqual_DnsConfigInfo(a->dnsConfigInfo, b->dnsConfigInfo) &&
      GuestInfo_IsEqual_WinsConfigInfo(a->winsConfigInfo, b->winsConfigInfo) &&
      GuestInfo_IsEqual_DhcpConfigInfo(a->dhcpConfigInfov4, b->dhcpConfigInfov4) &&
      GuestInfo_IsEqual_DhcpConfigInfo(a->dhcpConfigInfov6, b->dhcpConfigInfov6);
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_TypedIpAddress --                                   */ /**
 *
 * Compares two TypedIpAddresses.
 *
 * @param[in] a TypedIpAddress number 1.  May be NULL.
 * @param[in] b TypedIpAddress number 2.  May be NULL.
 *
 * @retval TRUE  TypedIpAddresses are equivalent.
 * @retval FALSE TypedIpAddresses differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_TypedIpAddress(const TypedIpAddress *a,
                                 const TypedIpAddress *b)
{
   RETURN_EARLY_CMP_PTRS(a, b);

   if (a->ipAddressAddrType != b->ipAddressAddrType ||
       memcmp(a->ipAddressAddr.InetAddress_val,
              b->ipAddressAddr.InetAddress_val,
              a->ipAddressAddr.InetAddress_len)) {
      return FALSE;
   }

   return TRUE;
}


/*
 ******************************************************************************
 * GuestInfo_IsEqual_WinsConfigInfo --                                   */ /**
 *
 * Compares a pair of WinsConfigInfos.
 *
 * @param[in] a WinsConfigInfo number 1.  May be NULL.
 * @param[in] b WinsConfigInfo number 2.  May be NULL.
 *
 * @retval TRUE  WinsConfigInfos are equivalent.
 * @retval FALSE WinsConfigInfos differ.
 *
 ******************************************************************************
 */

Bool
GuestInfo_IsEqual_WinsConfigInfo(const WinsConfigInfo *a,
                                 const WinsConfigInfo *b)
{
   RETURN_EARLY_CMP_PTRS(a, b);

   return GuestInfo_IsEqual_TypedIpAddress(&a->primary, &b->primary) &&
          GuestInfo_IsEqual_TypedIpAddress(&a->secondary, &b->secondary);
}

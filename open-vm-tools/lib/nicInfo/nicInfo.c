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
 * @file nicInfo.c
 *
 *	Library backing parts of the vm.GuestInfo VIM APIs.
 */

#include <stdlib.h>
#include <string.h>

#if defined _WIN32
#   include <ws2tcpip.h>
#endif

#include "vm_assert.h"
#include "debug.h"
#include "nicInfoInt.h"
#include "str.h"
#include "util.h"
#include "xdrutil.h"
#include "netutil.h"
#include "wiper.h"


/**
 * Helper to initialize an opaque struct member.
 *
 * @todo Move to xdrutil.h?  Sticking point is dependency on Util_SafeMalloc.
 */
#define XDRUTIL_SAFESETOPAQUE(ptr, type, src, size)                     \
   do {                                                                 \
      (ptr)->type##_len = (size);                                       \
      (ptr)->type##_val = Util_SafeMalloc((size));                      \
      memcpy((ptr)->type##_val, (src), (size));                         \
   } while (0)

static void * Util_DupeThis(const void *source, size_t sourceSize);

/*
 * Global functions.
 */


/*
 ******************************************************************************
 * GuestInfo_GetFqdn --                                                  */ /**
 *
 * @brief Returns the guest's hostname (aka fully qualified domain name, FQDN).
 *
 * @param[in]  outBufLen Size of outBuf.
 * @param[out] outBuf    Output buffer.
 *
 * @retval TRUE  Success.  Hostname written to @a outBuf.
 * @retval FALSE Failure.
 *
 ******************************************************************************
 */

Bool
GuestInfo_GetFqdn(int outBufLen,
                  char fqdn[])
{
   return GuestInfoGetFqdn(outBufLen, fqdn);
}


/*
 ******************************************************************************
 * GuestInfo_GetNicInfo --                                               */ /**
 *
 * @brief Returns guest networking configuration (and some runtime state).
 *
 * @param[out] nicInfo  Will point to a newly allocated NicInfo.
 *
 * @note
 * Caller is responsible for freeing @a nicInfo with GuestInfo_FreeNicInfo.
 *
 * @retval TRUE  Success.  @a nicInfo now points to a populated NicInfoV3.
 * @retval FALSE Failure.
 *
 ******************************************************************************
 */

Bool
GuestInfo_GetNicInfo(NicInfoV3 **nicInfo)
{
   Bool retval = FALSE;

   *nicInfo = Util_SafeCalloc(1, sizeof (struct NicInfoV3));

   retval = GuestInfoGetNicInfo(*nicInfo);
   if (!retval) {
      GuestInfo_FreeNicInfo(*nicInfo);
      *nicInfo = NULL;
   }

   return retval;
}


/*
 ******************************************************************************
 * GuestInfo_FreeNicInfo --                                              */ /**
 *
 * @brief Frees a NicInfoV3 structure and all memory it points to.
 *
 * @param[in] nicInfo   Pointer to NicInfoV3 container.
 *
 * @sa GuestInfo_GetNicInfo
 *
 ******************************************************************************
 */

void
GuestInfo_FreeNicInfo(NicInfoV3 *nicInfo)
{
   if (nicInfo != NULL) {
      VMX_XDR_FREE(xdr_NicInfoV3, nicInfo);
      free(nicInfo);
   }
}


/*
 ******************************************************************************
 * GuestInfo_GetPrimaryIP --                                             */ /**
 *
 * @brief Get the primary IP address on the running machine.
 *
 * @note Caller is responsible for free()ing returned string.
 *
 * @return  If applicable address found, returns string of said IP address.
 *          If applicable address not found, returns an empty string.
 *
 ******************************************************************************
 */

char *
GuestInfo_GetPrimaryIP(void)
{
   char *ipstr = GuestInfoGetPrimaryIP();

   if (ipstr == NULL) {
      ipstr = Util_SafeStrdup("");
   }

   return ipstr;
}


/*
 * Private library functions.
 */


/*
 ******************************************************************************
 * GuestInfoAddNicEntry --                                               */ /**
 *
 * @brief GuestNicV3 constructor.
 *
 * @param[in,out] nicInfo     List of NICs.
 * @param[in]     macAddress  MAC address of new NIC.
 * @param[in]     dnsInfo     Per-NIC DNS config state.
 * @param[in]     winsInfo    Per-NIC WINS config state.
 *
 * @note The returned GuestNic will take ownership of @a dnsInfo and
 *       @a winsInfo  The caller must not free it directly.
 *
 * @return Pointer to the new NIC, or NULL if NIC limit was reached.
 *
 ******************************************************************************
 */

GuestNicV3 *
GuestInfoAddNicEntry(NicInfoV3 *nicInfo,
                     const char macAddress[NICINFO_MAC_LEN],
                     DnsConfigInfo *dnsInfo,
                     WinsConfigInfo *winsInfo)
{
   GuestNicV3 *newNic;

   /* Check to see if we're going above our limit. See bug 605821. */
   if (nicInfo->nics.nics_len == NICINFO_MAX_NICS) {
      g_message("%s: NIC limit (%d) reached, skipping overflow.",
                __FUNCTION__, NICINFO_MAX_NICS);
      return NULL;
   }

   newNic = XDRUTIL_ARRAYAPPEND(nicInfo, nics, 1);
   ASSERT_MEM_ALLOC(newNic);

   newNic->macAddress = Util_SafeStrdup(macAddress);
   newNic->dnsConfigInfo = dnsInfo;
   newNic->winsConfigInfo = winsInfo;

   return newNic;
}


/*
 ******************************************************************************
 * GuestInfoAddIpAddress --                                              */ /**
 *
 * @brief Add an IP address entry into the GuestNic.
 *
 * @param[in,out] nic      The NIC information.
 * @param[in]     sockAddr The new IP address.
 * @param[in]     pfxLen   Prefix length (use 0 if unknown).
 * @param[in]     origin   Address's origin.  (Optional.)
 * @param[in]     status   Address's status.  (Optional.)
 *
 * @return Newly allocated IP address struct, NULL on failure.
 *
 ******************************************************************************
 */

IpAddressEntry *
GuestInfoAddIpAddress(GuestNicV3 *nic,
                      const struct sockaddr *sockAddr,
                      InetAddressPrefixLength pfxLen,
                      const IpAddressOrigin *origin,
                      const IpAddressStatus *status)
{
   IpAddressEntry *ip;

   ASSERT(sockAddr);

   /* Check to see if we're going above our limit. See bug 605821. */
   if (nic->ips.ips_len == NICINFO_MAX_IPS) {
      g_message("%s: IP address limit (%d) reached, skipping overflow.",
                __FUNCTION__, NICINFO_MAX_IPS);
      return NULL;
   }

   ip = XDRUTIL_ARRAYAPPEND(nic, ips, 1);
   ASSERT_MEM_ALLOC(ip);

   ASSERT_ON_COMPILE(sizeof *origin == sizeof *ip->ipAddressOrigin);
   ASSERT_ON_COMPILE(sizeof *status == sizeof *ip->ipAddressStatus);

   switch (sockAddr->sa_family) {
   case AF_INET:
      {
         static const IpAddressStatus defaultStatus = IAS_PREFERRED;

         GuestInfoSockaddrToTypedIpAddress(sockAddr, &ip->ipAddressAddr);

         ip->ipAddressPrefixLength = pfxLen;
         ip->ipAddressOrigin = origin ? Util_DupeThis(origin, sizeof *origin) : NULL;
         ip->ipAddressStatus = status ? Util_DupeThis(status, sizeof *status) :
            Util_DupeThis(&defaultStatus, sizeof defaultStatus);
      }
      break;
   case AF_INET6:
      {
         static const IpAddressStatus defaultStatus = IAS_UNKNOWN;

         GuestInfoSockaddrToTypedIpAddress(sockAddr, &ip->ipAddressAddr);

         ip->ipAddressPrefixLength = pfxLen;
         ip->ipAddressOrigin = origin ? Util_DupeThis(origin, sizeof *origin) : NULL;
         ip->ipAddressStatus = status ? Util_DupeThis(status, sizeof *status) :
            Util_DupeThis(&defaultStatus, sizeof defaultStatus);
      }
      break;
   default:
      NOT_REACHED();
   }

   return ip;
}


/*
 ******************************************************************************
 * GuestInfoSockaddrToTypedIpAddress --                                  */ /**
 *
 * @brief Converts a <tt>struct sockaddr</tt> to a @c TypedIpAddress.
 *
 * @param[in]  sa       Source @c sockaddr.
 * @param[out] typedIp  Destination @c TypedIpAddress.
 *
 * @warning Caller is responsible for making sure source is AF_INET or
 * AF_INET6.
 *
 ******************************************************************************
 */

void
GuestInfoSockaddrToTypedIpAddress(const struct sockaddr *sa,
                                  TypedIpAddress *typedIp)
{
   struct sockaddr_in *sin = (struct sockaddr_in *)sa;
   struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

   switch (sa->sa_family) {
   case AF_INET:
      typedIp->ipAddressAddrType = IAT_IPV4;
      XDRUTIL_SAFESETOPAQUE(&typedIp->ipAddressAddr, InetAddress,
                            &sin->sin_addr.s_addr,
                            sizeof sin->sin_addr.s_addr);
      break;
   case AF_INET6:
      typedIp->ipAddressAddrType = IAT_IPV6;
      XDRUTIL_SAFESETOPAQUE(&typedIp->ipAddressAddr, InetAddress,
                            &sin6->sin6_addr.s6_addr,
                            sizeof sin6->sin6_addr.s6_addr);

      /*
       * Some TCP stacks (hello Apple and FreeBSD!) deviate from the RFC and
       * embed the scope id in link-local IPv6 addresses. This breaks things
       * since the address with the scope id does not work on the wire. For
       * example:
       *
       *    fe80:4::20c:29ff:fece:3dcf
       *
       * Is an invalid IPv6 address because the "4" violates the RFC. But that's
       * what SIOCGIFCONF returns on these platforms.
       *
       * Detect link-local addresses here and make sure they comply with the
       * RFC. Just for reference, link local addresses start with '1111111010'
       * and have 54 zero bits after that:
       *
       * http://tools.ietf.org/html/rfc4291#section-2.5.6
       */
      {
         uint64  ip6_ll_test = 0x80FE;
         uint64  ip6_ll_mask = 0xC0FF;
         uint64 *ip6 = (uint64 *) typedIp->ipAddressAddr.InetAddress_val;

         if ((*ip6 & ip6_ll_mask) == ip6_ll_test) {
            *ip6 &= ip6_ll_mask;
         }
      }

      break;
   default:
      NOT_REACHED();
   }
}


#if defined _WIN32
/*
 ******************************************************************************
 * GuestInfoDupTypedIpAddress--                                          */ /**
 *
 * @brief Duplicates a @c TypedIpAddress.
 *
 * @param[in]  srcIp    Source @c TypedIpAddress.
 * @param[out] destIp   Destination @c TypedIpAddress.
 *
 ******************************************************************************
 */

void
GuestInfoDupTypedIpAddress(TypedIpAddress *srcIp,   // IN
                           TypedIpAddress *destIp)  // OUT
{
   *destIp = *srcIp;
   destIp->ipAddressAddr.InetAddress_val =
      Util_DupeThis(srcIp->ipAddressAddr.InetAddress_val,
                    srcIp->ipAddressAddr.InetAddress_len);
}

#endif // if defined _WIN32


#if defined linux || defined _WIN32
/*
 ******************************************************************************
 * GuestInfoGetNicInfoIfIndex --                                         */ /**
 *
 * @brief Given a local interface's index, find its corresponding location in the
 * NicInfoV3 @c nics vector.
 *
 * @param[in]  nicInfo     NIC container.
 * @param[in]  ifIndex     Device to search for.
 * @param[out] nicifIndex  Array offset, if found.
 *
 * @retval TRUE  Device found.
 * @retval FALSE Device not found.
 *
 ******************************************************************************
 */

Bool
GuestInfoGetNicInfoIfIndex(NicInfoV3 *nicInfo,
                           int ifIndex,
                           int *nicIfIndex)
{
   char hwAddrString[NICINFO_MAC_LEN];
   unsigned char hwAddr[16];
   IanaIfType ifType;
   Bool ret = FALSE;
   u_int i;

   ASSERT(nicInfo);
   ASSERT(nicIfIndex);

   if (NetUtil_GetHardwareAddress(ifIndex, hwAddr, sizeof hwAddr,
                                  &ifType) != 6 ||
       ifType != IANA_IFTYPE_ETHERNETCSMACD) {
      return FALSE;
   }

   Str_Sprintf(hwAddrString, sizeof hwAddrString,
               "%02x:%02x:%02x:%02x:%02x:%02x",
               hwAddr[0], hwAddr[1], hwAddr[2],
               hwAddr[3], hwAddr[4], hwAddr[5]);

   XDRUTIL_FOREACH(i, nicInfo, nics) {
      GuestNicV3 *nic = XDRUTIL_GETITEM(nicInfo, nics, i);
      if (!strcasecmp(nic->macAddress, hwAddrString)) {
         *nicIfIndex = i;
         ret = TRUE;
         break;
      }
   }

   return ret;
}
#endif // if defined linux || defined _WIN32


/*
 * XXX
 */


/**
 * Return a copy of arbitrary memory.
 *
 * @param[in] source     Source address.
 * @param[in] sourceSize Number of bytes to allocate, copy.
 *
 * @return Pointer to newly allocated memory.
 *
 * @todo Determine if I'm duplicating functionality.
 * @todo Move this to bora/lib/util or whatever.
 */

static void *
Util_DupeThis(const void *source,
              size_t sourceSize)
{
   void *dest;

   ASSERT(source);

   dest = Util_SafeMalloc(sourceSize);
   memcpy(dest, source, sourceSize);

   return dest;
}

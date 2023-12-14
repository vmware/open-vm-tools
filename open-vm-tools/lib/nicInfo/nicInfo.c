/*********************************************************
 * Copyright (c) 2014-2021 VMware, Inc. All rights reserved.
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

static GPtrArray *gIfaceExcludePatterns = NULL;
static GPtrArray *gIfacePrimaryPatterns = NULL;
static GPtrArray *gIfaceLowPriorityPatterns = NULL;

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
 *
 * GuestInfoResetPatternList --
 *
 * @brief Create list of patterns (to be used with the 'exclude-nics',
 * 'primary-nics' and low-priority options).
 *
 * @param[in]   list          NULL terminated array of pointers to strings with
 *                            patterns
 * @param[out]  pPatternList  pointer to the list to be created. If not NULL,
 *                            pPatternList will be freed.
 *
 ******************************************************************************
 */

static void
GuestInfoResetPatternList(char **list,
                          GPtrArray **pPatternList)
{
   guint i;

   if (*pPatternList != NULL) {
      g_ptr_array_free(*pPatternList, TRUE);
      *pPatternList = NULL;
   }

   if (list != NULL) {
      *pPatternList =
         g_ptr_array_new_with_free_func((GDestroyNotify) &g_pattern_spec_free);
      for (i = 0; list[i] != NULL; i++) {
         if (list[i][0] != '\0') {
            g_ptr_array_add(*pPatternList, g_pattern_spec_new(list[i]));
         }
      }
   }
}


/*
 ******************************************************************************
 *
 * GuestInfo_SetIfacePrimaryList --
 *
 * @brief Set list of network interfaces that can be considered primary
 *
 * @param[in] NULL terminated array of pointers to strings with patterns
 *
 * @sa gIfacePrimaryPatterns will be set
 *
 ******************************************************************************
 */

void
GuestInfo_SetIfacePrimaryList(char **list)
{
   GuestInfoResetPatternList(list, &gIfacePrimaryPatterns);
}


/*
 *******************************************************************************
 *
 * GuestInfo_SetIfaceLowPriorityList --
 *
 * @brief Set list of network interfaces that can be considered low priority
 *
 * @param[in] NULL terminated array of pointers to strings with patterns
 *
 * @sa gIfaceLowPriorityPatterns will be set
 *
 *******************************************************************************
 */

void
GuestInfo_SetIfaceLowPriorityList(char **list)
{
   GuestInfoResetPatternList(list, &gIfaceLowPriorityPatterns);
}


/*
 *******************************************************************************
 *
 * GuestInfo_SetIfaceExcludeList --
 *
 * @brief Set list of network interfaces to be excluded
 *
 * @param[in] NULL terminated array of pointers to strings with patterns
 *
 * @sa gIfaceExcludePatterns will be set
 *
 *******************************************************************************
 */


void
GuestInfo_SetIfaceExcludeList(char **list)
{
   GuestInfoResetPatternList(list, &gIfaceExcludePatterns);
}


/*
 ******************************************************************************
 *
 * GuestInfoMatchesPatternList --
 *
 * @brief Determine if a specific name matches a pattern in a list
 *
 * @param[in] The interface name.
 * @param[in] The list of patterns
 *
 * @retval TRUE if the name matches one of the patterns in the list.
 *
 ******************************************************************************
*/

static Bool
GuestInfoMatchesPatternList(const char *name,
                            const GPtrArray *patterns)
{
   int i;

   ASSERT(name);
   ASSERT(patterns);

   for (i = 0; i < patterns->len; i++) {
      if (g_pattern_match_string(g_ptr_array_index(patterns, i),
                                 name)) {
         g_debug("%s: interface %s matched pattern %d",
                 __FUNCTION__, name, i);
         return TRUE;
      }
   }
   return FALSE;
}


/*
 ******************************************************************************
 *
 * GuestInfo_IfaceIsExcluded --
 *
 * @brief Determine if a specific interface name shall be excluded.
 *
 * @param[in] The interface name.
 *
 * @retval TRUE if interface name shall be excluded.
 *
 ******************************************************************************
*/

Bool GuestInfo_IfaceIsExcluded(const char *name)
{
   ASSERT(name);
   return gIfaceExcludePatterns != NULL &&
          GuestInfoMatchesPatternList(name, gIfaceExcludePatterns);
}


/*
 ******************************************************************************
 *
 * GuestInfo_IfaceGetPriority --
 *
 * @brief Determine priority of an interface
 *
 * @param[in] The interface name.
 *
 * @retval one of NICINFO_PRIORITY_PRIMARY, NICINFO_PRIORITY_LOW or
 * NICINFO_PRIORITY_NORMAL
 *
 ******************************************************************************
*/

NicInfoPriority
GuestInfo_IfaceGetPriority(const char *name)
{
   ASSERT(name);
   g_debug("%s: checking %s", __FUNCTION__, name);
   if (gIfacePrimaryPatterns != NULL &&
       GuestInfoMatchesPatternList(name, gIfacePrimaryPatterns)) {
      return NICINFO_PRIORITY_PRIMARY;
   } else if (gIfaceLowPriorityPatterns != NULL &&
              GuestInfoMatchesPatternList(name, gIfaceLowPriorityPatterns)) {
      return NICINFO_PRIORITY_LOW;
   }
   return NICINFO_PRIORITY_NORMAL;
}


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
 * @param[in]  maxIPv4Routes  Max IPv4 routes to gather.
 * @param[in]  maxIPv6Routes  Max IPv6 routes to gather.
 * @param[out] nicInfo        Will point to a newly allocated NicInfo.
 * @param[out] maxNicsError   To determine NIC max limit error.
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
GuestInfo_GetNicInfo(unsigned int maxIPv4Routes,
                     unsigned int maxIPv6Routes,
                     NicInfoV3 **nicInfo,
                     Bool *maxNicsError)
{
   Bool retval;

   *nicInfo = Util_SafeCalloc(1, sizeof (struct NicInfoV3));

   retval = GuestInfoGetNicInfo(maxIPv4Routes, maxIPv6Routes, *nicInfo,
                                maxNicsError);
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
 * @param[in,out] nicInfo       List of NICs.
 * @param[in]     macAddress    MAC address of new NIC.
 * @param[in]     dnsInfo       Per-NIC DNS config state.
 * @param[in]     winsInfo      Per-NIC WINS config state.
 * @param[out]    maxNicsError  To determine NIC max limit error.
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
                     WinsConfigInfo *winsInfo,
                     Bool *maxNicsError)
{
   GuestNicV3 *newNic;

   /* Check to see if we're going above our limit. See bug 605821. */
   if (nicInfo->nics.nics_len == NICINFO_MAX_NICS) {
      if (maxNicsError != NULL) {
         *maxNicsError = TRUE;
      }
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


#if defined __linux__ || defined _WIN32
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
#endif // if defined __linux__ || defined _WIN32


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

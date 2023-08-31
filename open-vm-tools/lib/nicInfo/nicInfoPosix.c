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
 * @file nicInfoPosix.c
 *
 * Contains POSIX-specific bits of GuestInfo collector library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef sun
# include <sys/systeminfo.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
# include <ifaddrs.h>
# include <net/if.h>
#endif
#ifndef NO_DNET
# ifdef DNET_IS_DUMBNET
#  include <dumbnet.h>
# else
#  include <dnet.h>
# endif
#define USE_RESOLVE 1
#endif

#if defined(USERWORLD) || (defined(__linux__) && defined(NO_DNET))
#include "vm_basic_defs.h"
#include <net/if.h>
#include <netpacket/packet.h>
#include <ifaddrs.h>

#define USE_RESOLVE 1
#endif

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifdef __linux__
#   include <net/if.h>
#endif

/*
 * resolver(3) and IPv6:
 *
 * The ISC BIND resolver included various IPv6 implementations over time, but
 * unfortunately the ISC hadn't bumped __RES accordingly.  (__RES is -supposed-
 * to behave as a version datestamp for the resolver interface.)  Similarly
 * the GNU C Library forked resolv.h and made modifications of their own, also
 * without changing __RES.
 *
 * ISC, OTOH, provided accessing IPv6 servers via a res_getservers API.
 * TTBOMK, this went public with BIND 8.3.0.  Unfortunately __RES wasn't
 * bumped for this release, so instead I'm going to assume that appearance with
 * that release of a new macro, RES_F_EDNS0ERR, implies this API is available.
 * (For internal builds, we'll know instantly when a build breaks.  The down-
 * side is that this could cause some trouble for open-vm-tools users. ,_,)
 *
 * resolv.h version     IPv6 API        __RES
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * glibc 2.2+           _ext            19991006
 * BIND 8.3.0           getservers      19991006
 * BIND 8.3.4+          getservers      20030124(+?)
 *
 * To distinguish between the variants where __RES == 19991006, I'll
 * discriminate on the existence of new macros included with the appropriate
 * version.
 */

#if defined __linux__
#   define      RESOLVER_IPV6_EXT
#elif (__RES > 19991006 || (__RES == 19991006 && defined RES_F_EDNS0ERR))
#   define      RESOLVER_IPV6_GETSERVERS
#endif // if defined __linux__


#include "util.h"
#include "sys/utsname.h"
#include "sys/ioctl.h"
#include "vmware.h"
#include "hostinfo.h"
#include "nicInfoInt.h"
#include "debug.h"
#include "str.h"
#include "guest_os.h"
#include "guestApp.h"
#include "guestInfo.h"
#include "xdrutil.h"
#ifdef USE_SLASH_PROC
#   include "slashProc.h"
#endif
#include "netutil.h"
#include "file.h"

#ifndef IN6_IS_ADDR_UNIQUELOCAL
#define IN6_IS_ADDR_UNIQUELOCAL(a)        \
        (((a)->s6_addr[0] == 0xfc) && (((a)->s6_addr[1] & 0xc0) == 0x00))
#endif


/*
 * Local functions
 */


#ifndef NO_DNET
static Bool RecordNetworkAddress(GuestNicV3 *nic, const struct addr *addr);
static int ReadInterfaceDetails(const struct intf_entry *entry,
                                void *arg,
                                NicInfoPriority priority);
static int ReadInterfaceDetailsPrimary(const struct intf_entry *entry,
                                       void *arg);
static int ReadInterfaceDetailsNormal(const struct intf_entry *entry,
                                      void *arg);
static int ReadInterfaceDetailsLowPriority(const struct intf_entry *entry,
                                           void *arg);
static Bool RecordRoutingInfo(unsigned int maxIPv4Routes,
                              unsigned int maxIPv6Routes,
                              NicInfoV3 *nicInfo);

#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
typedef struct GuestInfoIpPriority {
   char *ipstr;
   NicInfoPriority priority;
} GuestInfoIpPriority;

static int GuestInfoGetIntf(const struct intf_entry *entry, void *arg);
#endif

#endif

static Bool RecordRoutingInfo(unsigned int maxIPv4Routes,
                              unsigned int maxIPv6Routes,
                              NicInfoV3 *nicInfo);

static char *ValidateConvertAddress(const struct sockaddr *addr);


#ifdef USE_RESOLVE
static Bool RecordResolverInfo(NicInfoV3 *nicInfo);
static void RecordResolverNS(res_state resp, DnsConfigInfo *dnsConfigInfo);
static int AddResolverNSInfo(DnsConfigInfo *dnsConfigInfo, struct sockaddr *sap);
#   ifndef RESOLVER_IPV6_GETSERVERS
static void PrintResolverNSInfo(res_state resp);
#   endif
#endif


/*
 ******************************************************************************
 * GuestInfoGetFqdn --                                                   */ /**
 *
 * @copydoc GuestInfo_GetFqdn
 *
 ******************************************************************************
 */

Bool
GuestInfoGetFqdn(int outBufLen,    // IN: length of output buffer
                 char fqdn[])      // OUT: fully qualified domain name
{
   ASSERT(fqdn);
   if (gethostname(fqdn, outBufLen) < 0) {
      g_debug("Error, gethostname failed\n");
      return FALSE;
   }

   return TRUE;
}


#if defined(USERWORLD) || defined(USE_SLASH_PROC) || (defined(__linux__) && defined(NO_DNET))
/*
 ******************************************************************************
 * CountNetmaskBits --                                                   */ /**
 * CountNetmaskBitsV4 --                                                 */ /**
 * CountNetmaskBitsV6 --                                                 */ /**
 *
 * @brief Count the number of bits set in a IPV4 or IPV6 netmask
 *
 * @retval the number of bits set
 *
 ******************************************************************************
 */

static unsigned
CountNetmaskBits(uint64_t x)
{
   /* SWAR reduction, much faster than using the loop/shift */
   const uint64_t m1  = 0x5555555555555555ull; /* binary: 0101... */
   const uint64_t m2  = 0x3333333333333333ull; /* binary: 00110011 */
   const uint64_t m4  = 0x0f0f0f0f0f0f0f0full; /* binary:  4 zeros,  4 ones */

   x -= (x >> 1) & m1;             /* each 2 bits into those 2 bits */
   x = (x & m2) + ((x >> 2) & m2); /* each 4 bits into those 4 bits */
   x = (x + (x >> 4)) & m4;        /* and so on ... */
   x += x >>  8;
   x += x >> 16;
   x += x >> 32;
   return x & 0x7f;
}

static unsigned
CountNetmaskBitsV4(struct sockaddr *netmask)
{
   uint64_t value = ((struct sockaddr_in *)netmask)->sin_addr.s_addr;
   return CountNetmaskBits(value);
}
#endif

#if defined(USERWORLD) || (defined(__linux__) && defined(NO_DNET))
static unsigned
CountNetmaskBitsV6(struct sockaddr *netmask)
{
   uint64_t *value = (uint64_t *)&((struct sockaddr_in6 *)netmask)->sin6_addr;

   return CountNetmaskBits(value[0]) + CountNetmaskBits(value[1]);
}


/*
 ******************************************************************************
 * IpEntryMatchesDevice --                                               */ /**
 *
 * @brief Check if the IP entry matches the network device.
 *
 * @param[in]   devName the device name
 * @param[in]   label   the IP entry name
 *
 * @retval      TRUE if the IP entry name matches the device name
 *              FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
IpEntryMatchesDevice(const char *devName,
                     const char *label)
{
   char *p;
   size_t n;

   if ((p = strchr(label, ':')) != NULL) {
      n = p - label;
   } else {
      n = strlen(label);
   }

   /* compare sub string label[0, n) with a null terminated string devName */
   return (0 == strncmp(devName, label, n) && '\0' == devName[n]);
}


/*
 ******************************************************************************
 * GuestInfoGetInterface --                                              */ /**
 *
 * @brief Gather IP addresses from ifaddrs and put into NicInfo, filtered
 *        by priority.
 *
 * @param[in]   ifaddrs       ifaddrs structure
 * @param[in]   priority      the priority - only interfaces with this priority
 *                            will be considered
 * @param[out]  nicInfo       NicInfoV3 structure
 * @param[out]  maxNicsError  To determine NIC max limit error.
 *
 ******************************************************************************
 */

static void
GuestInfoGetInterface(struct ifaddrs *ifaddrs,
                      NicInfoPriority priority,
                      NicInfoV3 *nicInfo,
                      Bool *maxNicsError)
{
   struct ifaddrs *pkt;
   /*
    * ESXi reports an AF_PACKET record for each physical interface.
    * The MAC address is the first six bytes of sll_addr.  AF_PACKET
    * records are intermingled with AF_INET and AF_INET6 records.
    */
   for (pkt = ifaddrs; pkt != NULL; pkt = pkt->ifa_next) {
      struct sockaddr_ll *sll = (struct sockaddr_ll *)pkt->ifa_addr;

      if (GuestInfo_IfaceGetPriority(pkt->ifa_name) != priority ||
          GuestInfo_IfaceIsExcluded(pkt->ifa_name)) {
         continue;
      }

      if (sll != NULL && sll->sll_family == AF_PACKET) {
         char macAddress[NICINFO_MAC_LEN];
         GuestNicV3 *nic;
         struct ifaddrs *ip;

         /*
          * PR 2193804:
          * On ESXi, AF_PACKET family is reported for vmk* interfaces only
          * and its ifa_flags is reported as 0. No AF_PACKET family ifaddrs
          * is reported for loopback interface.
          */
#if !defined(USERWORLD)
         /*
          * Ignore loopback and downed devices.
          */
         if (!(pkt->ifa_flags & IFF_UP) || pkt->ifa_flags & IFF_LOOPBACK) {
            continue;
         }
#endif

         Str_Sprintf(macAddress, sizeof macAddress,
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
                     sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
         nic = GuestInfoAddNicEntry(nicInfo, macAddress, NULL, NULL,
                                    maxNicsError);
         if (nic == NULL) {
            /*
             * We reached the maximum number of NICs that we can report.
             */
            break;
         }
         /*
          * Now look for all IPv4 and IPv6 interfaces that match
          * the current AF_PACKET interface.
          */
         for (ip = ifaddrs; ip != NULL; ip = ip->ifa_next) {
            struct sockaddr *sa = (struct sockaddr *)ip->ifa_addr;
            if (sa != NULL &&
                IpEntryMatchesDevice(pkt->ifa_name, ip->ifa_name)) {
               int family = sa->sa_family;
               Bool goodAddress = FALSE;
               unsigned nBits = 0;
               /*
                * Ignore any loopback addresses.
                * A loopback address would indicate a misconfiguration, since
                * this is not a loopback device (we checked for that above).
                */
               if (family == AF_INET) {
                  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
                  if ((ntohl(sin->sin_addr.s_addr) >> IN_CLASSA_NSHIFT) !=
                      IN_LOOPBACKNET) {
                     nBits = CountNetmaskBitsV4(ip->ifa_netmask);
                     goodAddress = TRUE;
                  }
               } else if (family == AF_INET6) {
                  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
                  if (!IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr)) {
                     nBits = CountNetmaskBitsV6(ip->ifa_netmask);
                     goodAddress = TRUE;
                  }
               }
               if (goodAddress) {
                  IpAddressEntry *ent = GuestInfoAddIpAddress(nic,
                                                              ip->ifa_addr,
                                                              nBits, NULL,
                                                              NULL);
                  if (NULL == ent) {
                     /*
                      * Reached the max number of IPs that can be reported
                      */
                     break;
                  }
               }
            }
         }
      }
   }
}
#endif


/*
 ******************************************************************************
 * GuestInfoGetNicInfo --                                                */ /**
 *
 * @param[in]  maxIPv4Routes  Max IPv4 routes to gather.
 * @param[in]  maxIPv6Routes  Max IPv6 routes to gather.
 * @param[out] nicInfo        NicInfoV3 container.
 * @param[out] maxNicsError   To determine NIC max limit error.
 *
 * @copydoc GuestInfo_GetNicInfo
 *
 ******************************************************************************
 */

Bool
GuestInfoGetNicInfo(unsigned int maxIPv4Routes,
                    unsigned int maxIPv6Routes,
                    NicInfoV3 *nicInfo,
                    Bool *maxNicsError)
{
#ifndef NO_DNET
   intf_t *intf;

   /* Get a handle to read the network interface configuration details. */
   if ((intf = intf_open()) == NULL) {
      g_warning("%s: intf_open() failed\n", __FUNCTION__);
      return FALSE;
   }

   /*
    * Iterate through the list of interfaces thrice - first for interfaces
    * considered to be primary; second for others, non-specified as primary
    * or low priority; and low-priority last. This ensures interfaces are
    * handled in the specified order.
    */
   if (intf_loop(intf, ReadInterfaceDetailsPrimary, nicInfo) < 0 ||
       intf_loop(intf, ReadInterfaceDetailsNormal, nicInfo) < 0 ||
       intf_loop(intf, ReadInterfaceDetailsLowPriority, nicInfo) < 0) {
      intf_close(intf);
      g_debug("%s: Error, negative result from intf_loop\n", __FUNCTION__);
      return FALSE;
   }

   intf_close(intf);

#ifdef USE_RESOLVE
   if (!RecordResolverInfo(nicInfo)) {
      return FALSE;
   }
#endif

   if ((maxIPv4Routes > 0 || maxIPv6Routes > 0) &&
       !RecordRoutingInfo(maxIPv4Routes, maxIPv6Routes, nicInfo)) {
      return FALSE;
   }

   return TRUE;
#elif defined(USERWORLD) || defined(__linux__)
   struct ifaddrs *ifaddrs = NULL;

   if (getifaddrs(&ifaddrs) == 0 && ifaddrs != NULL) {
      NicInfoPriority priority;

      /*
       * Handle primary interfaces first, then non-primary ones.
       */
      for (priority = NICINFO_PRIORITY_PRIMARY;
           priority < NICINFO_PRIORITY_MAX;
           priority++) {
         GuestInfoGetInterface(ifaddrs, priority, nicInfo, maxNicsError);
      }
      freeifaddrs(ifaddrs);
   }

#ifdef USE_RESOLVE
   if (!RecordResolverInfo(nicInfo)) {
      return FALSE;
   }
#endif

   if ((maxIPv4Routes > 0 || maxIPv6Routes > 0) &&
       !RecordRoutingInfo(maxIPv4Routes, maxIPv6Routes, nicInfo)) {
      return FALSE;
   }

   return TRUE;
#else
   (void)maxIPv4Routes;
   (void)maxIPv6Routes;
   (void)nicInfo;

   return FALSE;
#endif
}


/*
 ******************************************************************************
 * GuestInfoGetPrimaryIP --                                              */ /**
 *
 * @copydoc GuestInfo_GetPrimaryIP
 *
 ******************************************************************************
 */
#if defined(__FreeBSD__) || \
    defined(__APPLE__) || \
    defined(USERWORLD) || \
    (defined(__linux__) && defined(NO_DNET))

char *
GuestInfoGetPrimaryIP(void)
{
   struct ifaddrs *ifaces;
   struct ifaddrs *curr;
   char *currIpstr = NULL;
   NicInfoPriority currPri = NICINFO_PRIORITY_MAX;

   /*
    * getifaddrs(3) creates a NULL terminated linked list of interfaces for us
    * to traverse and places a pointer to it in ifaces.
    */
   if (getifaddrs(&ifaces) < 0) {
      return NULL;
   }

   /*
    * We traverse the list until there are no more interfaces or we have found
    * the primary interface. This function defines the primary interface to be
    * the first non-loopback, internet interface in the interface list.
    */
   for (curr = ifaces; curr != NULL; curr = curr->ifa_next) {
      char *ipstr = NULL;
      int currFamily;

      /*
       * Some interfaces ("tun") have no ifa_addr, so ignore them.
       */
      if (NULL == curr->ifa_addr) {
         continue;
      }
      currFamily = ((struct sockaddr_storage *)curr->ifa_addr)->ss_family;

      if (!(curr->ifa_flags & IFF_UP) || curr->ifa_flags & IFF_LOOPBACK) {
         continue;
      } else if (GuestInfo_IfaceIsExcluded(curr->ifa_name)) {
         continue;
      } else if (currFamily == AF_INET || currFamily == AF_INET6) {
         ipstr = ValidateConvertAddress(curr->ifa_addr);
      } else {
         continue;
      }

      if (ipstr != NULL) {
         NicInfoPriority pri = GuestInfo_IfaceGetPriority(curr->ifa_name);
         if (pri < currPri) {
            g_debug("%s: ifa_name=%s, pri=%d, currPri=%d, ipstr=%s",
                    __FUNCTION__, curr->ifa_name, pri, currPri, ipstr);
            free(currIpstr);
            currIpstr = ipstr;
            currPri = pri;
            if (pri == NICINFO_PRIORITY_PRIMARY) {
               /* not going to find anything better than that */
               break;
            }
         } else {
            free(ipstr);
         }
      }
   }

   freeifaddrs(ifaces);

   return currIpstr;
}

#else

#ifndef NO_DNET

char *
GuestInfoGetPrimaryIP(void)
{
   GuestInfoIpPriority ipp;
   intf_t *intf = intf_open();

   if (NULL == intf) {
      g_warning("%s: intf_open() failed\n", __FUNCTION__);
      return NULL;
   }

   ipp.ipstr = NULL;
   for (ipp.priority = NICINFO_PRIORITY_PRIMARY;
       ipp.priority < NICINFO_PRIORITY_MAX;
       ipp.priority++){
      intf_loop(intf, GuestInfoGetIntf, &ipp);
      if (ipp.ipstr != NULL) {
         break;
      }
   }
   intf_close(intf);

   g_debug("%s: returning '%s'",
           __FUNCTION__, ipp.ipstr ? ipp.ipstr : "<null>");

   return ipp.ipstr;
}
#else
#   error GuestInfoGetPrimaryIP needed for this platform
#endif
#endif


/*
 * Local functions
 */


#ifndef NO_DNET
/*
 ******************************************************************************
 * RecordNetworkAddress --                                               */ /**
 *
 * @brief Massages a dnet(3)-style interface address (IPv4 or IPv6) and stores
 *        it as part of a GuestNicV3 structure.
 *
 * @param[in]  nic      Operand NIC.
 * @param[in]  addr     dnet(3) address.
 *
 * @retval TRUE on success
 *
 ******************************************************************************
 */

static Bool
RecordNetworkAddress(GuestNicV3 *nic,           // IN: operand NIC
                     const struct addr *addr)   // IN: dnet(3) address to process
{
   struct sockaddr_storage ss;
   struct sockaddr *sa = (struct sockaddr *)&ss;
   const IpAddressEntry *ip;

   memset(&ss, 0, sizeof ss);
   addr_ntos(addr, sa);
   ip = GuestInfoAddIpAddress(nic, sa, addr->addr_bits, NULL, NULL);
   if (NULL == ip) {
      return FALSE;
   }
   return TRUE;
}


/*
 ******************************************************************************
 * ReadInterfaceDetails --                                               */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host. Cannot be used as a callback directly, see wrappers below.
 *
 * @param[in]  entry      Current interface entry.
 * @param[in]  arg        Pointer to NicInfoV3 container.
 * @param[in]  priority   Which priority interfaces to consider
 *
 * @note New GuestNicV3 structures are added to the NicInfoV3 structure.
 *
 * @retval 0    Success.
 * @retval -1   Failure.
 *
 ******************************************************************************
 */

static int
ReadInterfaceDetails(const struct intf_entry *entry, // IN
                     void *arg,                      // IN
                     NicInfoPriority priority)       // IN
{
   NicInfoV3 *nicInfo = arg;

   ASSERT(entry);
   ASSERT(arg);

   if (entry->intf_type == INTF_TYPE_ETH &&
       entry->intf_link_addr.addr_type == ADDR_TYPE_ETH) {

      /*
       * There is a race where the guest info plugin might be iterating over the
       * interfaces while the OS is modifying them (i.e. by bringing them up
       * after a resume). If we see an ethernet interface with an invalid MAC,
       * then ignore it for now. Subsequent iterations of the gather loop will
       * pick up any changes.
       */
      if (entry->intf_link_addr.addr_type == ADDR_TYPE_ETH) {
         char macAddress[NICINFO_MAC_LEN];
         GuestNicV3 *nic = NULL;
         int i;

         Str_Sprintf(macAddress, sizeof macAddress, "%s",
                     addr_ntoa(&entry->intf_link_addr));

         if (GuestInfo_IfaceIsExcluded(entry->intf_name) ||
             GuestInfo_IfaceGetPriority(entry->intf_name) != priority) {
            return 0;
         }

         nic = GuestInfoAddNicEntry(nicInfo, macAddress, NULL, NULL, NULL);
         if (NULL == nic) {
            /*
             * We reached maximum number of NICs we can report to the host.
             */
            return 0;
         }

         /* Record the "primary" address. */
         if (entry->intf_addr.addr_type == ADDR_TYPE_IP ||
             entry->intf_addr.addr_type == ADDR_TYPE_IP6) {
            if (!RecordNetworkAddress(nic, &entry->intf_addr)) {
               /*
                * We reached maximum number of IPs we can report to the host.
                */
               return 0;
            }
         }

         /* Walk the list of alias's and add those that are IPV4 or IPV6 */
         for (i = 0; i < entry->intf_alias_num; i++) {
            const struct addr *alias = &entry->intf_alias_addrs[i];
            if (alias->addr_type == ADDR_TYPE_IP ||
                alias->addr_type == ADDR_TYPE_IP6) {
               if (!RecordNetworkAddress(nic, alias)) {
                  /*
                   * We reached maximum number of IPs we can report to the host.
                   */
                  return 0;
               }
            }
         }
      }
   }

   return 0;
}


/*
 ******************************************************************************
 * ReadInterfaceDetailsPrimary --                                        */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host. Calls ReadInterfaceDetails with the priority param set to
 * NICINFO_PRIORITY_PRIMARY.
 *
 * @param[in]  entry     Current interface entry.
 * @param[in]  arg       Pointer to NicInfoV3 container.
 *
 * @note New GuestNicV3 structures are added to the NicInfoV3 structure.
 *
 * @retval 0    Success.
 * @retval -1   Failure.
 *
 ******************************************************************************
 */

static int
ReadInterfaceDetailsPrimary(const struct intf_entry *entry,
                            void *arg)
{
   return ReadInterfaceDetails(entry, arg, NICINFO_PRIORITY_PRIMARY);
}


/*
 ******************************************************************************
 * ReadInterfaceDetailsNormal --                                      */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host. Calls ReadInterfaceDetails with the priority param set to
 * NICINFO_PRIORITY_NORMAL.
 *
 * @param[in]  entry     Current interface entry.
 * @param[in]  arg       Pointer to NicInfoV3 container.
 *
 * @note New GuestNicV3 structures are added to the NicInfoV3 structure.
 *
 * @retval 0    Success.
 * @retval -1   Failure.
 *
 ******************************************************************************
 */

static int
ReadInterfaceDetailsNormal(const struct intf_entry *entry,
                           void *arg)
{
   return ReadInterfaceDetails(entry, arg, NICINFO_PRIORITY_NORMAL);
}

/*
 ******************************************************************************
 * ReadInterfaceDetailsLowPriority --                                    */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host. Calls ReadInterfaceDetails with the priority param set to
 * NICINFO_PRIORITY_LOW.
 *
 * @param[in]  entry     Current interface entry.
 * @param[in]  arg       Pointer to NicInfoV3 container.
 *
 * @note New GuestNicV3 structures are added to the NicInfoV3 structure.
 *
 * @retval 0    Success.
 * @retval -1   Failure.
 *
 ******************************************************************************
 */


static int
ReadInterfaceDetailsLowPriority(const struct intf_entry *entry,
                                void *arg)
{
   return ReadInterfaceDetails(entry, arg, NICINFO_PRIORITY_LOW);
}


#endif // !NO_DNET


#ifdef USE_RESOLVE

/*
 ******************************************************************************
 * RecordResolverInfo --                                                 */ /**
 *
 * @brief Query resolver(3), mapping settings to DnsConfigInfo.
 *
 * @param[out] nicInfo  NicInfoV3 container.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordResolverInfo(NicInfoV3 *nicInfo)  // OUT
{
   DnsConfigInfo *dnsConfigInfo = NULL;
   char namebuf[DNSINFO_MAX_ADDRLEN + 1];
   char **s;
   struct __res_state res;

   memset(&res, 0, sizeof res);
   if (res_ninit(&res) == -1) {
      g_warning("%s: Resolver res_init failed.\n", __FUNCTION__);
      return FALSE;
   }

   dnsConfigInfo = Util_SafeCalloc(1, sizeof *dnsConfigInfo);

   /*
    * Copy in the host name.
    */
   if (!GuestInfoGetFqdn(sizeof namebuf, namebuf)) {
      goto fail;
   }
   dnsConfigInfo->hostName =
      Util_SafeCalloc(1, sizeof *dnsConfigInfo->hostName);
   *dnsConfigInfo->hostName = Util_SafeStrdup(namebuf);

   /*
    * Repeat with the domain name.
    */
   dnsConfigInfo->domainName =
      Util_SafeCalloc(1, sizeof *dnsConfigInfo->domainName);
   *dnsConfigInfo->domainName = Util_SafeStrdup(res.defdname);

   /*
    * Name servers.
    */
   RecordResolverNS(&res, dnsConfigInfo);

   /*
    * Search suffixes.
    */
   for (s = res.dnsrch; *s; s++) {
      DnsHostname *suffix;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (dnsConfigInfo->searchSuffixes.searchSuffixes_len == DNSINFO_MAX_SUFFIXES) {
         g_message("%s: dns search suffix limit (%d) reached, skipping overflow.",
                   __FUNCTION__, DNSINFO_MAX_SUFFIXES);
         break;
      }

      suffix = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, searchSuffixes, 1);
      ASSERT_MEM_ALLOC(suffix);
      *suffix = Util_SafeStrdup(*s);
   }

   /*
    * "Commit" dnsConfigInfo to nicInfo.
    */
   nicInfo->dnsConfigInfo = dnsConfigInfo;

   res_nclose(&res);

   return TRUE;

fail:
   VMX_XDR_FREE(xdr_DnsConfigInfo, dnsConfigInfo);
   free(dnsConfigInfo);
   res_nclose(&res);

   return FALSE;
}

/*
 ******************************************************************************
 * RecordResolverNSResolvConf                                            */ /**
 *
 * @brief Parses file in resolv.conf format, and copies name servers to
 * @a dnsConfigInfo.
 *
 * @param[in]  file             path of file to read
 * @param[out] dnsConfigInfo    Destination DnsConfigInfo container
 *
 ******************************************************************************
 */

static void
RecordResolverNSResolvConf(const char *file, DnsConfigInfo *dnsConfigInfo)
{
   FILE *fptr;

   fptr = fopen(file, "rt");
   if (fptr != NULL) {
      char line[256];
      while (fgets(line, sizeof(line), fptr)) {
         char *tok, *saveptr = NULL;
         tok = strtok_r(line, " \t", &saveptr);
         if ((tok != NULL) && (strcmp(tok, "nameserver") == 0)) {
            char *val;
            struct sockaddr_in sa4;
            struct sockaddr_in6 sa6;
            val = strtok_r(NULL, " \t\r\n", &saveptr);
            if (val == NULL) {
               g_warning("%s: no value for nameserver in %s\n",
                         __FUNCTION__, file);
            } else if (inet_pton(AF_INET, val, &sa4.sin_addr)) {
               sa4.sin_family = AF_INET;
               if (0 == AddResolverNSInfo(dnsConfigInfo,
                                          (struct sockaddr *) &sa4)) {
                  /* Can't add more DNS entries to dnsConfigInfo, break loop */
                  break;
               }
            } else if (inet_pton(AF_INET6, val, &sa6.sin6_addr)) {
               sa6.sin6_family = AF_INET6;
               if (0 == AddResolverNSInfo(dnsConfigInfo,
                                          (struct sockaddr *) &sa6)) {
                  /* Can't add more DNS entries to dnsConfigInfo, break loop */
                  break;
               }
            } else {
               g_warning("%s: invalid IP address '%s' in %s ignored\n",
                         __FUNCTION__, val, file);
            }
         }
      }
      fclose(fptr);
   } else {
      g_warning("%s: could not open file '%s': %s\n", __FUNCTION__, file, strerror(errno));
   }
}

/*
 ******************************************************************************
 * RecordResolverNS --                                                   */ /**
 *
 * @brief Copies name servers used by resolver(3) to @a dnsConfigInfo.
 *
 * @param[in]  resp             res_state, pointer to a resolver(3) state
 *                              structure.
 * @param[out] dnsConfigInfo    Destination DnsConfigInfo container.
 *
 ******************************************************************************
 */

static void
RecordResolverNS(res_state resp, DnsConfigInfo *dnsConfigInfo) // IN
{
   int i;
   char resolvConf[PATH_MAX];

   /*
    * If systemd-resolved is used we want to report the external DNS
    * server, not the locally installed one. We detect this by checking
    * if /etc/resolv.conf is a link to /run/systemd/resolve/stub-resolv.conf.
    * In that case, /run/systemd/resolve/resolv.conf will hold the actual
    * DNS server. See
    * https://www.freedesktop.org/software/systemd/man/systemd-resolved.service.html
    */
   if (realpath("/etc/resolv.conf", resolvConf) != NULL) {
      if (strcmp(resolvConf, "/run/systemd/resolve/stub-resolv.conf") == 0) {
         const char *file = "/run/systemd/resolve/resolv.conf";
         if (access(file, R_OK) != -1) {
            RecordResolverNSResolvConf(file, dnsConfigInfo);
            return;
         } else {
            g_debug("%s: could not access %s for reading: %s\n",
                    __FUNCTION__, file, strerror(errno));
         }
      }
   }

#if defined RESOLVER_IPV6_GETSERVERS
   {
      union res_sockaddr_union *ns;
      ns = Util_SafeCalloc(resp->nscount, sizeof *ns);
      if (res_getservers(resp, ns, resp->nscount) != resp->nscount) {
         g_warning("%s: res_getservers failed.\n", __FUNCTION__);
         /* fallthrough to free & return */
      } else {
         for (i = 0; i < resp->nscount; i++) {
            if (0 == AddResolverNSInfo(
                        dnsConfigInfo,
                        (struct sockaddr *)&ns[i])) {
               /* Can't add more DNS entries to dnsConfigInfo, break loop */
               break;
            }
            /* Else: if < 0, reason already logged in AddResolverNSInfo */
         }
      }

      /* free and return */
      free(ns);
   }
#else                                   // if defined RESOLVER_IPV6_GETSERVERS
   {
      /*
       * GLIBC resolv implementation details to consider...
       *
       * In mixed mode, where both IPv4 and IPv6 nameserver are configured,
       * both arrays (__res_state.nsaddr_list, __res_state._u._ext.nsaddrs) are
       * combined to for the list of nameservers.
       *
       * The __res_state.nscount value is limited to MAXNS and represent the
       * current number of 'managed' or 'valid' nameserver entries across
       * both list.
       *
       * However, not everyone manages the arrays the same way (of course...).
       *
       * Assume resolv.conf:
       *    nameserver A (IPv4)
       *    nameserver B (IPv6)
       *    nameserver C (IPv4)
       *
       * Case 1: front-loaded
       *         IPv4 entries are front-loaded in __res_state.nsaddr_list[] and
       *         IPv6 entries are positional in __res_state._u._ext.nsaddrs[]
       *         position matches /etc/resolv.conf order
       *           __res_state.nsaddr_list | __res_state._u._ext.nsaddrs
       *                  -----------------+--------------------
       *                          A        |       -
       *                          C        |       B
       *                          -        |       -
       *
       * Case 2: staggered / positional
       *         IPv4 entries are staggered with IPv6 entries, thus positional
       *         in both arrays relative to each other.
       *           __res_state.nsaddr_list | __res_state._u._ext.nsaddrs
       *                  -----------------+--------------------
       *                          A        |       -
       *                          -        |       B
       *                          C        |       -
       *
       * Both cases are enhanced by different IPv4 data management features:
       *    Variation 1: IPv4 sin_family is not cleared.
       *    Variation 2: IPv4 sin_family is cleared (set to 0)
       *
       * Distro+release | nscount6 |  Case      | sin_family  |
       * =======================================================================
       *  RHEL 6.10     |   yes    | front-load | not cleared |
       *  RHEL 7.2      |   yes    | front-load | not cleared |
       *  RHEL 7.6      |   no     | staggered  |  post-fix   |
       * -----------------------------------------------------------------------
       *  SLES 11-sp3   |   yes    | staggered  | not cleared | **bad
       *  SLES 12       |   no     | staggered  |   cleared   |
       * -----------------------------------------------------------------------
       *  FreeBSD 11    |  -na-    |    -na-    |     -na-    | res_getservers
       * -----------------------------------------------------------------------
       *  Ubuntu 12.04  |   yes    | staggered  | not cleared | **bad
       *  Ubuntu 14.04  |   yes    | staggered  | not cleared | **bad
       *  Ubuntu 18.04  |   no     | staggered  |   cleared   |
       * -----------------------------------------------------------------------
       *  Fedora 24     |   no     | staggered  |   cleared   |
       * =======================================================================
       *
       * Restarting the process (service: vmtoolsd) resets the IPv4 array and
       * fixes issues that arise with "staggered + not cleared" combos, as well
       * as clearing out DNS reporting info issues for ealier versions.
       *
       * Fix for PR 2302591:
       *  - Move away from using the _res global __res_state var and instead use
       *    a locally allocated __res_state structure.
       *     *  Ensures the IPv4 data is reset every time we collect DNS info.
       *     *  Calling res_nclose on the resolver structure and freeing it at
       *        the end of the configuration update ensure the IPv6 structure
       *        are not leaked (see: RecordResolverInfo())
       *  - Log the res_state nameserver configuration for debugging.
       *  - Validate and compute nscount4, nscount6 values
       *  - Pick DNS entries from both list, with the IPv6 array driving the
       *    order of the entries. Increment the '4 or '6 count when a DNS entry
       *    is selected.
       *
       * Clearing the IPv4 sin_family is REQUIRED to support all cases.
       * Otherwise the "staggered+'not cleared'" and "front-load" scenarios are
       * indistinguishable.
       *
       * Manually clearing the IPv4 sin_family field can be done after close if
       * continued use of res_init() and the _res global is desirable.
       */
      int addRC;
      int nscount4 = 0; /* number of IPv4 entries collected */
      int nscount6 = 0; /* number of IPv6 entries collected */

      PrintResolverNSInfo(resp);

      /*
       * Collect DNS entries
       *   There are up to __res_state.nscount valid entries, and nscount is
       *   limited to the array size (MAXNS).
       *   Loop up to __res_state.nscount, or until the target number of entries
       *   (IPv4 and IPv6) are collected.
       *   Requires IPv4 sin_family to be set to 0 for unused entries in
       *   __res_state.nsaddr_list[].
       */
      for (i = 0; i < resp->nscount && ((nscount4 + nscount6) < resp->nscount); i++) {
#   if defined RESOLVER_IPV6_EXT
         /*
          * IPv6 list is authoritative at a given index, process it first
          */
         if (resp->_u._ext.nsaddrs[i]) {
            /* Has IPv6 nameserver at index */
            addRC = AddResolverNSInfo(
                       dnsConfigInfo,
                       (struct sockaddr *)resp->_u._ext.nsaddrs[i]);
            if (addRC == 0) {
               /* Can't add more DNS entries to dnsConfigInfo, break loop */
               break;
            } else if (addRC > 0) {
               /* update count only if added */
               nscount6++;
            }
            /* Else: IPv6.sin6_family != AF_INET6 or other issues already
             *       logged in AddResolverNSInfo
             */
         }
         /* Else: null entry indicates no IPv6 at that position */
#   endif

         if (resp->nsaddr_list[i].sin_family == AF_INET) {
            /* Has IPv4 nameserver at index */
            addRC = AddResolverNSInfo(
                       dnsConfigInfo,
                       (struct sockaddr *)(&resp->nsaddr_list[i]));
            if (addRC == 0){
               /* Can't add more DNS entries to dnsConfigInfo, break loop */
               break;
            } else if (addRC > 0) {
               /* update count only if added */
               nscount4++;
            }
            /* Else: issue logged in AddResolverNSInfo */
         }
         /* Else: IPv4.sin_family == 0, it means a 'free' slot. */
      } /* for nscount and one of the total count is < nscount */

      if ((nscount4 + nscount6) < resp->nscount) {
         /*
          * Not all expected nameserver entries were collected
          */
         g_warning(
            "%s: dns update canceled, index=%d, ns=%d, IPv4=%d, IPv6=%d",
            __FUNCTION__,
            i, resp->nscount, nscount4, nscount6);
      }
   }
#endif                                  // if !defined RESOLVER_IPV6_GETSERVERS
}

/*
 ******************************************************************************
 * AddResolverNSInfo --                                                  */ /**
 *
 * @brief Add a nameserver IP address from resolver(3) subsystem to the
 *        DnsConfigInfo container.
 *
 * @param[out] dnsConfigInfo  Destination DnsConfigInfo container.
 * @param[in]  sap            The sockaddr structure containing the IP address
 *
 * @retval -1    IP address not added due to wrong or invalid parameter.
 * @retval  0    IP address not added due to limit reached @a dnsConfigInfo.
 * @retval  1    IP address added to @a dnsConfigInfo or otherwise.
 *
 ******************************************************************************
 */
static int
AddResolverNSInfo(DnsConfigInfo *dnsConfigInfo, struct sockaddr *sap)
{
   TypedIpAddress *ip;

   ASSERT(sap != NULL);
   ASSERT(dnsConfigInfo != NULL);

   if (sap->sa_family != AF_INET
       && sap->sa_family != AF_INET6) {
      /* Not a supported address family */
      g_debug("%s: unhandled address family (%d)", __FUNCTION__, sap->sa_family);
      return -1;
   }

   /* Check to see if we're going above our limit. See bug 605821. */
   if (dnsConfigInfo->serverList.serverList_len == DNSINFO_MAX_SERVERS) {
      g_message("%s: dns server limit (%d) reached, skipping overflow.",
         __FUNCTION__, DNSINFO_MAX_SERVERS);
      return 0;
   }

   ip = XDRUTIL_ARRAYAPPEND(dnsConfigInfo, serverList, 1);
   ASSERT_MEM_ALLOC(ip);
   GuestInfoSockaddrToTypedIpAddress(sap, ip);
   return 1;
}

#   ifndef RESOLVER_IPV6_GETSERVERS
/*
 ******************************************************************************
 * PrintResolverNSInfo --                                                */ /**
 *
 * @brief log the resolver(3) subsystem state about nameservers
 *
 * @param[in] resp    res_state, pointer to a resolver(3) state structure.
 *
 ******************************************************************************
 */
static void
PrintResolverNSInfo(res_state resp)
{
   /*
    * Init IPv6 state for unsupported case
    * Keep the message short < INET_ADDRSTRLEN for both v4 & v6
    */
   char ipv6Buff[INET6_ADDRSTRLEN+1] = "no-support";
   char ipv4Buff[INET_ADDRSTRLEN+1];
   int nscount = 0;
   int nscount6 = 0;
   int nscount4 = 0;
   int i;

   /* Split the counts per GLIBC 2.11+ source */
   nscount = resp->nscount;
#   ifdef RESOLVER_IPV6_EXT
   nscount6 = resp->_u._ext.nscount6;
#   endif
   nscount4 = nscount - nscount6;

   g_debug(
      "Resolver State: id=%d Count=%d, IPv4=%d, IPv6=%d",
       resp->id, nscount, nscount4, nscount6);

   for (i = 0; i < MAXNS; i++) {
#   ifdef RESOLVER_IPV6_EXT
      /*
       * Set IPv6 entry status
       */
      if (resp->_u._ext.nsaddrs[i] != NULL) {
         if (resp->_u._ext.nsaddrs[i]->sin6_family == AF_INET6) {
            const char *cvRes = inet_ntop(AF_INET6,
                             &resp->_u._ext.nsaddrs[i]->sin6_addr,
                             ipv6Buff, INET6_ADDRSTRLEN);
            if (cvRes == NULL) {
               /* Failed to output IPv6 address to buffer */
               int errsv = errno;
               /* Keep the message short < INET_ADDRSTRLEN for both v4 & v6 */
               Str_Sprintf(ipv6Buff, sizeof ipv6Buff, "ntop-fail:%d", errsv);
            }
         } else {
            /* Keep the message short < INET_ADDRSTRLEN for both v4 & v6 */
            Str_Sprintf(ipv6Buff, sizeof ipv6Buff, "AF:%02d",
              resp->_u._ext.nsaddrs[i]->sin6_family);
         }
      } else {
         /* Keep the message short < INET_ADDRSTRLEN for both v4 & v6 */
         Str_Sprintf(ipv6Buff, sizeof ipv6Buff, "no-entry");
      }
#   endif

      /*
       * Set IPv4 entry status
       */
      if (resp->nsaddr_list[i].sin_family == AF_INET) {
         const char *cvRes = inet_ntop(AF_INET,
                          &resp->nsaddr_list[i].sin_addr,
                          ipv4Buff, INET_ADDRSTRLEN);
         if (cvRes == NULL) {
            /* Failed to output IPv4 address to buffer */
            int errsv = errno;
            /* Keep the message short < INET_ADDRSTRLEN for both v4 & v6 */
            Str_Sprintf(ipv4Buff, sizeof ipv4Buff, "ntop-fail:%d", errsv);
         }
      } else {
         /* Keep the message short < INET_ADDRSTRLEN for both v4 & v6 */
         Str_Sprintf(ipv4Buff, sizeof ipv4Buff, "AF:%02d",
           resp->nsaddr_list[i].sin_family);
      }

      g_debug(
         "Resolver State: id=%d Lists[%d] - IPv4(%s) - IPv6(%s)",
         resp->id, i, ipv4Buff, ipv6Buff);
   }
}
#   endif // ifndef RESOLVER_IPV6_GETSERVERS
#endif // USE_RESOLVE


#ifdef USE_SLASH_PROC
/*
 ******************************************************************************
 * RecordRoutingInfoIPv4 --                                              */ /**
 *
 * @brief Query the IPv4 routing subsystem and pack up contents
 * (struct rtentry) into InetCidrRouteEntries.
 *
 * @param[in]  maxRoutes   Max routes to gather.
 * @param[out] nicInfo     NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfoIPv4(unsigned int maxRoutes,
                      NicInfoV3 *nicInfo)
{
   GPtrArray *routes = NULL;
   guint i;
   Bool ret = FALSE;

   ASSERT(maxRoutes > 0);

   if ((routes = SlashProcNet_GetRoute(maxRoutes, RTF_UP)) == NULL) {
      return FALSE;
   }

   for (i = 0; i < routes->len; i++) {
      struct rtentry *rtentry;
      struct sockaddr_in *sin_dst;
      struct sockaddr_in *sin_gateway;
      struct sockaddr_in *sin_genmask;
      InetCidrRouteEntry *icre;
      uint32_t ifIndex;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (nicInfo->routes.routes_len == NICINFO_MAX_ROUTES) {
         g_message("%s: route limit (%d) reached, skipping overflow.",
                   __FUNCTION__, NICINFO_MAX_ROUTES);
         break;
      }

      rtentry = g_ptr_array_index(routes, i);

      if (!GuestInfoGetNicInfoIfIndex(nicInfo,
                                      if_nametoindex(rtentry->rt_dev),
                                      &ifIndex)) {
         continue;
      }

      icre = XDRUTIL_ARRAYAPPEND(nicInfo, routes, 1);
      ASSERT_MEM_ALLOC(icre);

      sin_dst = (struct sockaddr_in *)&rtentry->rt_dst;
      sin_gateway = (struct sockaddr_in *)&rtentry->rt_gateway;
      sin_genmask = (struct sockaddr_in *)&rtentry->rt_genmask;

      GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin_dst,
                                        &icre->inetCidrRouteDest);

      icre->inetCidrRoutePfxLen = CountNetmaskBitsV4((struct sockaddr *)sin_genmask);

      /*
       * Gateways are optional (ex: one can bind a route to an interface w/o
       * specifying a next hop address).
       */
      if (rtentry->rt_flags & RTF_GATEWAY) {
         TypedIpAddress *ip = Util_SafeCalloc(1, sizeof *ip);
         GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin_gateway, ip);
         icre->inetCidrRouteNextHop = ip;
      }

      /*
       * Interface, metric.
       */
      icre->inetCidrRouteIfIndex = ifIndex;
      icre->inetCidrRouteMetric = rtentry->rt_metric;
   }

   ret = TRUE;

   SlashProcNet_FreeRoute(routes);
   return ret;
}


/*
 ******************************************************************************
 * RecordRoutingInfoIPv6 --                                              */ /**
 *
 * @brief Query the IPv6 routing subsystem and pack up contents
 * (struct in6_rtmsg) into InetCidrRouteEntries.
 *
 * @param[in]  maxRoutes   Max routes to gather.
 * @param[out] nicInfo     NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected, attached to @a nicInfo.
 * @retval FALSE        Something went wrong.  @a nicInfo is unharmed.
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfoIPv6(unsigned int maxRoutes,
                      NicInfoV3 *nicInfo)
{
   GPtrArray *routes = NULL;
   guint i;
   Bool ret = FALSE;

   ASSERT(maxRoutes > 0);

   /*
    * Reading large number of ipv6 routes in pathToNetRoute6 could
    * result in performance issue because:
    *  1. IPv6 route table is not efficient natively compared to ipv4
    *     because of its implementation.
    *  2. The glib I/O channel can aggravate the performace.
    * Considering bug 605821/2064541, we try to read the first maxRoutes
    * lines of pathToNetRoute6 with route flag RTF_UP set.
    */
   if ((routes = SlashProcNet_GetRoute6(maxRoutes, RTF_UP)) == NULL) {
      return FALSE;
   }

   for (i = 0; i < routes->len; i++) {
      struct sockaddr_storage ss;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
      struct in6_rtmsg *in6_rtmsg;
      InetCidrRouteEntry *icre;
      uint32_t ifIndex = -1;

      /* Check to see if we're going above our limit. See bug 605821. */
      if (nicInfo->routes.routes_len == NICINFO_MAX_ROUTES) {
         g_message("%s: route limit (%d) reached, skipping overflow.",
                   __FUNCTION__, NICINFO_MAX_ROUTES);
         break;
      }

      in6_rtmsg = g_ptr_array_index(routes, i);

      if (!GuestInfoGetNicInfoIfIndex(nicInfo, in6_rtmsg->rtmsg_ifindex,
                                      &ifIndex)) {
         continue;
      }

      icre = XDRUTIL_ARRAYAPPEND(nicInfo, routes, 1);
      ASSERT_MEM_ALLOC(icre);

      /*
       * Destination.
       */
      sin6->sin6_family = AF_INET6;
      sin6->sin6_addr = in6_rtmsg->rtmsg_dst;
      GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin6,
                                        &icre->inetCidrRouteDest);

      icre->inetCidrRoutePfxLen = in6_rtmsg->rtmsg_dst_len;

      /*
       * Next hop.
       */
      if (in6_rtmsg->rtmsg_flags & RTF_GATEWAY) {
         TypedIpAddress *ip = Util_SafeCalloc(1, sizeof *ip);
         sin6->sin6_addr = in6_rtmsg->rtmsg_gateway;
         GuestInfoSockaddrToTypedIpAddress((struct sockaddr *)sin6, ip);
         icre->inetCidrRouteNextHop = ip;
      }

      /*
       * Interface, metric.
       */
      icre->inetCidrRouteIfIndex = ifIndex;
      icre->inetCidrRouteMetric = in6_rtmsg->rtmsg_metric;
   }

   ret = TRUE;

   SlashProcNet_FreeRoute6(routes);
   return ret;
}


/*
 ******************************************************************************
 * RecordRoutingInfo --                                                  */ /**
 *
 * @brief Query the routing subsystem and pack up contents into
 * InetCidrRouteEntries when either of IPv4 or IPV6 is configured.
 *
 * @param[in]  maxIPv4Routes  Max IPv4 routes to gather.
                              Set 0 to disable gathering.
 * @param[in]  maxIPv6Routes  Max IPv6 routes to gather.
                              Set 0 to disable gathering.
 * @param[out] nicInfo        NicInfoV3 container.
 *
 * @note Do not call this routine without first populating @a nicInfo 's NIC
 * list.
 *
 * @retval TRUE         Values collected(either IPv4 or IPv6 or both),
 *                      attached to @a nicInfo.
 * @retval FALSE        Something went wrong(neither IPv4 nor IPv6 configured).
 *
 ******************************************************************************
 */

static Bool
RecordRoutingInfo(unsigned int maxIPv4Routes,
                  unsigned int maxIPv6Routes,
                  NicInfoV3 *nicInfo)
{
   Bool retIPv4 = FALSE;
   Bool retIPv6 = FALSE;

   ASSERT(maxIPv4Routes > 0 || maxIPv6Routes > 0);

   /*
    * We gather IPv4 routes first, then IPv6. This means IPv4 routes are more
    * prioritized than IPv6. When there's more than NICINFO_MAX_ROUTES IPv4
    * routes in system, the IPv6 routes will be ignored. A more equitable
    * design might be getting max IPv4 and IPv6 routes first, and then pick
    * out the head NICINFO_MAX_ROUTES/2 of each route list.
    */
   if (maxIPv4Routes > 0) {
      if (RecordRoutingInfoIPv4(maxIPv4Routes, nicInfo)) {
         retIPv4 = TRUE;
      } else {
         g_warning("%s: Unable to collect IPv4 routing table.\n", __func__);
      }
   }

   if (maxIPv6Routes > 0 && nicInfo->routes.routes_len < NICINFO_MAX_ROUTES) {
      if (RecordRoutingInfoIPv6(maxIPv6Routes, nicInfo)) {
         retIPv6 = TRUE;
      } else {
         g_warning("%s: Unable to collect IPv6 routing table.\n", __func__);
      }
   }

   return (retIPv4 || retIPv6);
}

#else                                           // ifdef USE_SLASH_PROC
static Bool
RecordRoutingInfo(unsigned int maxIPv4Routes,
                  unsigned int maxIPv6Routes,
                  NicInfoV3 *nicInfo)
{
   return TRUE;
}
#endif                                          // else

#ifndef NO_DNET

#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined(USERWORLD)
/*
 ******************************************************************************
 * GuestInfoGetIntf --                                                   */ /**
 *
 * @brief Callback function called by libdnet when iterating over all the NICs
 * on the host.
 *
 * @param[in]      intf_entry  sockaddr struct to convert.
 * @param[in/out]  arg         GuestInfoIpPriority struct
 *
 * arg points to an GuestInfoIpPriority structure containing the priority we
 * are looking for, and a pointer to the ip address to be set.
 *
 * @retval -1              If applicable address found, returns string of said
 *                         IP address in buffer.
 * @retval 0               If applicable address not found.
 *
 ******************************************************************************
 */

static int
GuestInfoGetIntf(const struct intf_entry *entry, // IN
                 void *arg)                      // IN/OUT
{
   GuestInfoIpPriority *ipp = arg;
   char **ipstr = &ipp->ipstr;

   if (entry->intf_type == INTF_TYPE_ETH &&
       entry->intf_link_addr.addr_type == ADDR_TYPE_ETH) {
      struct sockaddr_storage ss;
      struct sockaddr *saddr = (struct sockaddr *)&ss;

      if (GuestInfo_IfaceGetPriority(entry->intf_name) != ipp->priority ||
          GuestInfo_IfaceIsExcluded(entry->intf_name)) {
         return 0;
      }

      memset(&ss, 0, sizeof ss);
      addr_ntos(&entry->intf_addr, saddr);
      *ipstr = ValidateConvertAddress(saddr);
      if (*ipstr != NULL) {
         /* We need to return an error to exit out of the loop. */
         return -1;
      }
   }

   return 0;
}
#endif

#endif // ifndef NO_DNET

/*
 ******************************************************************************
 * ValidateConvertAddress --                                             */ /**
 *
 * @brief Helper routine validates an address as a return value for
 * GuestInfoGetPrimaryIP.
 *
 * @param[in]  addr  sockaddr struct to convert.
 *
 * @return  If applicable address found, returns string of said IP address.
 *          If an error occurred, returns NULL.
 *
 ******************************************************************************
 */

static char *
ValidateConvertAddress(const struct sockaddr *addr)  // IN:
{
   char ipstr[INET6_ADDRSTRLEN];

   if (addr->sa_family == AF_INET) {
      struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;

      if (addr4->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ||
          addr4->sin_addr.s_addr == htonl(INADDR_ANY) ||
          !inet_ntop(addr->sa_family, &addr4->sin_addr, ipstr, sizeof ipstr)) {
         return NULL;
      }
   } else if (addr->sa_family == AF_INET6) {
      struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

      if (IN6_IS_ADDR_LOOPBACK(&addr6->sin6_addr) ||
          IN6_IS_ADDR_LINKLOCAL(&addr6->sin6_addr) ||
          IN6_IS_ADDR_SITELOCAL(&addr6->sin6_addr) ||
          IN6_IS_ADDR_UNIQUELOCAL(&addr6->sin6_addr) ||
          IN6_IS_ADDR_UNSPECIFIED(&addr6->sin6_addr) ||
          !inet_ntop(addr->sa_family, &addr6->sin6_addr, ipstr,
                     sizeof ipstr)) {
         return NULL;
      }
   } else {
      return NULL;
   }

   return Util_SafeStrdup(ipstr);
}

/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file guestInfo.c
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
#include "guestInfoInt.h"
#include "str.h"
#include "util.h"
#include "wiper.h"
#include "xdrutil.h"
#include "netutil.h"


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


/*
 * Global functions.
 */


/*
 ******************************************************************************
 * GuestInfo_GetAvailableDiskSpace --                                    */ /**
 *
 * @brief Given a mount point, return the amount of free space on that volume.
 *
 * Get the amount of disk space available on the volume the FCP (file copy/
 * paste) staging area is in. DnD and FCP use same staging area in guest.
 * But it is only called in host->guest FCP case. DnD checks guest available
 * disk space in host side (UI).
 *
 * @param[in]  pathName Mount point to examine.
 *
 * @todo This doesn't belong in lib/guestInfo.
 *
 * @return Bytes free on success, 0 on failure.
 *
 ******************************************************************************
 */

uint64
GuestInfo_GetAvailableDiskSpace(char *pathName)
{
   WiperPartition p;
   uint64 freeBytes  = 0;
   uint64 totalBytes = 0;
   char *wiperError;

   if (strlen(pathName) > sizeof p.mountPoint) {
      Debug("GetAvailableDiskSpace: gFileRoot path too long\n");
      return 0;
   }
   Str_Strcpy((char *)p.mountPoint, pathName, sizeof p.mountPoint);
   wiperError = (char *)WiperSinglePartition_GetSpace(&p, &freeBytes, &totalBytes);
   if (strlen(wiperError) > 0) {
      Debug("GetAvailableDiskSpace: error using wiper lib: %s\n", wiperError);
      return 0;
   }
   Debug("GetAvailableDiskSpace: free bytes is %"FMT64"u\n", freeBytes);
   return freeBytes;
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
GuestInfo_GetFqdn(int outBufLen,        // IN
                  char fqdn[])          // OUT
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
GuestInfo_GetNicInfo(NicInfoV3 **nicInfo)  // OUT
{
   Bool retval = FALSE;

   *nicInfo = Util_SafeCalloc(1, sizeof (struct NicInfoV3));

   retval = GuestInfoGetNicInfo(*nicInfo);
   if (!retval) {
      free(*nicInfo);
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
GuestInfo_FreeNicInfo(NicInfoV3 *nicInfo)       // IN
{
   if (nicInfo != NULL) {
      VMX_XDR_FREE(xdr_NicInfoV3, nicInfo);
      free(nicInfo);
   }
}


/*
 ******************************************************************************
 * GuestInfo_GetDiskInfo --                                              */ /**
 *
 * @brief Get disk information.
 *
 * @param[in,out] di    DiskInfo container.
 *
 * @note 
 * Allocates memory for di->partitionList.
 *
 * @retval TRUE  Success.
 * @retval FALSE Failure.
 *
 ******************************************************************************
 */

Bool
GuestInfo_GetDiskInfo(PGuestDiskInfo di)     // IN/OUT
{
   WiperPartition_List pl;
   DblLnkLst_Links *curr;
   unsigned int partCount = 0;
   uint64 freeBytes = 0;
   uint64 totalBytes = 0;
   unsigned int partNameSize = 0;
   Bool success = FALSE;

   ASSERT(di);
   partNameSize = sizeof (di->partitionList)[0].name;
   di->numEntries = 0;
   di->partitionList = NULL;

   /* Get partition list. */
   if (!WiperPartition_Open(&pl)) {
      Debug("GetDiskInfo: ERROR: could not get partition list\n");
      return FALSE;
   }

   DblLnkLst_ForEach(curr, &pl.link) {
      WiperPartition *part = DblLnkLst_Container(curr, WiperPartition, link);

      if (part->type != PARTITION_UNSUPPORTED) {
         PPartitionEntry newPartitionList;
         PPartitionEntry partEntry;
         unsigned char *error;

         error = WiperSinglePartition_GetSpace(part, &freeBytes, &totalBytes);
         if (strlen(error)) {
            Debug("GetDiskInfo: ERROR: could not get space for partition %s: %s\n",
                  part->mountPoint, error);
            goto out;
         }

         if (strlen(part->mountPoint) + 1 > partNameSize) {
            Debug("GetDiskInfo: ERROR: Partition name buffer too small\n");
            goto out;
         }

         newPartitionList = realloc(di->partitionList,
                                    (partCount + 1) * sizeof *di->partitionList);
         if (newPartitionList == NULL) {
            Debug("GetDiskInfo: ERROR: could not allocate partition list.\n");
            goto out;
         }

         partEntry = &newPartitionList[partCount++];
         Str_Strcpy(partEntry->name, part->mountPoint, partNameSize);
         partEntry->freeBytes = freeBytes;
         partEntry->totalBytes = totalBytes;

         di->partitionList = newPartitionList;
      }
   }

   di->numEntries = partCount;
   success = TRUE;

out:
   WiperPartition_Close(&pl);
   return success;
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
 * @return Pointer to the new NIC.
 *
 ******************************************************************************
 */

GuestNicV3 *
GuestInfoAddNicEntry(NicInfoV3 *nicInfo,                       // IN/OUT
                     const char macAddress[NICINFO_MAC_LEN],   // IN
                     DnsConfigInfo *dnsInfo,                   // IN
                     WinsConfigInfo *winsInfo)                 // IN
{
   GuestNicV3 *newNic;

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
GuestInfoAddIpAddress(GuestNicV3 *nic,                  // IN/OUT
                      const struct sockaddr *sockAddr,  // IN
                      InetAddressPrefixLength pfxLen,   // IN
                      const IpAddressOrigin *origin,    // IN
                      const IpAddressStatus *status)    // IN
{
   IpAddressEntry *ip;

   ASSERT(sockAddr);

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
GuestInfoSockaddrToTypedIpAddress(const struct sockaddr *sa,    // IN
                                  TypedIpAddress *typedIp)      // OUT
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
      break;
   default:
      NOT_REACHED();
   }
}


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
GuestInfoGetNicInfoIfIndex(NicInfoV3 *nicInfo,  // IN
                           int ifIndex,         // IN
                           int *nicIfIndex)     // OUT
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

void *
Util_DupeThis(const void *source,
              size_t sourceSize)
{
   void *dest;

   ASSERT(source);

   dest = Util_SafeMalloc(sourceSize);
   memcpy(dest, source, sourceSize);

   return dest;
}

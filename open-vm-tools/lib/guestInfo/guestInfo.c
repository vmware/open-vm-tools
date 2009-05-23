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

/*
 * guestInfo.c ---
 *
 *	Provides interface to information about the guest, such as hostname,
 *	NIC/IP address information, etc.
 */

#include <stdlib.h>
#include <string.h>

#include "vm_assert.h"
#include "debug.h"
#include "guestInfoInt.h"
#include "str.h"
#include "wiper.h"
#include "xdrutil.h"


/*
 * Global functions
 */

 
/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfo_GetAvailableDiskSpace --
 *
 *    Get the amount of disk space available on the volume the FCP (file copy/
 *    paste) staging area is in. DnD and FCP use same staging area in guest.
 *    But it is only called in host->guest FCP case. DnD checks guest available
 *    disk space in host side (UI).
 *
 * Results:
 *    Available disk space size if succeed, otherwise 0.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
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
 *-----------------------------------------------------------------------------
 *
 * GuestInfo_GetFqdn --
 *
 *      Returns the guest's hostname.
 *
 * Results:
 *      Returns TRUE on success, FALSE on failure.
 *      Returns the guest's fully qualified domain name in fqdn.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfo_GetFqdn(int outBufLen,        // IN: sizeof fqdn
                  char fqdn[])          // OUT: buffer to store hostname
{
   return GuestInfoGetFqdn(outBufLen, fqdn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfo_GetNicInfo --
 *
 *      Returns the guest's hostname.
 *
 * Results:
 *      Return MAC addresses of all the NICs in the guest and their
 *      corresponding IP addresses.
 *
 *      Returns TRUE on success and FALSE on failure.
 *      Return MAC addresses of all NICs and their corresponding IPs.
 *
 * Side effects:
 *      Memory is allocated for each NIC, as well as IP addresses of all NICs
 *      on successful return.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfo_GetNicInfo(GuestNicList *nicInfo)  // OUT: storage for NIC information
{
   return GuestInfoGetNicInfo(nicInfo);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_GetDiskInfo --
 *
 *      Get disk information.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *	Allocates memory for di->partitionList.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestInfo_GetDiskInfo(PGuestDiskInfo di)     // IN/OUT
{
   return GuestInfoGetDiskInfo(di);
}


/**
 * Add a NIC into the given list. The macAddress of the new GuestNic is
 * initialized with the given address.
 *
 * @param[in,out] nicInfo     List of NICs.
 * @param[in]     macAddress  MAC address of new NIC.
 *
 * @return The new NIC, or NULL on failure.
 */

GuestNic *
GuestInfoAddNicEntry(GuestNicList *nicInfo,                    // IN/OUT
                     const char macAddress[NICINFO_MAC_LEN])   // IN
{
   GuestNic *newNic;

   newNic = XDRUTIL_ARRAYAPPEND(nicInfo, nics, 1);
   if (newNic != NULL) {
      Str_Strcpy(newNic->macAddress, macAddress, sizeof newNic->macAddress);
   }

   return newNic;
}


/**
 * Add an IP address entry into the GuestNic.
 *
 * @param[in,out] nic      The NIC information.
 * @param[in]     ipAddr   The new IP address to add.
 * @param[in]     af_type  Interface type.
 *
 * @return Newly allocated IP address struct, NULL on failure.
 */

VmIpAddress *
GuestInfoAddIpAddress(GuestNic *nic,                    // IN/OUT
                      const char *ipAddr,               // IN
                      const uint32 af_type)             // IN
{
   VmIpAddress *ip;

   ip = XDRUTIL_ARRAYAPPEND(nic, ips, 1);
   if (ip != NULL) {
      Str_Strcpy(ip->ipAddress, ipAddr, sizeof ip->ipAddress);
      ip->addressFamily = af_type;
   }

   return ip;
}


/**
 * Add an IPv4 netmask / IPv6 prefix length to the IpAddress in ASCII form.
 *
 * If convertToMask is true the 'n' bits subnet mask is converted
 * to an ASCII string as a hexadecimal number (0xffffff00) and
 * added to the IPAddressEntry.  (Applies to IPv4 only.)
 *
 * If convertToMask is false the value is added to the IPAddressEntry in
 * string form - ie '24'.
 *
 * @param[in,out] ipAddressEntry    The IP address info.
 * @param[in]     subnetMaskBits    The mask.
 * @param[in]     convertToMask     See above.
 */

void
GuestInfoAddSubnetMask(VmIpAddress *ipAddressEntry,            // IN/OUT
                       const uint32 subnetMaskBits,            // IN
                       Bool convertToMask)                     // IN
{
   int i;
   uint32 subnetMask = 0;

   ASSERT(ipAddressEntry);
   /*
    * It's an error to set convertToMask on an IPv6 address.
    */
   ASSERT(ipAddressEntry->addressFamily == INFO_IP_ADDRESS_FAMILY_IPV4 ||
          !convertToMask);

   if (convertToMask && (subnetMaskBits <= 32)) {
      /*
       * Convert the subnet mask from a number of bits (ie. '24') to
       * hexadecimal notation such 0xffffff00
       */
      for (i = 0; i < subnetMaskBits; i++) {
         subnetMask |= (0x80000000 >> i);
      }

      // Convert the hexadecimal value to a string and add to the IpAddress Entry
      Str_Sprintf(ipAddressEntry->subnetMask,
                  sizeof ipAddressEntry->subnetMask,
                  "0x%x", subnetMask);
   } else {
      Str_Sprintf(ipAddressEntry->subnetMask,
                  sizeof ipAddressEntry->subnetMask,
                  "%d", subnetMaskBits);
   }
   return;
}


/**
 * Get disk information.
 *
 * @param[out]    di    Where to store the disk information.
 *
 * @return TRUE if successful, FALSE otherwise.
 */

Bool
GuestInfoGetDiskInfo(PGuestDiskInfo di)
{
   int i = 0;
   WiperPartition_List *pl = NULL;
   unsigned int partCount = 0;
   uint64 freeBytes = 0;
   uint64 totalBytes = 0;
   WiperPartition nextPartition;
   unsigned int partNameSize = 0;
   Bool success = FALSE;

   ASSERT(di);
   partNameSize = sizeof (di->partitionList)[0].name;
   di->numEntries = 0;
   di->partitionList = NULL;

   /* Get partition list. */
   pl = WiperPartition_Open();
   if (pl == NULL) {
      Debug("GetDiskInfo: ERROR: could not get partition list\n");
      return FALSE;
   }

   for (i = 0; i < pl->size; i++) {
      nextPartition = pl->partitions[i];
      if (!strlen(nextPartition.comment) ||
          strcmp(nextPartition.comment, WIPER_DEVICE_MAPPER_STRING) == 0) {
         PPartitionEntry newPartitionList;
         unsigned char *error;
         error = WiperSinglePartition_GetSpace(&nextPartition, &freeBytes, &totalBytes);
         if (strlen(error)) {
            Debug("GetDiskInfo: ERROR: could not get space for partition %s: %s\n",
                  nextPartition.mountPoint, error);
            goto out;
         }

         if (strlen(nextPartition.mountPoint) + 1 > partNameSize) {
            Debug("GetDiskInfo: ERROR: Partition name buffer too small\n");
            goto out;
         }

         newPartitionList = realloc(di->partitionList,
                                    (partCount + 1) * sizeof *di->partitionList);
         if (newPartitionList == NULL) {
            Debug("GetDiskInfo: ERROR: could not allocate partition list.\n");
            goto out;
         }
         di->partitionList = newPartitionList;

         Str_Strcpy((di->partitionList)[partCount].name, nextPartition.mountPoint,
                        partNameSize);
         (di->partitionList)[partCount].freeBytes = freeBytes;
         (di->partitionList)[partCount].totalBytes = totalBytes;
         partCount++;
      }
   }

   di->numEntries = partCount;
   success = TRUE;
out:
   WiperPartition_Close(pl);
   return success;
}


/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * guestInfoPosix.c --
 *
 *    Routines to get guest information. These are invoked by guestInfoServer
 *    which writes this information into Vmdb.
 *
 */

#ifndef VMX86_DEVEL

#endif

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
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
#endif
#ifndef NO_DNET
# ifdef DNET_IS_DUMBNET
#  include <dumbnet.h>
# else
#  include <dnet.h>
# endif
#endif
#include "util.h"
#include "sys/utsname.h"
#include "sys/ioctl.h"
#include "vmware.h"
#include "hostinfo.h"
#include "guestInfoInt.h"
#include "debug.h"
#include "str.h"
#include "guest_os.h"
#include "guestApp.h"
#include "guestInfo.h"


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoGetFqdn--
 *
 *      Return the guest's fully qualified domain name.
 *      This is just a thin wrapper around gethostname.
 *
 * Return value:
 *      Returns TRUE on success and FALSE on failure.
 *      Returns the guest's fully qualified domain name.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfoGetFqdn(int outBufLen,    // IN: length of output buffer
                 char fqdn[])      // OUT: fully qualified domain name
{
   ASSERT(fqdn);
   if (gethostname(fqdn, outBufLen) < 0) {
      Debug("Error, gethostname failed\n");
      return FALSE;
   }

   return TRUE;
}


#ifndef NO_DNET
/*
 *-----------------------------------------------------------------------------
 *
 * RecordNetworkAddress --
 *
 *      Massages a dnet(3)-style interface address (IPv4 or IPv6) and stores it
 *      as part of a GuestNic structure.
 *
 * Results:
 *      If addr is IPv4 or IPv6, it will be appended to the GuestNic's list of
 *      IP addresses.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RecordNetworkAddress(GuestNic *nic,             // IN: operand NIC
                     const struct addr *addr)   // IN: dnet(3) address to process
{
   char ipAddress[NICINFO_MAX_IP_LEN];
   VmIpAddress *ip = NULL;

   switch (addr->addr_type) {
   case ADDR_TYPE_IP:
      /*
       * GuestNicInfo clients expect IPv4 addresses and netmasks to be stored
       * as strings in separate fields.  As such, we'll use ip_ntop instead of
       * addr_ntop to get a string without the netmask bits.
       */
      ip_ntop(&addr->addr_ip, ipAddress, sizeof ipAddress);
      ip = GuestInfoAddIpAddress(nic, ipAddress, INFO_IP_ADDRESS_FAMILY_IPV4);
      if (ip) {
         GuestInfoAddSubnetMask(ip, addr->addr_bits, TRUE);
      }
      break;
   case ADDR_TYPE_IP6:
      ip6_ntop(&addr->addr_ip6, ipAddress, sizeof ipAddress);
      ip = GuestInfoAddIpAddress(nic, ipAddress, INFO_IP_ADDRESS_FAMILY_IPV6);
      if (ip) {
         GuestInfoAddSubnetMask(ip, addr->addr_bits, FALSE);
      }
      break;
   default:
      Debug("%s: Unknown address type: %hu\n", __func__, addr->addr_type);
      break;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ReadInterfaceDetails --
 *
 *      Callback function called by libdnet when iterating over all the
 *      NICs on the host.
 *
 * Return value:
 *      Returns 0 on success and -1 on failure.
 *      Adds the MAC addresses of all NICs and their corresponding IPs.
 *
 * Side effects:
 *      Memory is allocated for each NIC, as well as IP addresses of all NICs
 *      on successful return.
 *
 *-----------------------------------------------------------------------------
 */

int
ReadInterfaceDetails(const struct intf_entry *entry,  // IN: current interface entry
                     void *arg)                       // IN: Pointer to the GuestNicList
{
   int i;
   GuestNicList *nicInfo = arg;

   if ((entry->intf_type & INTF_TYPE_ETH) == INTF_TYPE_ETH) {
      GuestNic *nic;
      char macAddress[NICINFO_MAC_LEN];

      Str_Sprintf(macAddress, sizeof macAddress, "%s",
                  addr_ntoa(&entry->intf_link_addr));
      nic = GuestInfoAddNicEntry(nicInfo, macAddress);

      if (nic == NULL) {
         return -1;
      }

      /* Record the "primary" address. */
      if (entry->intf_addr.addr_type == ADDR_TYPE_IP ||
          entry->intf_addr.addr_type == ADDR_TYPE_IP6) {
         RecordNetworkAddress(nic, &entry->intf_addr);
      }

      /* Walk the list of alias's and add those that are IPV4 or IPV6 */
      for (i = 0; i < entry->intf_alias_num; i++) {
         const struct addr *alias = &entry->intf_alias_addrs[i];
         if (alias->addr_type == ADDR_TYPE_IP ||
             alias->addr_type == ADDR_TYPE_IP6) {
            RecordNetworkAddress(nic, alias);
         }
      }
   }

   return 0;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoGetNicInfo --
 *
 *      Return MAC addresses of all the NICs in the guest and their
 *      corresponding IP addresses.
 *
 * Return value:
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
GuestInfoGetNicInfo(GuestNicList *nicInfo)   // OUT
{
#ifndef NO_DNET
   intf_t *intf;

   memset(nicInfo, 0, sizeof *nicInfo);

   /* Get a handle to read the network interface configuration details. */
   if ((intf = intf_open()) == NULL) {
      Debug("GuestInfo: Error, failed NULL result from intf_open()\n");
      return FALSE;
   }

   if (intf_loop(intf, ReadInterfaceDetails, nicInfo) < 0) {
      intf_close(intf);
      Debug("GuestInfo: Error, negative result from intf_loop\n");
      return FALSE;
   }

   intf_close(intf);
   return TRUE;
#else
   return FALSE;
#endif
}

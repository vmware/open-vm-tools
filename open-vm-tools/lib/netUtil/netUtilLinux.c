/*********************************************************
 * Copyright (C) 1998-2017, 2021 VMware, Inc. All rights reserved.
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
 * netUtilLinux.c --
 *
 *    Network routines for all guest applications.
 *
 *    Linux implementation
 *
 */

#ifndef VMX86_DEVEL

#endif


#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
#   error This file should not be compiled
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netdb.h>
#ifdef sun
# include <sys/sockio.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
/* <netinet/in.h> must precede <arpa/in.h> for FreeBSD to compile. */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>         // for ARPHRD_ETHER

#if defined(__FreeBSD__) || defined(__APPLE__)
#include "ifaddrs.h"
#endif

#include "vm_assert.h"
#include "netutil.h"
#include "debug.h"
#include "guestApp.h"
#include "util.h"
#include "str.h"

#define MAX_IFACES      64
#define LOOPBACK        "lo"    // XXX: We would have a problem with something like "loa0".
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ValidateConvertAddress --
 *
 *      Helper routine validates an address as a return value for
 *      NetUtil_GetPrimaryIP.
 *
 * Results:
 *      Returns TRUE with sufficient result stored in outputBuffer on success.
 *      Returns FALSE with "" stored in outputBuffer on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ValidateConvertAddress(const char *ifaceName,           // IN: interface name
                       const struct sockaddr_in *addr,  // IN: network address to
                                                        //     evaluate
                       char ipstr[INET_ADDRSTRLEN])     // OUT: converted address
                                                        //      stored here
{
   /*
    * 1.  Ensure this isn't a loopback device.
    * 2.  Ensure this is an (IPv4) internet address.
    */
   if (ifaceName[0] == '\0' ||
       strncmp(ifaceName, LOOPBACK, sizeof LOOPBACK - 1) == 0 ||
       addr->sin_family != AF_INET) {
      goto invalid;
   }

   /*
    * Branches separated because it just looked really silly to lump the
    * initial argument checking and actual conversion logic together.
    */

   /*
    * 3.  Attempt network to presentation conversion.
    * 4.  Ensure the IP isn't all zeros.
    */
   if (inet_ntop(AF_INET, (void *)&addr->sin_addr, ipstr, INET_ADDRSTRLEN) != NULL &&
       strcmp(ipstr, "0.0.0.0") != 0) {
      return TRUE;
   }

invalid:
   ipstr[0] = '\0';
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * NetUtil_GetPrimaryIP --
 *
 *      Get the primary IP for this machine.
 *
 * Results:
 *      If applicable address found, returns string of said IP address.
 *      If applicable address not found, returns an empty string.
 *      If an error occurred, returns NULL.
 *
 * Side effects:
 *	Caller is responsible for free()ing returned string.
 *
 *----------------------------------------------------------------------
 */

#if !defined(__FreeBSD__) && !defined(__APPLE__) /* { */
char *
NetUtil_GetPrimaryIP(void)
{
   int sd, i;
   struct ifconf iflist;
   struct ifreq ifaces[MAX_IFACES];
   char ipstr[INET_ADDRSTRLEN] = "";

   /* Get a socket descriptor to give to ioctl(). */
   sd = socket(PF_INET, SOCK_STREAM, 0);
   if (sd < 0) {
      return NULL;
   }

   memset(&iflist, 0, sizeof iflist);
   memset(ifaces, 0, sizeof ifaces);

   /* Tell ioctl where to write interface list to and how much room it has. */
   iflist.ifc_req = ifaces;
   iflist.ifc_len = sizeof ifaces;

   if (ioctl(sd, SIOCGIFCONF, &iflist) < 0) {
      close(sd);
      return NULL;
   }

   close(sd);

   /* Loop through the list of interfaces provided by ioctl(). */
   for (i = 0; i < (sizeof ifaces/sizeof *ifaces); i++) {
      if (ValidateConvertAddress(ifaces[i].ifr_name,
                                 (struct sockaddr_in *)&ifaces[i].ifr_addr,
                                 ipstr)) {
         break;
      }
   }

   /* Success.  Here, caller, you can throw this away. */
   return strdup(ipstr);
}

#else /* } FreeBSD || APPLE { */

char *
NetUtil_GetPrimaryIP(void)
{
   struct ifaddrs *ifaces;
   struct ifaddrs *curr;
   char ipstr[INET_ADDRSTRLEN] = "";

   /*
    * getifaddrs(3) creates a NULL terminated linked list of interfaces for us
    * to traverse and places a pointer to it in ifaces.
    */
   if (getifaddrs(&ifaces) < 0) {
      return NULL;
   }

   /*
    * We traverse the list until there are no more interfaces or we have found
    * the primary interface.  This function defines the primary interface to be
    * the first non-loopback, internet interface in the interface list.
    */
   for(curr = ifaces; curr != NULL; curr = curr->ifa_next) {
      if (ValidateConvertAddress(curr->ifa_name,
                                 (struct sockaddr_in *)curr->ifa_addr,
                                 ipstr)) {
         break;
      }
   }

   /* Tell FreeBSD to free our linked list. */
   freeifaddrs(ifaces);

   /* Success.  Here, caller, you can throw this away. */
   return strdup(ipstr);
}
#endif /* } */


/*
 *----------------------------------------------------------------------
 *
 * NetUtil_GetPrimaryNic --
 *
 *      Get the primary Nic entry for this machine. Primary Nic is the 
 *      first interface that comes up when you do a ifconfig.
 *
 * Results:
 *      The primary NIC entry or NULL if an error occurred. In nicEntry
 *      returned, only IP address is retuend. All other fields remain zero. 
 *
 * Side effects:
 *      Memory is allocated for the returned NicEntry. Caller is 
 *      supposed to free it after use.
 *
 *----------------------------------------------------------------------
 */

GuestNic *
NetUtil_GetPrimaryNic(void)
{
   GuestNic *nicEntry = NULL;
   VmIpAddress *ip;
   char *ipstr;

   ipstr = NetUtil_GetPrimaryIP();
   if (NULL == ipstr) {
      goto quit;
   }

   nicEntry = Util_SafeCalloc(1, sizeof *nicEntry);
   ip = Util_SafeCalloc(1, sizeof *ip);

   nicEntry->ips.ips_len = 1;
   nicEntry->ips.ips_val = ip;

   Str_Strcpy(ip->ipAddress, ipstr, sizeof ip->ipAddress);
   free(ipstr);

quit:
   return nicEntry;
}


#if defined(__linux__)
#   ifdef DUMMY_NETUTIL
/*
 *-----------------------------------------------------------------------------
 *
 * NetUtil_GetIfIndex (dummy version) --
 *
 *      Given an interface name, return its index.
 *
 * Results:
 *      Returns a valid interface index on success or -1 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
NetUtil_GetIfIndex(const char *ifName)  // IN: interface name
{
   NetUtilIfTableEntry *iterator;
   int ifIndex = -1;

   ASSERT(netUtilIfTable != NULL);

   for (iterator = netUtilIfTable; iterator->ifName; iterator++) {
      if (strcmp(iterator->ifName, ifName) == 0) {
         ifIndex = iterator->ifIndex;
         break;
      }
   }

   return ifIndex;
}


/*
 *-----------------------------------------------------------------------------
 *
 * NetUtil_GetIfName (dummy version) --
 *
 *      Given an interface index, return its name.
 *
 * Results:
 *      Returns a valid interface name on success, NULL on failure.
 *
 * Side effects:
 *      Caller is responsible for freeing the returned string.
 *
 *-----------------------------------------------------------------------------
 */

char *
NetUtil_GetIfName(int ifIndex)  // IN: interface index
{
   NetUtilIfTableEntry *iterator;
   char *ifName = NULL;

   ASSERT(netUtilIfTable != NULL);

   for (iterator = netUtilIfTable; iterator->ifName; iterator++) {
      if (iterator->ifIndex == ifIndex) {
         ifName = Util_SafeStrdup(iterator->ifName);
         break;
      }
   }

   return ifName;
}


#   endif // ifdef DUMMY_NETUTIL


/*
 *-----------------------------------------------------------------------------
 *
 * NetUtil_GetHardwareAddress --
 *
 *      Given an interface name, return its hardware/link layer address.
 *
 * Results:
 *      Returns TRUE and populates hwAddr on success.
 *      Returns FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

size_t
NetUtil_GetHardwareAddress(int ifIndex,         // IN
                           char *hwAddr,        // OUT
                           size_t hwAddrSize,   // IN
                           IanaIfType *ifType)  // OUT
{
   struct ifreq ifreq;
   int fd = -1;
   size_t ret = 0;

   if (hwAddrSize < IFHWADDRLEN) {
      return FALSE;
   }

   ASSERT(sizeof ifreq.ifr_name >= IF_NAMESIZE);

   memset(&ifreq, 0, sizeof ifreq);
   if (if_indextoname(ifIndex, ifreq.ifr_name) == NULL) {
      return FALSE;
   }

   if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
      return FALSE;
   }

   if (ioctl(fd, SIOCGIFHWADDR, &ifreq) == 0 &&
       ifreq.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
      memcpy(hwAddr, ifreq.ifr_hwaddr.sa_data, IFHWADDRLEN);
      *ifType = IANA_IFTYPE_ETHERNETCSMACD;
      ret = IFHWADDRLEN;
   }

   close(fd);

   return ret;
}


#endif // if defined(__linux__)

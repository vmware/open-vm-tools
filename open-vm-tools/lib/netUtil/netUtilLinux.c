/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
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


#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun)
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

#ifdef __FreeBSD__
#include "ifaddrs.h"
#endif

#include "vm_assert.h"
#include "netutil.h"
#include "debug.h"
#include "guestApp.h"
#include "util.h"
#include "str.h"

#define MAX_IFACES      4
#define LOOPBACK        "lo"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif


/*
 *----------------------------------------------------------------------
 *
 * NetUtil_GetPrimaryIP --
 *
 *      Get the primary IP for this machine.
 *
 * Results:
 *      
 *      The IP or NULL if an error occurred.
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifndef __FreeBSD__ /* { */
char *
NetUtil_GetPrimaryIP(void)
{
   int sd, i;
   struct ifconf iflist;
   struct ifreq ifaces[MAX_IFACES];
   char *ipstr;

   /* Get a socket descriptor to give to ioctl(). */
   sd = socket(PF_INET, SOCK_STREAM, 0);
   if (sd < 0) {
      goto error;
   }

   memset(&iflist, 0, sizeof iflist);
   memset(ifaces, 0, sizeof ifaces);

   /* Tell ioctl where to write interface list to and how much room it has. */
   iflist.ifc_req = ifaces;
   iflist.ifc_len = sizeof ifaces;

   if (ioctl(sd, SIOCGIFCONF, &iflist) < 0) {
      close(sd);
      goto error;
   }

   close(sd);

   /* Loop through the list of interfaces provided by ioctl(). */
   for (i = 0; i < (sizeof ifaces/sizeof *ifaces); i++) {
      /*
       * Find the first interface whose name is not blank and isn't a
       * loopback device.  This should be the primary interface.
       */
      if ((*ifaces[i].ifr_name != '\0') &&
          (strncmp(ifaces[i].ifr_name, LOOPBACK, strlen(LOOPBACK)) != 0)) {
         struct sockaddr_in *addr;

         /*
          * Allocate memory to return to caller; they must free this if we
          * don't return error.
          */
         ipstr = calloc(1, INET_ADDRSTRLEN);
         if (!ipstr) {
            goto error;
         }

         addr = (struct sockaddr_in *)(&ifaces[i].ifr_addr);

         /* Convert this address to dotted decimal */
         if (inet_ntop(AF_INET, (void *)&addr->sin_addr,
                       ipstr, INET_ADDRSTRLEN) == NULL) {
            goto error_free;
         }

         /* We'd rather return NULL than an IP of zeros. */
         if (strcmp(ipstr, "0.0.0.0") == 0) {
            goto error_free;
         }

         return ipstr;
      }
   }

   /* Making it through loop means no non-loopback devices were found. */
   return NULL;

error_free:
   free(ipstr);
error:
   return NULL;
}

#else /* } FreeBSD { */

char *
NetUtil_GetPrimaryIP(void)
{
   struct ifaddrs *ifaces;
   struct ifaddrs *curr;
   char ipstr[INET_ADDRSTRLEN];

   /*
    * getifaddrs(3) creates a NULL terminated linked list of interfaces for us
    * to traverse and places a pointer to it in ifaces.
    */
   if (getifaddrs(&ifaces) < 0) {
      return NULL;
   }

   if (!ifaces) {
      return NULL;
   }

   /*
    * We traverse the list until there are no more interfaces or we have found
    * the primary interface.  This function defines the primary interface to be
    * the first non-loopback, internet interface in the interface list.
    */
   for(curr = ifaces; curr != NULL; curr = curr->ifa_next) {
      struct sockaddr_in *addr;

      /* Ensure this isn't a loopback device. */
      if (strncmp(curr->ifa_name, LOOPBACK, strlen(LOOPBACK)) == 0) {
         continue;
      }

      addr = (struct sockaddr_in *)(curr->ifa_addr);

      /* Ensure this is an (IPv4) internet interface. */
      if (addr->sin_family == AF_INET) {
         memset(ipstr, 0, sizeof ipstr);

         /* Attempt network to presentation conversion. */
         if (inet_ntop(AF_INET, (void *)&addr->sin_addr, ipstr, sizeof ipstr) == NULL) {
            continue;
         }

         /* If the IP is all zeros we'll try for another interface. */
         if (strcmp(ipstr, "0.0.0.0") == 0) {
            /* Empty the string so we never return "0.0.0.0". */
            ipstr[0] = '\0';
            continue;
         }

         /*
          * We have found the primary interface and its dotted-decimal IP is
          * in ipstr.
          */
         break;
      }
   }

   /* Tell FreeBSD to free our linked list. */
   freeifaddrs(ifaces);

   /*
    * If ipstr is blank, just return NULL.  Otherwise, we create a copy of the
    * string and return the pointer; the caller must free this memory.
    */
   return (ipstr[0] == '\0') ? NULL : strdup(ipstr);
}
#endif /* } */


/*
 *----------------------------------------------------------------------
 *
 * NetUtil_GetPrimaryNicEntry --
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

NicEntry *
NetUtil_GetPrimaryNicEntry(void)
{
   NicEntry *nicEntry = NULL;
   VmIpAddressEntry *ipAddressEntry;
   char *ipstr;

   ipstr = NetUtil_GetPrimaryIP();
   if (NULL == ipstr) {
      goto abort;
   }

   nicEntry = Util_SafeCalloc(1, sizeof *nicEntry);
   DblLnkLst_Init(&nicEntry->ipAddressList);
   ipAddressEntry = Util_SafeCalloc(1, sizeof *ipAddressEntry);
   DblLnkLst_Init(&ipAddressEntry->links);
   DblLnkLst_LinkLast(&nicEntry->ipAddressList, &ipAddressEntry->links);

   /*
    *  Now, record these values in nicEntry.
    */
   Str_Strcpy(ipAddressEntry->ipEntryProto.ipAddress, 
              ipstr,
              sizeof ipAddressEntry->ipEntryProto.ipAddress);

   free(ipstr);

abort:
   return nicEntry;
}

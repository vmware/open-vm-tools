/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * machineID.c --
 *
 *	Obtain "universally unique" identification information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(_WIN32)	// Windows
#include <windows.h>
#include <process.h>
#include <iptypes.h>
#endif

#include "vmware.h"
#include "hostinfo.h"
#include "util.h"
#include "log.h"
#include "str.h"
#include "err.h"
#include "vm_product.h"
#include "vm_atomic.h"

#define LOGLEVEL_MODULE main
#include "loglevel_user.h"

#if defined(_WIN32)	// Windows
/*
 *----------------------------------------------------------------------
 *
 * FindWindowsAdapter --
 *
 *      Search the list of networking interfaces for an appropriate choice.
 *
 *      A suitable adapter either must contain or must not contain a
 *      (simple) pattern string as specified by the findPattern argument.
 *
 * Results:
 *      IP_ADAPTER_INFO pointer or NULL if no suitable choice can be made.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static IP_ADAPTER_INFO *
FindWindowsAdapter(IP_ADAPTER_INFO *head,  // IN:
                   char *pattern,          // IN:
                   Bool findPattern)       // IN:
{
   IP_ADAPTER_INFO *adapterInfo;
   IP_ADAPTER_INFO *adapterChoice = NULL;

   ASSERT(head);
   ASSERT(pattern);

   for (adapterInfo = head; adapterInfo; adapterInfo = adapterInfo->Next) {
      // Bias - only ethernet adapters; more likely to be local
      if (adapterInfo->AddressLength == 6) {
         Bool takeIt = FALSE;

         if ((adapterInfo->Description == NULL) ||
             (*adapterInfo->Description == '\0')) {
            takeIt = TRUE;
         } else {
            char *p = strstr(adapterInfo->Description, pattern);

            if (((p != NULL) && findPattern) ||
                ((p == NULL) && !findPattern)) {
               takeIt = TRUE;
            }
         }

         if (takeIt) {
            adapterChoice = adapterInfo;
            break;
         }
      }
   }

   return adapterChoice;
}


/*
 *----------------------------------------------------------------------
 *
 * ObtainHardwareID --
 *
 *      Locate and return the hardwareID for this machine.
 *
 * Results:
 *      0       No errors occured
 *      >0      failure (errno)
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Return the MAC address of a suitable networking interface.
 *      The hardwareID will be zero if nothing suitable could be found.
 *
 *----------------------------------------------------------------------
 */

static int
ObtainHardwareID(uint64 *hardwareID) // OUT:
{
   void *buf;
   DWORD status;
   HMODULE dllHandle;
   IP_ADAPTER_INFO *adapterList;
   IP_ADAPTER_INFO *adapterChoice;
   DWORD (WINAPI *getAdaptersFn)(IP_ADAPTER_INFO *, ULONG *);

   ULONG bufLen = 0;

   // Deal with BUG 21643
   dllHandle = LoadLibrary(TEXT("icmp.dll"));

   if (!dllHandle) {
      Warning("%s Failed to load icmp.dll.\n", __FUNCTION__);

      return EINVAL;
   }

   FreeLibrary(dllHandle);

   dllHandle = LoadLibrary(TEXT("IpHlpApi.dll"));

   if (!dllHandle) {
      Warning("%s Failed to load iphlpapi.dll.\n", __FUNCTION__);

      return EINVAL;
   }

   getAdaptersFn = (void *) GetProcAddress(dllHandle, "GetAdaptersInfo");

   if (!getAdaptersFn) {
      FreeLibrary(dllHandle);
      Warning("%s Failed to find GetAdaptersInfo.\n", __FUNCTION__);

      return EINVAL;
   }

   // Force GetAdaptersInfo to provide the amount of memory necessary.

   status = (*getAdaptersFn)(NULL, &bufLen);

   switch (status) {
   case ERROR_NOT_SUPPORTED:
      Warning("%s GetAdaptersInfo is not supported.\n", __FUNCTION__);
      // fall through
   case ERROR_NO_DATA:
      FreeLibrary(dllHandle);
      *hardwareID = 0;
      return 0;
      break;

   case ERROR_BUFFER_OVERFLOW:
   case NO_ERROR:
      break;

   default:
      FreeLibrary(dllHandle);
      Warning("%s GetAdaptersInfo failure %d: %d.\n", __FUNCTION__,
              __LINE__, status);

      return EINVAL;
   }

   buf = malloc(bufLen);

   if (buf == NULL) {
      FreeLibrary(dllHandle);

      return ENOMEM;
   }

   adapterList = (IP_ADAPTER_INFO *) buf;

   status = (*getAdaptersFn)(adapterList, &bufLen);

   FreeLibrary(dllHandle);

   if (status != NO_ERROR) {
      // something is seriously wrong; worked before...
      Warning("%s GetAdaptersInfo failure %d: %d.\n", __FUNCTION__,
              __LINE__, status);

      free(buf);

      return EINVAL;
   }

   // Try to find real hardware.
   adapterChoice = FindWindowsAdapter(adapterList, PRODUCT_GENERIC_NAME,
                                      FALSE);

   if (adapterChoice == NULL) {
      *hardwareID = 0;
   } else {
      union bytes {
         uint64 data;
         char   bytes[8];
      } x;

      x.bytes[0] = adapterChoice->Address[0];
      x.bytes[1] = adapterChoice->Address[1];
      x.bytes[2] = adapterChoice->Address[2];
      x.bytes[3] = adapterChoice->Address[3];
      x.bytes[4] = adapterChoice->Address[4];
      x.bytes[5] = adapterChoice->Address[5];
      x.bytes[6] = '\0';
      x.bytes[7] = '\0';

      *hardwareID = x.data;
   }

   free(buf);

   return 0;
}
#elif defined(__APPLE__)	// MacOS X
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/ethernet.h>


/*
 *----------------------------------------------------------------------
 *
 * CheckEthernet --
 *
 *      Determine if "en<n>" exists and, if so, return its MAC address.
 *
 * Results:
 *      NULL    interface doesn't exist or isn't usable
 *      !NULL   interface exists and is usable
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static struct ifaddrs *
CheckEthernet(struct ifaddrs *ifp,  // IN:
              uint32 n)             // IN:
{
   struct ifaddrs *p;
   char name[8];

   // Construct the interface name
   Str_Sprintf(name, sizeof name, "en%u", n);

   // Go through the list and see if this interface exists and is usable.
   p = ifp;

   while (p != NULL) {
      if ((strcmp(p->ifa_name, name) == 0) &&
          (p->ifa_addr != NULL) &&
          (p->ifa_addr->sa_family == AF_LINK)) {
         break;
      }

      p = p->ifa_next;
   }

   return p;
}


/*
 *----------------------------------------------------------------------
 *
 * ObtainHardwareID --
 *
 *      Locate and return the hardwareID for this machine.
 *
 * Results:
 *      0       No errors occured
 *      >0      failure (errno)
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Return the MAC address of a suitable networking interface.
 *      The hardwareID will be zero if nothing suitable could be found.
 *
 *----------------------------------------------------------------------
 */

static int
ObtainHardwareID(uint64 *hardwareID)  // OUT:
{
   uint32 i;
   struct ifaddrs *ifp;

   // Attempt to get the list of networking interfaces
   if (getifaddrs(&ifp) == -1) {
      int saveErrno = errno;

      Warning("%s getifaddrs failure: %s.\n", __FUNCTION__,
              Err_Errno2String(saveErrno));

      return saveErrno;
   }

   // search through a "reasonable" number of interfaces
   for (i = 0, *hardwareID = 0; i < 8; i++) {
      struct ifaddrs *p = CheckEthernet(ifp, i);

      if (p != NULL) {
         memcpy(hardwareID, LLADDR((struct sockaddr_dl *)p->ifa_addr), ETHER_ADDR_LEN);
         break;
      }
   }

   freeifaddrs(ifp);

   return 0;
}
#elif defined(__linux__) || defined __ANDROID__
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/time.h>
#if defined __ANDROID__
#include <sys/socket.h>  // For SOCK_DGRAM etc.
#endif


/*
 *----------------------------------------------------------------------
 *
 * CheckEthernet --
 *
 *      Determine if "eth<n>" exists and, if so, return its MAC address
 *
 * Results:
 *      0       No errors occured
 *      >0      failure (errno)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
CheckEthernet(uint32 n,        // IN:
              char *machineID) // OUT:
{
   int          fd;
   int          erc;
   struct ifreq ifreq;
   int          saveErrno;

   fd = socket(AF_INET, SOCK_DGRAM, 0);

   if (fd == -1) {
      return errno;
   }

   /* get the MAC address of eth<n> */
   Str_Sprintf(ifreq.ifr_name, IFNAMSIZ, "eth%u", n);

   erc = ioctl(fd, SIOCGIFHWADDR, &ifreq);
   saveErrno = errno;

   close(fd);

   if (erc == -1) {
      return saveErrno;
   }

   machineID[0] = ifreq.ifr_hwaddr.sa_data[0] & 0xFF;
   machineID[1] = ifreq.ifr_hwaddr.sa_data[1] & 0xFF;
   machineID[2] = ifreq.ifr_hwaddr.sa_data[2] & 0xFF;
   machineID[3] = ifreq.ifr_hwaddr.sa_data[3] & 0xFF;
   machineID[4] = ifreq.ifr_hwaddr.sa_data[4] & 0xFF;
   machineID[5] = ifreq.ifr_hwaddr.sa_data[5] & 0xFF;
   machineID[6] = '\0';
   machineID[7] = '\0';

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * ObtainHardwareID --
 *
 *      Locate the hardwareID for this machine.
 *
 * Results:
 *      0       No errors occured
 *      >0      failure (errno)
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Return the MAC address of a suitable networking interface.
 *      The hardwareID will be zero if nothing suitable could be found.
 *
 *      This is a "good enough" initial hack but will need to be revisited.
 *      Ideally checks should be made for sysfs (Linux 2.6 and later) and,
 *      if present, search through the information and try to insure that
 *      the ethernet chosen is on the motherboard.
 *
 *----------------------------------------------------------------------
 */

static int
ObtainHardwareID(uint64 *hardwareID)  // OUT:
{
   uint32 i;

   // search through a "reasonable" number of interfaces
   for (i = 0; i < 8; i++) {
      int erc;

      erc = CheckEthernet(i, (char *) hardwareID);

      if (erc == 0) {
         return 0;
      } else {
         if (erc != ENODEV) {
            Warning("%s unexpected failure: %d.\n", __FUNCTION__, erc);
            return erc;
         }
      }
   }

   *hardwareID = 0;

   return 0;
}
#else				// Not a specifically code OS
#include <unistd.h>


/*
 *----------------------------------------------------------------------
 *
 * ObtainHardwareID --
 *
 *      Locate the hardwareID for this machine.
 *
 * Results:
 *      0       No errors occured
 *      >0      failure (errno)
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      This is a dummy to catch an uncoded OS.
 *----------------------------------------------------------------------
 */

static int
ObtainHardwareID(uint64 *hardwareID)  // OUT:
{
   *hardwareID = gethostid();

   return 0;
}
#endif				// Not a specifically code OS


/*
 *----------------------------------------------------------------------
 *
 * HostNameHash --
 *
 *      Return the hash value of a string.
 *
 * Results:
 *      The hash value of the string.
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      This uses the DBJ2 hash algorithm.
 *
 *----------------------------------------------------------------------
 */

static uint32
HostNameHash(unsigned char *str) // IN:
{
   uint32 c;

   uint32 hash = 5381;

   while ((c = *str++) != '\0') {
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
   }

   return hash;
}

 
/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_MachineID --
 *
 *      Return the machine ID information.
 *
 *      There are two pieces of identification returned: a hash of the
 *      hostname and and a hardware identifier. If either of them is
 *      unavailable they will be given a value of zero.
 *
 *      The hardware identifier should be extracted from a piece of hardware
 *      that has individual discrimination - each "part" is uniquely labelled.
 *      The MAC address of an ethernet and the WWN of a FibreChannel HBA are
 *      perfect examples.
 *
 * Results:
 *	The values are returned.
 *
 * Side effects:
 *      The hardware identifier should persistent across system reboots and
 *      resets as long of the hardware is available.
 *
 *      Currently the Windows implementation is used only for file locking -
 *      not system identification - so any acceptable piece of hardware
 *      may be used, even if it changes during a reboot.
 *
 *----------------------------------------------------------------------
 */

void
Hostinfo_MachineID(uint32 *hostNameHash,    // OUT:
                   uint64 *hostHardwareID)  // OUT:
{
   static Atomic_Ptr cachedHardwareID;
   static Atomic_Ptr cachedHostNameHash;
   uint64 *tmpHardwareID;
   uint32 *tmpNameHash;

   ASSERT(hostNameHash);
   ASSERT(hostHardwareID);

   tmpNameHash = Atomic_ReadPtr(&cachedHostNameHash);
   if (!tmpNameHash) {
      char *hostName;

      tmpNameHash = Util_SafeMalloc(sizeof *tmpNameHash);

      // 4 bytes (32 bits) of host name information
      hostName = (char *) Hostinfo_HostName();

      if (hostName == NULL) {
         Warning("%s Hostinfo_HostName failure; providing default.\n",
                __FUNCTION__);

         *tmpNameHash = 0;
      } else {
         *tmpNameHash = HostNameHash((unsigned char *) hostName);
         free(hostName);
      }

      if (Atomic_ReadIfEqualWritePtr(&cachedHostNameHash, NULL, 
                                     tmpNameHash)) {
         free(tmpNameHash);
         tmpNameHash = Atomic_ReadPtr(&cachedHostNameHash);
      }
   }
   *hostNameHash = *tmpNameHash;

   tmpHardwareID = Atomic_ReadPtr(&cachedHardwareID);
   if (!tmpHardwareID) {
      int  erc;

      tmpHardwareID = Util_SafeMalloc(sizeof *tmpHardwareID);

      // 8 bytes (64 bits) of hardware information
      erc = ObtainHardwareID(tmpHardwareID);
      if (erc != 0) {
         Warning("%s ObtainHardwareID failure (%s); providing default.\n",
                 __FUNCTION__, Err_Errno2String(erc));

         *tmpHardwareID = 0;
      }

      if (Atomic_ReadIfEqualWritePtr(&cachedHardwareID, NULL, 
                                     tmpHardwareID)) {
         free(tmpHardwareID);
         tmpHardwareID = Atomic_ReadPtr(&cachedHardwareID);
      }
   }
   *hostHardwareID = *tmpHardwareID;
}

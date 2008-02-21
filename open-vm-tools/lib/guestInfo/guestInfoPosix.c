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
# include <sys/sockio.h>
# include <sys/systeminfo.h>
# include <dnet.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef __FreeBSD__
# include <netinet/in.h>
# include <net/if_dl.h>
# include <ifaddrs.h>
# include <sys/sysctl.h>
#endif
#include "util.h"
#include "arpa/inet.h"
#include "sys/utsname.h"
#include "net/if_arp.h"
#include "net/if.h"
#include "netdb.h"
#include "sys/ioctl.h"
#include "vmware.h"
#include "guestInfoInt.h"
#include "debug.h"
#include "str.h"
#include "guest_os.h"
#include "guestApp.h"
#include "guestInfo.h"

#define DISTRO_BUF_SIZE 255

#define SYSINFO_STRING_32       "i386"
#define SYSINFO_STRING_64       "amd64"
#define MAX_ARCH_NAME_LEN       sizeof SYSINFO_STRING_32 > sizeof SYSINFO_STRING_64 ? \
                                   sizeof SYSINFO_STRING_32 : \
                                   sizeof SYSINFO_STRING_64

typedef struct lsb_distro_info {
   char *name;
   char *scanstring;
} LSBDistroInfo;


LSBDistroInfo lsbFields[] = {
   {"DISTRIB_ID=", "DISTRIB_ID=%s"},
   {"DISTRIB_RELEASE=", "DISTRIB_RELEASE=%s"},
   {"DISTRIB_CODENAME=", "DISTRIB_CODENAME=%s"},
   {"DISTRIB_DESCRIPTION=", "DISTRIB_DESCRIPTION=%s"},
   {NULL, NULL},
};


typedef struct distro_info {
   char *name;
   char *filename;
} DistroInfo;


DistroInfo distroArray[] = {
   {"RedHat", "/etc/redhat-release"},
   {"RedHat", "/etc/redhat_version"},
   {"Sun", "/etc/sun-release"},
   {"SuSE", "/etc/SuSE-release"},
   {"SuSE", "/etc/novell-release"},
   {"SuSE", "/etc/sles-release"},
   {"Debian", "/etc/debian_version"},
   {"Debian", "/etc/debian_release"},
   {"Mandrake", "/etc/mandrake-release"},
   {"Mandriva", "/etc/mandriva-release"},
   {"Mandrake", "/etc/mandrakelinux-release"},
   {"TurboLinux", "/etc/turbolinux-release"},
   {"Fedora Core", "/etc/fedora-release"},
   {"Gentoo", "/etc/gentoo-release"},
   {"Novell", "/etc/nld-release"},
   {"Ubuntu", "/etc/lsb-release"},
   {"Annvix", "/etc/annvix-release"},
   {"Arch", "/etc/arch-release"},
   {"Arklinux", "/etc/arklinux-release"},
   {"Aurox", "/etc/aurox-release"},
   {"BlackCat", "/etc/blackcat-release"},
   {"Cobalt", "/etc/cobalt-release"},
   {"Conectiva", "/etc/conectiva-release"},
   {"Immunix", "/etc/immunix-release"},
   {"Knoppix", "/etc/knoppix_version"},
   {"Linux-From-Scratch", "/etc/lfs-release"},
   {"Linux-PPC", "/etc/linuxppc-release"},
   {"MkLinux", "/etc/mklinux-release"},
   {"PLD", "/etc/pld-release"},
   {"Slackware", "/etc/slackware-version"},
   {"Slackware", "/etc/slackware-release"},
   {"SMEServer", "/etc/e-smith-release"},
   {"Solaris", "/etc/release"},
   {"Tiny Sofa", "/etc/tinysofa-release"},
   {"UltraPenguin", "/etc/ultrapenguin-release"},
   {"UnitedLinux", "/etc/UnitedLinux-release"},
   {"VALinux", "/etc/va-release"},
   {"Yellow Dog", "/etc/yellowdog-release"},
   {NULL, NULL},
};



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

#if defined(__FreeBSD__)
Bool
GuestInfoGetNicInfo(NicInfo *nicInfo)           // OUT
{
   struct ifaddrs *ifaces;
   struct ifaddrs *cur;
   typedef struct IfNameNicMap {    // linked list to remember interface names
      char *name;
      NicEntry *nicEntry;     // point to the NIC entry of the interface
      DblLnkLst_Links links;
   } IfNameNicMap;
   IfNameNicMap *ifNamesCur;
   DblLnkLst_Links ifNameList;
   DblLnkLst_Links *sCurrent;
   DblLnkLst_Links *sNext;
   DblLnkLst_Links *linkIfName;

   ASSERT(nicInfo);
   memset(nicInfo, 0, sizeof *nicInfo);
   DblLnkLst_Init(&nicInfo->nicList);
   DblLnkLst_Init(&ifNameList);

   if (getifaddrs(&ifaces) < 0) {
      Debug("GuestInfo: Error, failed to call getifaddrs(3)\n");
      return FALSE;
   }

   /*
    * First pass: identify all interfaces with MAC addresses. As we add each
    * MAC address to nicInfo, we also track the interface name tied to that
    * address. This is because we must later use the interface name to find
    * the appropriate MAC given a pair of {name,IP}.
    */
   for (cur = ifaces; cur != NULL; cur = cur->ifa_next) {
      if (cur->ifa_addr->sa_family == AF_LINK) {
         NicEntry *nicEntryCur;
         unsigned char tempMacAddress[6];
         char macAddress[MAC_ADDR_SIZE];
         struct sockaddr_dl *sdl = (struct sockaddr_dl *)cur->ifa_addr;

         /*
          * By ensuring the address length is 6, we implicitly ignore all
          * non-Ethernet addresses (such as loopback).
          */
         if (sdl->sdl_alen != sizeof tempMacAddress) {
            Debug("GuestInfo: Unexpected length for MAC address, skipping "
                  "interface\n");
            continue;
         }

         /*
          * insert the new entry to the end of the nicList
          */

         memcpy(tempMacAddress, LLADDR(sdl), sdl->sdl_alen);
         Str_Sprintf(macAddress,
                     sizeof macAddress,
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     tempMacAddress[0], tempMacAddress[1], tempMacAddress[2],
                     tempMacAddress[3], tempMacAddress[4], tempMacAddress[5]);

         nicEntryCur = NicInfo_AddNicEntry(nicInfo, macAddress);

         /*
          * Make interface name link list
          */
         ifNamesCur = Util_SafeCalloc(1, sizeof *ifNamesCur);
         ifNamesCur->name = cur->ifa_name;
         ifNamesCur->nicEntry = nicEntryCur;
         DblLnkLst_Init(&ifNamesCur->links);
         DblLnkLst_LinkLast(&ifNameList, &ifNamesCur->links);
      } 
   } 
  
   /* Second pass: tie each IP address to its MAC address. */
   for (cur = ifaces; cur != NULL; cur = cur->ifa_next) {
      if (cur->ifa_addr->sa_family == AF_INET) {
         struct sockaddr_in *sin = (struct sockaddr_in *)cur->ifa_addr;

         /*
          * This is tedious but necessary. We iterate over the stored interface
          * names looking for the one tied to this IP address. Then we convert
          * the IP address to a string and store it in the nicInfo struct.
          */

         DblLnkLst_ForEach(linkIfName, &ifNameList) {
            ifNamesCur = DblLnkLst_Container(linkIfName, IfNameNicMap, links);
            if (strcmp(ifNamesCur->name, cur->ifa_name) == 0) {
               VmIpAddressEntry *ipAddressCur;
               NicEntry *entry = ifNamesCur->nicEntry;
               char ipAddress[IP_ADDR_SIZE];

               if (!inet_ntop(AF_INET,
                              &sin->sin_addr,
                              ipAddress,
                              sizeof ipAddress)) {
                  Debug("GuestInfo: Could not convert IP address, skipping "
                        "IP\n");
                  break;
               }

               ipAddressCur = NicEntry_AddIpAddress(entry, 
                                                    ipAddress,
                                                    0); /* not used */
               break;
            }
         } 
      } 
   } 

   freeifaddrs(ifaces);

   /* free alllocated temp memory */
   DblLnkLst_ForEachSafe(sCurrent, sNext, &ifNameList) {
      DblLnkLst_Unlink1(sCurrent);
      free(DblLnkLst_Container(sCurrent, IfNameNicMap, links));
   }

   return TRUE;
}

#else /* FreeBSD */

Bool
GuestInfoGetNicInfo(NicInfo *nicInfo)           // OUT
{
   Bool retVal = FALSE;
   int sockfd;
   struct ifconf ifc;
   struct ifreq *ifr;
   unsigned int numReqs = 30;  /* Initial estimate of number of ifreqs. */
   unsigned int i;
   char ipAddress[IP_ADDR_SIZE];
   char macAddress[MAC_ADDR_SIZE];

   ASSERT(nicInfo);
   if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      Debug("GuestInfo: Error, could not create socket\n");
      return FALSE;
   }

   memset(nicInfo, 0, sizeof *nicInfo);
   DblLnkLst_Init(&nicInfo->nicList);
   ifc.ifc_buf = NULL;
  
   /* Get all the interface entries for this machine. */
   for (;;) {
      void *tmp = ifc.ifc_buf;
      ifc.ifc_len = (sizeof *ifr) * numReqs;
      if (!(ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len))) {
         Debug("GuestInfo: Error, failed to allocate ifconf\n");
         free(tmp);
         goto out;
      }

      if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
         Debug("GuestInfo: SIOCGIFCONF failed\n");
         goto out;
      }
      if (ifc.ifc_len == (sizeof *ifr) * numReqs) {
         /* assume it overflowed and try again */
         numReqs += 10;
         continue;
      }
      break;
   } 

   /* Get the IP and MAC address for each of these interfaces. */
   ifr = ifc.ifc_req;

   for (i = 0; i * (sizeof *ifr) < ifc.ifc_len; i++, ifr++) {
      unsigned char *ptr = NULL;
      VmIpAddressEntry *ipAddressCur;
      NicEntry *macNicEntry;
#     ifdef sun
      eth_t *device;
      eth_addr_t addr;
#     endif

      Debug("%s\n", ifr->ifr_name);

      /*
       * Get the mac address for this interface. Some interfaces (like the
       * loopback interface) have no MAC address, and we skip them.
       */
#     ifndef sun
      if (ioctl(sockfd, SIOCGIFHWADDR, ifr) < 0) {
         Debug("GuestInfo: Failed to get MAC address, skipping interface\n");
         continue;
      }

      ptr = &ifr->ifr_hwaddr.sa_data[0];
#     else
      /* libdnet's eth_* interface gets the ethernet address for us. */
      device = eth_open(ifr->ifr_name);
      if (!device) {
         Debug("GuestInfo: Failed to open device, skipping interface\n");
         continue;
      }

      if (eth_get(device, &addr) != 0) {
         eth_close(device);
         Debug("GuestInfo: Failed to get MAC address, skipping interface\n");
         continue;
      }

      eth_close(device);
      ptr = &addr.data[0];
#endif

      
      /* Get the corresponding IP address. */
      ifr->ifr_addr.sa_family = AF_INET;
      if (ioctl(sockfd, SIOCGIFADDR, ifr) < 0) {
         Debug("GuestInfo: Failed to get ip address, skipping interface\n");
         continue;
      }

      /* Convert IP to a string. */
      if (!inet_ntop(AF_INET,
                     &((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr,
                     ipAddress, sizeof ipAddress)) {
         Debug("GuestInfo: Failed in inet_ntop, skipping interface\n");
         continue;
      }

      Str_Sprintf(macAddress, sizeof macAddress,
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  *ptr, *(ptr + 1), *(ptr + 2),
                  *(ptr + 3), *(ptr + 4), *(ptr + 5));

      /* Which entry in nic info corresponds to this MAC address? */
      macNicEntry = NicInfo_FindMacAddress(nicInfo, macAddress);
      if (NULL == macNicEntry) {
         /* This mac address has not been added to nicInfo, get a new entry. */  
         macNicEntry = NicInfo_AddNicEntry(nicInfo,
                                           macAddress);
      }

      ipAddressCur = NicEntry_AddIpAddress(macNicEntry, ipAddress, 0);
   }

   retVal = TRUE;

out:
   if (close(sockfd) == -1) {
      Debug("GuestInfo: Close socket failed\n");
      retVal = FALSE;
   }

   if (ifc.ifc_buf) {
      free(ifc.ifc_buf);
   }
   return retVal;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GetShortName --
 *
 *      Returns distro information based on .vmx format (distroShort).
 *
 * Return value:
 *      Overwrited the short name if we recognise the OS.
 *      Otherwise leave the short name as it is.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
GetShortName(char *distro,             // IN: full distro name
             char *distroShort,        // OUT: short distro name (based on .vmx file)
             int distroShortSize)      // IN: size of the buffer allocated for short distro name

{
   char *distroLower = NULL;  /* Lower case distro name */

   distroLower = calloc(strlen(distro) + 1, sizeof *distroLower);

   if (distroLower == NULL) {
      Debug("GetShortName: could not allocate memory\n");
      return;
   }

   Str_Strcpy(distroLower, distro, distroShortSize);
   distroLower = Str_ToLower(distroLower);

   if (strstr(distroLower, "red hat")) {
      if (strstr(distroLower, "enterprise")) {

         /*
          * Looking for "release x" here instead of "x" as there could be
          * build version which can be misleading.
          * For example Red Hat Enterprise Linux ES release 4 (Nahant Update 3)
          */

         int release = 0;
         char *releaseStart = strstr(distroLower, "release");

         if (releaseStart) {
            sscanf(releaseStart, "release %d", &release);
            if (release > 0) {
               snprintf(distroShort, distroShortSize, STR_OS_RED_HAT_EN"%d", release);
            }
         }

         if (release <= 0) {
            Debug("GetShortName: could not read Red Hat Enterprise release version\n");
            Str_Strcpy(distroShort, STR_OS_RED_HAT_EN, distroShortSize);
         }

      } else {
         Str_Strcpy(distroShort, STR_OS_RED_HAT, distroShortSize);
      }
   } else if (strstr(distroLower, "suse")) {
      if (strstr(distroLower, "enterprise")) {
         Str_Strcpy(distroShort, STR_OS_SUSE_EN, distroShortSize);
      } else if (strstr(distroLower, "sun")) {
         Str_Strcpy(distroShort, STR_OS_SUN_DESK, distroShortSize);
      } else if (strstr(distroLower, "novell")) {
         Str_Strcpy(distroShort, STR_OS_NOVELL, distroShortSize);
      } else {
         Str_Strcpy(distroShort, STR_OS_SUSE, distroShortSize);
      }
   } else if (strstr(distroLower, "mandrake")) {
      Str_Strcpy(distroShort, STR_OS_MANDRAKE, distroShortSize);
   } else if (strstr(distroLower, "turbolinux")) {
      Str_Strcpy(distroShort, STR_OS_TURBO, distroShortSize);
   } else if (strstr(distroLower, "sun")) {
      Str_Strcpy(distroShort, STR_OS_SUN_DESK, distroShortSize);
   } else if (strstr(distroLower, "annvix")) {
      Str_Strcpy(distroShort, STR_OS_ANNVIX, distroShortSize);
   } else if (strstr(distroLower, "arch")) {
      Str_Strcpy(distroShort, STR_OS_ARCH, distroShortSize);
   } else if (strstr(distroLower, "arklinux")) {
      Str_Strcpy(distroShort, STR_OS_ARKLINUX, distroShortSize);
   } else if (strstr(distroLower, "aurox")) {
      Str_Strcpy(distroShort, STR_OS_AUROX, distroShortSize);
   } else if (strstr(distroLower, "black cat")) {
      Str_Strcpy(distroShort, STR_OS_BLACKCAT, distroShortSize);
   } else if (strstr(distroLower, "cobalt")) {
      Str_Strcpy(distroShort, STR_OS_COBALT, distroShortSize);
   } else if (strstr(distroLower, "conectiva")) {
      Str_Strcpy(distroShort, STR_OS_CONECTIVA, distroShortSize);
   } else if (strstr(distroLower, "debian")) {
      Str_Strcpy(distroShort, STR_OS_DEBIAN, distroShortSize);
   } else if (strstr(distroLower, "fedora")) {
      Str_Strcpy(distroShort, STR_OS_FEDORA, distroShortSize);
   } else if (strstr(distroLower, "gentoo")) {
      Str_Strcpy(distroShort, STR_OS_GENTOO, distroShortSize);
   } else if (strstr(distroLower, "immunix")) {
      Str_Strcpy(distroShort, STR_OS_IMMUNIX, distroShortSize);
   } else if (strstr(distroLower, "linux-from-scratch")) {
      Str_Strcpy(distroShort, STR_OS_LINUX_FROM_SCRATCH, distroShortSize);
   } else if (strstr(distroLower, "linux-ppc")) {
      Str_Strcpy(distroShort, STR_OS_LINUX_PPC, distroShortSize);
   } else if (strstr(distroLower, "mandravia")) {
      Str_Strcpy(distroShort, STR_OS_MANDRAVIA, distroShortSize);
   } else if (strstr(distroLower, "mklinux")) {
      Str_Strcpy(distroShort, STR_OS_MKLINUX, distroShortSize);
   } else if (strstr(distroLower, "pld")) {
      Str_Strcpy(distroShort, STR_OS_PLD, distroShortSize);
   } else if (strstr(distroLower, "slackware")) {
      Str_Strcpy(distroShort, STR_OS_SLACKWARE, distroShortSize);
   } else if (strstr(distroLower, "sme server")) {
      Str_Strcpy(distroShort, STR_OS_SMESERVER, distroShortSize);
   } else if (strstr(distroLower, "tiny sofa")) {
      Str_Strcpy(distroShort, STR_OS_TINYSOFA, distroShortSize);
   } else if (strstr(distroLower, "ubuntu")) {
      Str_Strcpy(distroShort, STR_OS_UBUNTU, distroShortSize);
   } else if (strstr(distroLower, "ultra penguin")) {
      Str_Strcpy(distroShort, STR_OS_ULTRAPENGUIN, distroShortSize);
   } else if (strstr(distroLower, "united linux")) {
      Str_Strcpy(distroShort, STR_OS_UNITEDLINUX, distroShortSize);
   } else if (strstr(distroLower, "va linux")) {
      Str_Strcpy(distroShort, STR_OS_VALINUX, distroShortSize);
   } else if (strstr(distroLower, "yellow dog")) {
      Str_Strcpy(distroShort, STR_OS_YELLOW_DOG, distroShortSize);
   }

   free(distroLower);

}


/*
 *-----------------------------------------------------------------------------
 *
 * ReadDistroFile --
 *
 *      Look for a distro version file /etc/xxx-release.
 *      Once found, read the file in and figure out which distribution.
 *
 * Return value:
 *      Returns TRUE on success and FALSE on failure.
 *      Returns distro information verbatium from /etc/xxx-release (distro).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ReadDistroFile(char *filename,   // IN: distro version file name
               int distroSize,   // IN: size of the buffer allocated for OS distro name
               char *distro)     // OUT: full distro name
{
   int fd;
   int buf_sz;
   struct stat st;
   Bool ret = FALSE;
   char *distroOrig = NULL;
   char distroPart[DISTRO_BUF_SIZE];
   char *tmpDistroPos = NULL;
   int i = 0;

   if ((fd = open(filename, O_RDONLY)) < 0) {
      Debug("ReadDistroFile: could not open file%s: %d\n", filename, errno);
      return FALSE;
   }

   if (fstat(fd, &st)) {
      Debug("ReadDistroFile: could not stat the file %s: %d\n", filename, errno);
      goto out;
   }

   buf_sz = st.st_size;
   if (buf_sz >= distroSize) {
      Debug("ReadDistroFile: input buffer too small\n");
      goto out;
   }
   distroOrig = calloc(distroSize, sizeof *distroOrig);

   if (distroOrig == NULL) {
      Debug("ReadDistroFile: could not allocate memory\n");
      close(fd);
      return FALSE;
   }

   if (read(fd, distroOrig, buf_sz) != buf_sz) {
      Debug("ReadDistroFile: could not read file %s: %d\n", filename, errno);
      goto out;
   }

   distroOrig[buf_sz] = '\0';

   /*
    * For the case where we do have a release file in the LSB format,
    * but there is no LSB module, let's parse the LSB file for possible fields.
    */

   distro[0] = '\0';

   for (i = 0; lsbFields[i].name != NULL; i++) {

      tmpDistroPos = strstr(distroOrig, lsbFields[i].name);
      if (tmpDistroPos) {
         sscanf(tmpDistroPos, lsbFields[i].scanstring, distroPart);
         if (distroPart[0] == '"') {
            char *tmpMakeNull = NULL;

            tmpDistroPos += strlen(lsbFields[i].name) + 1;
            tmpMakeNull = strchr(tmpDistroPos + 1 , '"');
            if (tmpMakeNull) {
               *tmpMakeNull = '\0';
               Str_Strcat(distro, tmpDistroPos, distroSize);
               *tmpMakeNull = '"' ;
            }
         } else {
            Str_Strcat(distro, distroPart, distroSize);
         }
         Str_Strcat(distro, " ", distroSize);
      }
   }

   if (distro[0] == '\0') {
      /* Copy original string. What we got wasn't LSB compliant. */
      Str_Strcpy(distro, distroOrig, distroSize);
   }

   ret = TRUE;

out:
   close(fd);
   free(distroOrig);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoGetOSName --
 *
 *      Return OS version information. First retrieve OS information using
 *      uname, then look in /etc/xxx-release file to get the distro info.
 *      osFullName will be:
 *      <OS NAME> <OS RELEASE> <SPECIFIC_DISTRO_INFO>
 *      An example of such string would be:
 *      Linux 2.4.18-3 Red Hat Linux release 7.3 (Valhalla)
 *
 *      osName contains an os name in the same format that is used
 *      in .vmx file.
 *
 * Return value:
 *      Returns TRUE on success and FALSE on failure.
 *      Returns the guest's full OS name (osNameFull)
 *      Returns the guest's OS name in the same format as .vmx file (osName)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfoGetOSName(unsigned int outBufFullLen,      // IN: length of osNameFull buffer
                   unsigned int outBufLen,          // IN: length of osName buffer
                   char *osNameFull,                // OUT: Full OS name
                   char *osName)                    // OUT: OS name (.vmx file format)
{
   struct utsname buf;
   unsigned int lastCharPos;
   const char *lsbCmd = "lsb_release -sd 2>/dev/null";
   char *lsbOutput = NULL;

   /*
    * Use uname to get complete OS information.
    */

   if (uname(&buf) < 0) {
      Debug("GuestInfoGetOSName: uname failed %d\n", errno);
      return FALSE;
   }


   if (strlen(buf.sysname) + strlen(buf.release) + 3 > outBufFullLen) {
      Debug("GuestInfoGetOSName: Error: buffer too small\n");
      return FALSE;
   }

   Str_Strcpy(osName, STR_OS_EMPTY, outBufLen);
   Str_Strcpy(osNameFull, buf.sysname, outBufFullLen);
   Str_Strcat(osNameFull, STR_OS_EMPTY, outBufFullLen);
   Str_Strcat(osNameFull, buf.release, outBufFullLen);

   /*
    * Check to see if this is Linux
    * If yes, determine the distro by looking for /etc/xxx file.
    */

   if (strstr(osNameFull, "Linux")) {
      char distro[DISTRO_BUF_SIZE];
      char distroShort[DISTRO_BUF_SIZE];
      int i = 0;
      int distroSize = sizeof distro;

      /*
       * Write default distro string depending on
       * the kernel version.
       * If later we find more detailed information
       * this will get overwritten.
       */

      if (strstr(buf.release, "2.4")) {
         Str_Strcpy(distro, STR_OS_OTHER_24_FULL, distroSize);
         Str_Strcpy(distroShort, STR_OS_OTHER_24, distroSize);
      } else if (strstr(buf.release, "2.6")) {
         Str_Strcpy(distro, STR_OS_OTHER_26_FULL, distroSize);
         Str_Strcpy(distroShort, STR_OS_OTHER_26, distroSize);
      } else {
         Str_Strcpy(distro, STR_OS_OTHER_FULL, distroSize);
         Str_Strcpy(distroShort, STR_OS_OTHER, distroSize);
      }

      /*
       * Try to get OS detailed information
       * from the lsb_release command.
       */
      lsbOutput = GuestApp_GetCmdOutput(lsbCmd);

      if (!lsbOutput) {
         /*
          * Try to get more detailed information
          * from the version file.
          */

         for (i = 0; distroArray[i].filename != NULL; i++) {
            if (ReadDistroFile(distroArray[i].filename, distroSize, distro)) {
               break;
            }
         }

         /*
          * If we failed to read every distro file, exit now, before calling
          * strlen on the distro buffer (which wasn't set).
          */
         if (distroArray[i].filename == NULL) {
            Debug("GuestInfoGetOSName: Error: no distro file found\n");
            return FALSE;
         }
      } else {
         char *lsbStart = lsbOutput;
         char *quoteEnd = NULL;

         if (lsbStart[0] == '"') {
            lsbStart++;
            quoteEnd = strchr(lsbStart, '"');
            if (quoteEnd) {
               *quoteEnd = '\0';
            }
         }
         Str_Strcpy(distro, lsbStart, distroSize);
         free(lsbOutput);
      }

      GetShortName(distro, distroShort, distroSize);

      if (strlen(distro) + strlen(osNameFull) + 2 > outBufFullLen) {
         Debug("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Strcat(osNameFull, " ", outBufFullLen);
      Str_Strcat(osNameFull, distro, outBufFullLen);

      if (strlen(distroShort) + 1 > outBufLen) {
         Debug("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Strcpy(osName, distroShort, outBufLen);
   } else if (strstr(osNameFull, "FreeBSD")) {
      size_t nameLen = sizeof STR_OS_FREEBSD - 1;
      size_t releaseLen = 0;
      char *dashPtr;

      /*
       * FreeBSD releases report their version as "x.y-RELEASE". We'll be naive
       * look for the first dash, and use everything before it as the version
       * number.
       */
      dashPtr = Str_Strchr(buf.release, '-');
      if (dashPtr != NULL) {
         releaseLen = dashPtr - buf.release;
      }

      if (nameLen + releaseLen + 1 > outBufLen) {
         Debug("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Strcpy(osName, STR_OS_FREEBSD, outBufLen);
      if (releaseLen != 0) {
         Str_Strncat(osName, outBufLen, buf.release, releaseLen);
      }
   } else if (strstr(osNameFull, "SunOS")) {
      size_t nameLen = sizeof STR_OS_SOLARIS - 1;
      size_t releaseLen = 0;
      char *periodPtr;

      /*
       * Solaris releases report their version as "x.y". For our supported
       * releases it seems that x is always "5", and is ignored in favor of
       * y for the version number. We'll be naive and look for the first
       * period, and use the entire string after that as the version number.
       */
      periodPtr = Str_Strchr(buf.release, '.');
      if (periodPtr != NULL) {
         releaseLen = (buf.release + strlen(buf.release)) - periodPtr;
      }

      if (nameLen + releaseLen + 1 > outBufLen) {
         Debug("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Strcpy(osName, STR_OS_SOLARIS, outBufLen);
      if (releaseLen != 0) {
         Str_Strcat(osName, periodPtr + 1, outBufLen);
      }
   }

   if (GuestInfo_GetSystemBitness() == 64) {
      if (strlen(osName) + sizeof STR_OS_64BIT_SUFFIX > outBufLen) {
         Debug("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }
      Str_Strcat(osName, STR_OS_64BIT_SUFFIX, outBufLen);
   }

   /*
    * Before returning, truncate
    * the \n character at the end of the full name.
    */
   lastCharPos = strlen(osNameFull) - 1;
   if (osNameFull[lastCharPos] == '\n') {
      osNameFull[lastCharPos] = '\0';
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * GuestInfo_GetSystemBitness --
 *
 *      Determines the operating system's bitness.
 *
 * Return value:
 *      32 or 64 on success, negative value on failure. Check errno for more 
 *      details of error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
GuestInfo_GetSystemBitness(void)
{
   char buf[MAX_ARCH_NAME_LEN] = { 0 };
#if defined(__FreeBSD__)
   int mib[2];
   size_t len;

   len = sizeof buf;
   mib[0] = CTL_HW;
   mib[1] = HW_MACHINE;

   if (sysctl(mib, ARRAYSIZE(mib), buf, &len, NULL, 0) < 0) {
      return -1;
   }
#elif defined(sun)
# if !defined(SOL10)
   /*
    * XXX: This is bad.  We define SI_ARCHITECTURE_K to what it is on Solaris
    * 10 so that we can use a single guestd build for Solaris 9 and 10.  In the
    * future we should have the Solaris 9 build just return 32 -- since it did
    * not support 64-bit x86 -- and let the Solaris 10 headers define
    * SI_ARCHITECTURE_K, then have the installer symlink to the correct binary.
    * For now, though, we'll share a single build for both versions.
    */
#  define SI_ARCHITECTURE_K  518
# endif
   if (sysinfo(SI_ARCHITECTURE_K, buf, sizeof buf) < 0) {
      return -1;
   }
#elif defined(linux)
   struct utsname u;

   if (uname(&u) < 0) {
      return -1;
   }
   if (strstr(u.machine, "x86_64")) {
      return 64;
   } else {
      return 32;
   }
#endif

   if (strcmp(buf, SYSINFO_STRING_32) == 0) {
      return 32;
   } else if (strcmp(buf, SYSINFO_STRING_64) == 0) {
      return 64;
   }

   return -1;
}

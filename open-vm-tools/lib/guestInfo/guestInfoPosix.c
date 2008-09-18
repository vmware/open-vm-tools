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


#ifndef NO_DNET
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
      char ipAddress[NICINFO_MAX_IP_LEN];

      Str_Sprintf(macAddress, sizeof macAddress, "%s",
                  addr_ntoa(&entry->intf_link_addr));
      nic = GuestInfoAddNicEntry(nicInfo, macAddress);

      if (nic == NULL) {
         return -1;
      }

      if (entry->intf_addr.addr_type == ADDR_TYPE_IP) {
         VmIpAddress *ip = NULL;
         /* Use ip_ntop instead of addr_ntop since we don't want the netmask bits. */
         ip_ntop(&entry->intf_addr.addr_ip, ipAddress, sizeof ipAddress);
         ip = GuestInfoAddIpAddress(nic,
                                    ipAddress,
                                    INFO_IP_ADDRESS_FAMILY_IPV4);
         if (ip) {
            GuestInfoAddSubnetMask(ip, entry->intf_addr.addr_bits, TRUE);
         }
         /* Walk the list of alias's and add those that are IPV4 or IPV6 */
         for (i = 0; i < entry->intf_alias_num; i++) {
            if (entry->intf_alias_addrs[i].addr_type == ADDR_TYPE_IP) {
               ip_ntop(&entry->intf_alias_addrs[i].addr_ip,
                       ipAddress,
                       sizeof ipAddress);
               ip = GuestInfoAddIpAddress(nic,
                                          ipAddress,
                                          INFO_IP_ADDRESS_FAMILY_IPV4);
               if (ip) {
                  GuestInfoAddSubnetMask(ip, entry->intf_addr.addr_bits, TRUE);
               }
            } else if (entry->intf_alias_addrs[i].addr_type == ADDR_TYPE_IP6) {
               memcpy(ipAddress,
                      addr_ntoa(&entry->intf_alias_addrs[i]),
                      sizeof ipAddress);
               GuestInfoAddIpAddress(nic,
                                     ipAddress,
                                     INFO_IP_ADDRESS_FAMILY_IPV6);
            }
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
   } else if (strstr(distroLower, "mandriva")) {
      Str_Strcpy(distroShort, STR_OS_MANDRIVA, distroShortSize);
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
#if defined(__FreeBSD__) || defined(__APPLE__)
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

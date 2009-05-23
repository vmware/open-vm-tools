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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <fcntl.h>
#if defined(sun)
#include <sys/systeminfo.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/sysctl.h>
#endif

#include "vmware.h"
#include "hostinfo.h"
#include "safetime.h"
#include "str.h"
#include "guest_os.h"
#include "dynbuf.h"
#include "vmstdio.h"
#include "posix.h"

#define SYSTEM_BITNESS_32 "i386"
#define SYSTEM_BITNESS_64_SUN "amd64"
#define SYSTEM_BITNESS_64_LINUX "x86_64"
#define SYSTEM_BITNESS_MAXLEN \
   MAX(sizeof SYSTEM_BITNESS_32, \
   MAX(sizeof SYSTEM_BITNESS_64_SUN, \
       sizeof SYSTEM_BITNESS_64_LINUX))

static Bool hostinfoOSVersionInitialized;

#if defined(__APPLE__)
#define SYS_NMLN _SYS_NAMELEN
#endif
static int hostinfoOSVersion[3];
static char hostinfoOSVersionString[SYS_NMLN];

#define DISTRO_BUF_SIZE 255

typedef struct lsb_distro_info {
   char *name;
   char *scanstring;
} LSBDistroInfo;


static LSBDistroInfo lsbFields[] = {
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


static DistroInfo distroArray[] = {
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
 *----------------------------------------------------------------------
 *
 * HostinfoOSVersionInit --
 *
 *      Compute the OS version information
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      hostinfoOS* variables are filled in.
 *
 *----------------------------------------------------------------------
 */

static void
HostinfoOSVersionInit(void)
{
   struct utsname u;
   char extra[SYS_NMLN] = "";

   if (hostinfoOSVersionInitialized) {
      return;
   }

   if (uname(&u) < 0) {
      Warning("%s unable to get host OS version (uname): %s\n",
	      __FUNCTION__, strerror(errno));
      NOT_IMPLEMENTED();
   }

   Str_Strcpy(hostinfoOSVersionString, u.release, SYS_NMLN);

   ASSERT(ARRAYSIZE(hostinfoOSVersion) >= 3);
   if (sscanf(u.release, "%d.%d.%d%s",
	      &hostinfoOSVersion[0], &hostinfoOSVersion[1],
	      &hostinfoOSVersion[2], extra) < 1) {
      Warning("%s unable to parse host OS version string: %s\n",
              __FUNCTION__, u.release);
      NOT_IMPLEMENTED();
   }

   hostinfoOSVersionInitialized = TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSVersionString --
 *
 *	Returns the host version information as returned in the
 *      release field of uname(2)
 *
 * Results:
 *	const char * - pointer to static buffer containing the release
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

const char *
Hostinfo_OSVersionString(void)
{
   HostinfoOSVersionInit();

   return hostinfoOSVersionString;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_OSVersion --
 *
 *      Host OS release info.
 *
 * Results:
 *      The i-th component of a dotted release string.
 *	0 if i is greater than the number of components we support.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Hostinfo_OSVersion(int i)
{
   HostinfoOSVersionInit();

   return i < ARRAYSIZE(hostinfoOSVersion) ? hostinfoOSVersion[i] : 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Hostinfo_GetTimeOfDay --
 *
 *      Return the current time of day according to the host.  We want
 *      UTC time (seconds since Jan 1, 1970).
 *
 * Results:
 *      Time of day in microseconds.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void 
Hostinfo_GetTimeOfDay(VmTimeType *time)
{
   struct timeval tv;

   gettimeofday(&tv, NULL);

   *time = ((int64)tv.tv_sec * 1000000) + tv.tv_usec;
}


/*
 *----------------------------------------------------------------------------
 *
 * Hostinfo_GetSystemBitness --
 *
 *      Determines the operating system's bitness.
 *
 * Return value:
 *      32 or 64 on success.
 *      -1 on failure. Check errno for more details of error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
Hostinfo_GetSystemBitness(void)
{
#if defined __linux__
   struct utsname u;

   if (uname(&u) < 0) {
      return -1;
   }

   if (strstr(u.machine, SYSTEM_BITNESS_64_LINUX)) {
      return 64;
   } else {
      return 32;
   }
#elif defined N_PLAT_NLM
   return 32;
#else
   char buf[SYSTEM_BITNESS_MAXLEN] = { '\0', };
#   if defined __FreeBSD__ || defined __APPLE__
   static int mib[2] = { CTL_HW, HW_MACHINE, };
   size_t len = sizeof buf;

   if (sysctl(mib, ARRAYSIZE(mib), buf, &len, NULL, 0) == -1) {
      return -1;
   }
#   elif defined sun
#      if !defined SOL10
   /*
    * XXX: This is bad.  We define SI_ARCHITECTURE_K to what it is on Solaris
    * 10 so that we can use a single guestd build for Solaris 9 and 10.  In the
    * future we should have the Solaris 9 build just return 32 -- since it did
    * not support 64-bit x86 -- and let the Solaris 10 headers define
    * SI_ARCHITECTURE_K, then have the installer symlink to the correct binary.
    * For now, though, we'll share a single build for both versions.
    */
#         define SI_ARCHITECTURE_K  518
#      endif

   if (sysinfo(SI_ARCHITECTURE_K, buf, sizeof buf) < 0) {
      return -1;
   }
#   endif

   if (strcmp(buf, SYSTEM_BITNESS_32) == 0) {
      return 32;
   } else if (   strcmp(buf, SYSTEM_BITNESS_64_SUN) == 0
              || strcmp(buf, SYSTEM_BITNESS_64_LINUX) == 0) {
      return 64;
   }

   return -1;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HostinfoGetOSShortName --
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

static void
HostinfoGetOSShortName(char *distro,         // IN: full distro name
                       char *distroShort,    // OUT: short distro name
                       int distroShortSize)  // IN: size of short distro name

{
   char *distroLower = NULL;  /* Lower case distro name */

   distroLower = calloc(strlen(distro) + 1, sizeof *distroLower);

   if (distroLower == NULL) {
      Warning("%s: could not allocate memory\n", __FUNCTION__);
      return;
   }

   Str_Strcpy(distroLower, distro, distroShortSize);
   distroLower = Str_ToLower(distroLower);

   if (strstr(distroLower, "red hat")) {
      if (strstr(distroLower, "enterprise")) {

         /*
          * Looking for "release x" here instead of "x" as there could be
          * build version which can be misleading. For example Red Hat
          * Enterprise Linux ES release 4 (Nahant Update 3)
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
            Warning("%s: could not read Red Hat Enterprise release version\n",
                  __FUNCTION__);
            Str_Strcpy(distroShort, STR_OS_RED_HAT_EN, distroShortSize);
         }

      } else {
         Str_Strcpy(distroShort, STR_OS_RED_HAT, distroShortSize);
      }
   } else if (strstr(distroLower, "suse")) {
      if (strstr(distroLower, "enterprise")) {
         if (strstr(distroLower, "server 11")) {
            Str_Strcpy(distroShort, STR_OS_SLES_11, distroShortSize);
         } else if (strstr(distroLower, "server 10")) {
            Str_Strcpy(distroShort, STR_OS_SLES_10, distroShortSize);
         } else {
            Str_Strcpy(distroShort, STR_OS_SUSE_EN, distroShortSize);
         }
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
 * HostinfoReadDistroFile --
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
HostinfoReadDistroFile(char *filename,  // IN: distro version file name
                       int distroSize,  // IN: size of OS distro name buffer
                       char *distro)    // OUT: full distro name
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
      Warning("%s: could not open file%s: %d\n", __FUNCTION__, filename, errno);
      return FALSE;
   }

   if (fstat(fd, &st)) {
      Warning("%s: could not stat the file %s: %d\n", __FUNCTION__, filename,
           errno);
      goto out;
   }

   buf_sz = st.st_size;
   if (buf_sz >= distroSize) {
      Warning("%s: input buffer too small\n", __FUNCTION__);
      goto out;
   }
   distroOrig = calloc(distroSize, sizeof *distroOrig);

   if (distroOrig == NULL) {
      Warning("%s: could not allocate memory\n", __FUNCTION__);
      close(fd);
      return FALSE;
   }

   if (read(fd, distroOrig, buf_sz) != buf_sz) {
      Warning("%s: could not read file %s: %d\n", __FUNCTION__, filename, errno);
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
 *----------------------------------------------------------------------
 *
 * HostinfoGetCmdOutput --
 *
 *      Run a cmd & get its cmd line output
 *
 * Results:
 *      An allocated string or NULL if an error occurred.
 *
 * Side effects:
 *	The cmd is run.
 *
 *----------------------------------------------------------------------
 */

static char *
HostinfoGetCmdOutput(const char *cmd)  // IN:
{
#if defined(N_PLAT_NLM)
   Warning("Trying to execute command \"%s\" and catch its output... No way on NetWare...\n", cmd);
   return NULL;
#else
   DynBuf db;
   FILE *stream;
   char *out = NULL;

   DynBuf_Init(&db);

   stream = Posix_Popen(cmd, "r");
   if (stream == NULL) {
      Warning("Unable to get output of command \"%s\"\n", cmd);

      return NULL;
   }

   for (;;) {
      char *line = NULL;
      size_t size;

      switch (StdIO_ReadNextLine(stream, &line, 0, &size)) {
      case StdIO_Error:
         goto closeIt;
         break;

      case StdIO_EOF:
         break;

      case StdIO_Success:
         break;

      default:
         ASSERT_NOT_IMPLEMENTED(FALSE);
      }

      if (line == NULL) {
         break;
      }

      DynBuf_Append(&db, line, size);
      free(line);
   }

   if (DynBuf_Get(&db)) {
      out = (char *) DynBuf_AllocGet(&db);
   }
 closeIt:
   DynBuf_Destroy(&db);

   pclose(stream);

   return out;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Hostinfo_GetOSName --
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
Hostinfo_GetOSName(uint32 outBufFullLen,  // IN: length of osNameFull buffer
                   uint32 outBufLen,      // IN: length of osName buffer
                   char *osNameFull,      // OUT: Full OS name
                   char *osName)          // OUT: OS name (.vmx file format)
{
   struct utsname buf;
   unsigned int lastCharPos;
   const char *lsbCmd = "lsb_release -sd 2>/dev/null";
   char *lsbOutput = NULL;

   /*
    * Use uname to get complete OS information.
    */

   if (uname(&buf) < 0) {
      Warning("%s: uname failed %d\n", __FUNCTION__, errno);
      return FALSE;
   }


   if (strlen(buf.sysname) + strlen(buf.release) + 3 > outBufFullLen) {
      Warning("%s: Error: buffer too small\n", __FUNCTION__);
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
       * Write default distro string depending on the kernel version. If
       * later we find more detailed information this will get overwritten.
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

      lsbOutput = HostinfoGetCmdOutput(lsbCmd);

      if (!lsbOutput) {
         /*
          * Try to get more detailed information from the version file.
          */

         for (i = 0; distroArray[i].filename != NULL; i++) {
            if (HostinfoReadDistroFile(distroArray[i].filename, distroSize,
                                       distro)) {
               break;
            }
         }

         /*
          * If we failed to read every distro file, exit now, before calling
          * strlen on the distro buffer (which wasn't set).
          */

         if (distroArray[i].filename == NULL) {
            Warning("%s: Error: no distro file found\n", __FUNCTION__);
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

      HostinfoGetOSShortName(distro, distroShort, distroSize);

      if (strlen(distro) + strlen(osNameFull) + 2 > outBufFullLen) {
         Warning("%s: Error: buffer too small\n", __FUNCTION__);
         return FALSE;
      }

      Str_Strcat(osNameFull, " ", outBufFullLen);
      Str_Strcat(osNameFull, distro, outBufFullLen);

      if (strlen(distroShort) + 1 > outBufLen) {
         Warning("%s: Error: buffer too small\n", __FUNCTION__);
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
         Warning("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Strcpy(osName, STR_OS_FREEBSD, outBufLen);
   } else if (strstr(osNameFull, "SunOS")) {
      size_t nameLen = sizeof STR_OS_SOLARIS - 1;
      size_t releaseLen = 0;
      char solarisRelease[3] = "";

      /*
       * Solaris releases report their version as "x.y". For our supported
       * releases it seems that x is always "5", and is ignored in favor of
       * y for the version number.
       */

      if (sscanf(buf.release, "5.%2[0-9]", solarisRelease) == 1) {
         releaseLen = strlen(solarisRelease);
      }

      if (nameLen + releaseLen + 1 > outBufLen) {
         Warning("GuestInfoGetOSName: Error: buffer too small\n");
         return FALSE;
      }

      Str_Snprintf(osName, outBufLen, "%s%s", STR_OS_SOLARIS, solarisRelease);
   }

   if (Hostinfo_GetSystemBitness() == 64) {
      if (strlen(osName) + sizeof STR_OS_64BIT_SUFFIX > outBufLen) {
         Warning("%s: Error: buffer too small\n", __FUNCTION__);
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


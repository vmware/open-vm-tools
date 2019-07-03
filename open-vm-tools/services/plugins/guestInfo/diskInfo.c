/*********************************************************
 * Copyright (C) 2014-2019 VMware, Inc. All rights reserved.
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
 * @file diskInfo.c
 *
 *      Get disk information.
 */

#include <stdlib.h>
#include <string.h>
#include "vm_assert.h"
#include "debug.h"
#include "guestInfoInt.h"
#include "str.h"
#include "posix.h"
#include "file.h"
#include "util.h"
#include "wiper.h"

/*
 * TODO: A general reorganization of or removal of diskInfo.c is needed
 *
 * Presumably diskInfo.c is meant to contain routines common to both Windows
 * and *nix guests; but that has not been the case.  Up until the addition
 * of OS disk device mapping, this source file contained only two functions.
 *
 *  GuestInfo_FreeDiskInfo() -    4 line function called for Windows and *nix
 *                                to free the partitionList used to collect
 *                                disk information in the guest.
 *  GuestInfoGetDiskInfoWiper() - Function only called from diskInfoPosix.c
 *                                to use the disk wiper routines to query
 *                                "known" file systems on a *nix guest.
 *
 * As a result, the Windows guestInfo plugin has included the unreferenced
 * GuestInfoGetDiskInfoWiper() function in releases.
 *
 * As of this change, diskInfoWin32.c has its own copy of
 * GuestInfo_FreeDiskInfo() as that function has changed for *nix disk
 * info.  Only non-Windows tools builds will include diskInfo.c.
 *
 * As device-based disc mapping is implemented for FreeBSD, MacOS and
 * Solaris, diskInfo.c could be folded into diskInfoPosix.c.  Disk device
 * lookup could be separate modules based on OS type or implemented
 * with conditional compilation.  Both methods have been used elsewhere
 * in tools.
 *
 * PR 2350224 has been filed to track this TODO item.
 */

#if defined (__linux__)
#ifndef PATH_MAX
   # define PATH_MAX 1024
#endif

#define LINUX_SYS_BLOCK_DIR "/sys/class/block"

#define PCI_IDE         0x010100
#define PCI_SATA_AHCI_1 0x010601

#define PCI_SUBCLASS    0xFFFF00
#endif

#define COMP_STATIC_REGEX(gregex, mypattern, gerr, errorout)        \
   if (gregex == NULL) {                                            \
      gregex = g_regex_new(mypattern, 0, 0, &gerr);                 \
      if (gregex == NULL) {                                         \
         g_warning("%s: bad regex pattern \"" mypattern "\" (%s);"  \
                   " failing with INVALID_ARG\n",  __FUNCTION__,    \
                   gerr != NULL ? gerr->message : "");              \
         goto errorout;                                             \
      }                                                             \
   }



/*
 ******************************************************************************
 * GuestInfo_FreeDiskInfo --                                             */ /**
 *
 * @brief Frees memory allocated by GuestInfoGetDiskInfo.
 *
 * @param[in] di    DiskInfo container.
 *
 ******************************************************************************
 */

void
GuestInfo_FreeDiskInfo(GuestDiskInfoInt *di)
{
   if (di) {
      int indx;

      for (indx = 0; indx < di->numEntries; indx++) {
         free(di->partitionList[indx].diskDevNames);
      }
      free(di->partitionList);
      free(di);
   }
}


/*
 * Private library functions.
 */

#if defined (__linux__)

/*
 ******************************************************************************
 * GuestInfoAddDeviceName --                                             */ /**
 *
 * Add the disk device name into the array of anticipated devices for
 * the specified filesystem.
 *
 * @param[in]     devName    The device name being added.  May be an empty
 *                           string.
 * @param[in/out] partEntry  Pointer to the PartitionEntryInt structure to
 *                           receive the disk device names.
 * @param[in]     devNum     The number of the DiskDevName to be updated.
 *                           The diskDevNames is numbered  1, 2, ... as opposed
 *                           to array indexes which start at 0.
 *
 ******************************************************************************
 */

static void
GuestInfoAddDeviceName(char *devName,
                       PartitionEntryInt *partEntry,
                       int devNum)
{
   ASSERT(devName);
   ASSERT(partEntry);

   /* Add the device name to the device name array at the specified slot. */
   Str_ToLower(devName);
   if (devNum > partEntry->diskDevCnt) {
      partEntry->diskDevCnt = devNum;
      partEntry->diskDevNames = Util_SafeRealloc(partEntry->diskDevNames,
                                                 devNum *
                                                 sizeof *partEntry->diskDevNames);
   }
   Str_Strncpy(partEntry->diskDevNames[devNum - 1],
               sizeof *partEntry->diskDevNames,
               devName, strlen(devName));

   if (devName[0] == '\0') {
      g_debug("Empty disk device name in slot %d\n", devNum);
   }
}


/*
 ******************************************************************************
 * GuestInfoGetPCIName --                                                */ /**
 *
 * Extract the controller class and controller number from the "label" of
 * the specified PCI device.  Combine these with the previously determined
 * device or unit number.  The device name will be constructed in the format of
 * <class> <controller> : <unit> such as scsi0:0 or sata0:0.  The devName
 * will be left "blank" if unable to process the contents of the "label" file.
 *
 * @param[in]  pciDevPath  Path of the PCI device of interest.
 * @param[in]  unit        Disk unit or device number (previously determined).
 * @param[out] devName     Address of the buffer to receive device name.
 * @param[in]  devMaxLen   Maximum length of the device buffer available.
 *
 ******************************************************************************
 */

static void
GuestInfoGetPCIName(const char *pciDevPath,
                    const char *unit,
                    char *devName,
                    unsigned int devMaxLen)
{
   char labelPath[PATH_MAX];
   FILE *labelFile;
   char buffer[25];
   char *cPtr;

   Str_Snprintf(labelPath, PATH_MAX, "%s/%s", pciDevPath, "label");

   if ((labelFile = fopen(labelPath, "r")) == NULL) {
      g_debug("%s: unable to open \"label\" file for device %s.\n",
              __FUNCTION__, pciDevPath);
      return;
   }
   if (fgets(buffer, sizeof buffer, labelFile) == NULL) {
      g_debug("%s: unable to read \"label\" file for device %s.\n",
              __FUNCTION__, pciDevPath);
      goto exit;
   }

   /*
    * The "label" contents should already be in the form of SCSIn or satan.
    * A '\0' is stored after the last character in the buffer by fgets().
    * Check if the last character read is a new line and strip if found. */
   cPtr = &buffer[strlen(buffer) -1];
   if (*cPtr == '\n') {
      *cPtr = '\0';
   }
   Str_Snprintf (devName, devMaxLen, "%s:%s", buffer, unit);

exit:
   fclose(labelFile);
}


/*
 ******************************************************************************
 * GuestInfoGetIdeSataDev --                                             */ /**
 *
 * Determine the IDE controller or the SATA device number of the specified disk
 * device.
 *
 * @param[in]  tgtHostPath  Path of the PCI device of interest.
 * @param[in]  pciDevPath   Path of the PCI device of interest.
 *
 * @return   The unit number of the specified disk device.  A return value
 *           of -1 indicates that the unit number could not be determined.
 *
 ******************************************************************************
 */

static int
GuestInfoGetIdeSataDev(const char *tgtHostPath,
                       const char *pciDevPath)
{
   char *realPath = NULL;
   char **fileNameList = NULL;
   static GRegex *regexHostPath = NULL;
   static GRegex *regexHost = NULL;
   static GRegex *regexAtaPath = NULL;
   static GRegex *regexAta = NULL;
   GRegex *regexAtaOrHost;
   GError *gErr = NULL;
   GMatchInfo *matchInfo = NULL;
   char *charHost = NULL;
   int result = -1;
   int numFiles = 0;
   int number = 0;
   int fileNum;
   int host;

   /*
    * Depending on the Linux kernel version, the IDE device or SATA
    * controller notation may begin with "host" or "ata".  More recent
    * Linux releases are using "ata".
    */
   COMP_STATIC_REGEX(regexAtaPath, "^.*/ata(\\d+)$", gErr, exit)
   realPath = Posix_RealPath(tgtHostPath);
   if (g_regex_match(regexAtaPath, realPath, 0, &matchInfo)) {
      COMP_STATIC_REGEX(regexAta, "^ata(\\d+)$", gErr, exit)
      regexAtaOrHost = regexAta;

   } else {
      COMP_STATIC_REGEX(regexHostPath, "^.*/host(\\d+)$", gErr, exit)
      if (g_regex_match(regexHostPath, realPath, 0, &matchInfo)) {
         COMP_STATIC_REGEX(regexHost, "^host(\\d+)$", gErr, exit)
         regexAtaOrHost = regexHost;

      } else {
         g_debug("%s: Unable to locate IDE/SATA \"ata\" or \"host\" node "
                 "directory.\n", __FUNCTION__);
         goto exit;
      }
   }

   charHost = g_match_info_fetch(matchInfo, 1);
   if (sscanf(charHost, "%d", &host) != 1) {
      g_debug("%s: Unable to read host number.\n", __FUNCTION__);
      goto exit;
   }

   numFiles = File_ListDirectory(pciDevPath, &fileNameList);
   if (numFiles < 0) {
      g_debug("%s: Unable to list files in \"%s\" directory.\n",
              __FUNCTION__, pciDevPath);
   }
   for (fileNum = 0; fileNum < numFiles; fileNum++) {
      int currHost;

      if (g_regex_match(regexAtaOrHost, fileNameList[fileNum], 0, &matchInfo)) {
         g_free(charHost);
         charHost = g_match_info_fetch(matchInfo, 1);
         if (sscanf(charHost, "%d", &currHost) != 1) {
            g_debug("%s: Unable to read current host number.\n",
                    __FUNCTION__);
            goto exit;
         }
         if (currHost < host) {
            number++;
         }
      }
   }
   result = number;

exit:
   g_match_info_free(matchInfo);
   g_free(charHost);
   g_clear_error(&gErr);
   if (fileNameList != NULL) {
      Util_FreeStringList(fileNameList, numFiles);
   }
   free(realPath);
   return result;
}


/*
 ******************************************************************************
 * GuestInfoGetDevClass --                                               */ /**
 *
 * Locate and extract the value from the "class" file of the specified disk
 * device.  This file is typically located in the directory provided by the
 * parameter "pciDevPath".
 *
 * An IDE device layout change that occurred between 2.x and 3.x Linux kernels
 * may require a check in the parent directory.  If that is necessary, the
 * "pciDevPath" and "devPath" will be updated to reflect that difference.
 *
 * @param[in/out]  pciDevPath  Path to the IDE, SCSI or SAS disk device of
 *                             interest; expecting "class" file.
 * @param[in/out]  devPath     Associated directory which will need to be
 *                             adjusted if pciDevPath is altered.
 *
 * @return     The disk device class value.
 *
 ******************************************************************************
 */

static unsigned int
GuestInfoGetDevClass(char *pciDevPath,
                     char *devPath)
{
   char devClassPath[PATH_MAX];
   FILE *devClass = NULL;
   unsigned int classValue = 0;

   ASSERT(pciDevPath);
   Str_Snprintf(devClassPath, PATH_MAX, "%s/%s", pciDevPath, "class");
   if (!File_Exists(devClassPath)) {
      /* Check if "class" exists in the parent directory. */
      Str_Snprintf(devClassPath, PATH_MAX, "%s/../%s", pciDevPath, "class");
      if (File_Exists(devClassPath)) {
         /* "class" found in parent directory; probably IDE/SATA device.*/
         Str_Strcat(pciDevPath, "/..", PATH_MAX);
         Str_Strcat(devPath, "/..", PATH_MAX);
      } else {
         g_debug("%s: Unable to locate device 'class' file.\n", __FUNCTION__);
         goto exit;
      }
   }

   devClass = fopen((const char *)devClassPath, "r");
   if (devClass == NULL) {
      g_debug("%s: Error opening device 'class' file.\n", __FUNCTION__);
      goto exit;
   }
   if (fscanf(devClass, "%x", &classValue) != 1) {
      classValue = 0;
      g_debug("%s: Unable to read expected hex class setting.\n", __FUNCTION__);
   }
   fclose(devClass);

exit:
   return classValue;
}


/*
 ******************************************************************************
 * GuestInfoCheckSASDevice --                                            */ /**
 *
 * Check if the referenced disk device is a SAS device and if so, recalulate
 * the device (unit) number and update the paths as needed to continue
 * processing a SAS device.
 *
 * @param[in/out] pciDevPath  Path of the disk device to be checked.
 * @param[out]    tgtHostPath Address of the "host" path to be updated if this
 *                            is a SAS device.
 * @param[out]    unit        Pointer to the address of the unit buffer to be
 *                            updated if this is a SAS device.
 *
 ******************************************************************************
 */

static void
GuestInfoCheckSASDevice(char *pciDevPath,
                        char *tgtHostPath,
                        char **unit)
{
   char sas_portPath[PATH_MAX];
   char **fileNameList = NULL;
   static GRegex *regexSas = NULL;
   GError *gErr = NULL;
   GMatchInfo *matchInfo = NULL;
   int numFiles = 0;
   int fileNum;

   Str_Snprintf(sas_portPath, PATH_MAX, "%s/%s", pciDevPath, "sas_port");
   if (!File_IsDirectory(sas_portPath)) {
      return;
   }
   g_debug("%s: located a \"sas_port\" directory - %s.\n", __FUNCTION__,
           sas_portPath);

   /* Expecting to find a new "unit" number; scribble over old value.. */
   **unit = '?';
   COMP_STATIC_REGEX(regexSas, "^phy-\\d+:(\\d+)$", gErr, exit)

   numFiles = File_ListDirectory(pciDevPath, &fileNameList);
   if (numFiles < 0) {
      g_debug("%s: Unable to list files in \"%s\" directory.\n", __FUNCTION__,
              pciDevPath);
   }
   for (fileNum = 0; fileNum < numFiles; fileNum++) {
      if (g_regex_match(regexSas, fileNameList[fileNum], 0, &matchInfo)) {
         free(*unit);     /* free previous "unit" string */
         *unit = g_match_info_fetch(matchInfo, 1);
         break;
      }
   }

   /*
    * Adjust the tgtHostPath and pciDevPath for continued processing of
    * a SAS disk device.
    */
   Str_Snprintf(tgtHostPath, PATH_MAX, "%s/..", pciDevPath);
   Str_Snprintf(pciDevPath, PATH_MAX, "%s/..", tgtHostPath);

exit:
   g_match_info_free(matchInfo);
   g_clear_error(&gErr);
   if (fileNameList != NULL) {
      Util_FreeStringList(fileNameList, numFiles);
   }
}


/*
 ******************************************************************************
 * GuestInfoNvmeDevice --                                                */ /**
 *
 * Extract the NVMe disk unit number for the specified disk device.  In
 * Linux kernels 3.x and later, the NVME device is contained in the "nsid"
 * file in the sysfs filesystem.  On earlier kernels such as 2.6, the NVME
 * device number is the last number group in the node name.
 *
 * NVMe devices are number 1 and up; the "physical" disk unit numbering
 * starts at zero.
 *
 *  @param[in]    devPath    Path of the "device" file for this NVMe device.
 *                           The "nsid" file, if it exists, will be in the
 *                           same directory.
 * @param[out]    pciDevPath Path to be updated with the directory containing
 *                           the device "label" file.   This path will also
 *                           contain the device "class" file.
 * @param[out]    unit       Pointer to receive the address of the disk device
 *                           number dereived from the contents of the "nsid"
 *                           file or from the node name in 2.6 Linux kernels.
 *
 * @return TRUE if the NVMe disk device number has been determined and the
 *         device number is passed back as a character string through "unit".
 *         The "pciDevPath will be updated (replaced) with the directory which
 *         should contain the device "label" file.
 *
 ******************************************************************************
 */

static Bool
GuestInfoNvmeDevice(const char *devPath,
                    char *pciDevPath,
                    char **unit)
{
   char nsidPath[PATH_MAX];
   FILE *devNsid = NULL;
   int dirPathLen;
   int nsid;

   ASSERT(devPath);
   ASSERT(pciDevPath);
   dirPathLen = strrchr(devPath, '/') - devPath;
   Str_Strncpy(nsidPath, sizeof nsidPath, devPath, dirPathLen);
   Str_Strcat(nsidPath, "/nsid", PATH_MAX);
   if (File_Exists(nsidPath)) {
      devNsid = fopen((const char *)nsidPath, "r");
      if (devNsid == NULL) {
         g_debug("%s: Error opening NVMe device \"nsid\" file.\n",
                 __FUNCTION__);
         return FALSE;
      }
      if (fscanf(devNsid, "%d", &nsid) != 1) {
         g_debug("%s: Unable to read the nsid device number.\n", __FUNCTION__);
         fclose(devNsid);
         return FALSE;
      }
      fclose(devNsid);
   } else {
      static GRegex *regexNvmeNode = NULL;
      GError *gErr = NULL;
      GMatchInfo *matchInfo = NULL;
      char *nsidStr = NULL;
      char * realPath;
      Bool match_result = FALSE;

      /*
       * Earlier NVMe kernel implementation; no "nsid" file available.
       * The nsid equivalent can be derived from the last number in the
       * directory containing the "device" symbolic link.  Need real path
       * name.
       */
      Str_Strncpy(nsidPath, sizeof nsidPath, devPath, dirPathLen);
      realPath = Posix_RealPath(nsidPath);

      /* Check for NVMe device. */
      COMP_STATIC_REGEX(regexNvmeNode, "^.*/nvme\\d+n(\\d+)$", gErr, finished)

      if (!g_regex_match(regexNvmeNode, realPath, 0, &matchInfo)) {
         goto finished;
      }
      nsidStr = g_match_info_fetch(matchInfo, 1);
      nsid = atoi(nsidStr);
      match_result = TRUE;

finished:
      /* Some clean-up for this block before possibly returning FALSE. */
      g_match_info_free(matchInfo);
      g_clear_error(&gErr);
      g_free(nsidStr);
      free(realPath);

      if (!match_result) {
         return FALSE;
      }
   }

   /* NVMe device numbers start at 1; hardware device numbers start at zero. */
   *unit = g_strdup_printf("%d", nsid - 1);

   /* Set the pciDevPath to the directory containing the "label" file, */
   Str_Snprintf(pciDevPath, PATH_MAX, "%s/%s", devPath, "../..");
   return TRUE;
}


/*
 ******************************************************************************
 * GuestInfoLinuxBlockDevice --                                          */ /**
 *
 * Determine if this is a block device and if so add the disk device name
 * to the specified PartitionEntryInt structure.  If the startDevPath
 * represents a full disk, i.e. no partition table on the disk, the
 * "device" file will be in the starting directory.  For filesystems
 * created on a disk partition, the "device" file will be in the parent node.
 *
 * @param[in]     startPath  Starting path to begin the search for the "device"
 *                           file.
 * @param[in/out] partEntry  Pointer to the PartitionEntryInt structure to
 *                           receive the disk device names.
 * @param[in]     devNum     The number of the DiskDevName to be updated.
 *                           The diskDevNames is numbered  1, 2, ... as opposed
 *                           to array indexes which start at 0.
 *
 ******************************************************************************
 */

static void
GuestInfoLinuxBlockDevice(const char *startPath,
                          PartitionEntryInt *partEntry,
                          int devNum)
{
   char devPath[PATH_MAX];
   char pciDevPath[PATH_MAX];
   char *realPath = NULL;
   static GRegex *regex = NULL;
   static GRegex *regexNvme = NULL;
   GError *gErr = NULL;
   GMatchInfo *matchInfo = NULL;
   char *unit = NULL;
   unsigned int devClass;
   char devName[DISK_DEVICE_NAME_SIZE];

   ASSERT(startPath);
   ASSERT(partEntry);
   ASSERT(devNum > 0);
   devName[0] = '\0';   /* Empty string for the device name until determined. */
   g_debug("%s: looking up device for file system on \"%s\"\n", __FUNCTION__,
           startPath);

   /* Check for "device" file in the starting path provided. */
   Str_Snprintf(devPath, PATH_MAX,  "%s/device", startPath);
   if (!File_Exists(devPath)) {
      /*
       * If working with a filesystem on a disk partition, we will need
       * to check in the parent node.
       */
      Str_Snprintf(devPath, PATH_MAX,  "%s/../device", startPath);
      if (!File_Exists(devPath)) {
         goto finished;
      }
   }

   realPath = Posix_RealPath(devPath);
   COMP_STATIC_REGEX(regex, "^.*/\\d+:\\d+:(\\d+):\\d+$", gErr, finished)

   if (g_regex_match(regex, realPath, 0, &matchInfo)) {
      unit = g_match_info_fetch(matchInfo, 1);

      Str_Strcat(devPath, "/../..", PATH_MAX);
      Str_Snprintf(pciDevPath, PATH_MAX, "%s/%s", devPath, "..");
      /*
       * Check if this is a SAS device.  The contents of "unit", "devPath" and
       * "pciDevPath" will be altered if a SAS device is detected..
       */
      GuestInfoCheckSASDevice(pciDevPath, devPath, &unit);

      /* Getting the disk device class. */
      devClass = GuestInfoGetDevClass(pciDevPath, devPath);

      /*
       * IDE and SATA devices need different handling.
       */
      if ((devClass & PCI_SUBCLASS) == PCI_IDE || devClass == PCI_SATA_AHCI_1) {
         int cnt;

         cnt = GuestInfoGetIdeSataDev(devPath, pciDevPath);
         if (cnt < 0) {
            g_debug("%s: ERROR, unable to determine IDE controller or SATA "
                    "device.\n", __FUNCTION__);
            goto finished;
         }
         if ((devClass & PCI_SUBCLASS) == PCI_IDE) {
            /* IDE - full device representation can be constructed. */
            Str_Snprintf(devName, sizeof devName, "ide%d:%s", cnt, unit);
         } else {
            /* SATA - The "host cnt" obtained becomes the "unit" number. */
            g_free(unit);
            unit = g_strdup_printf("%d", cnt);
         }
      }
   } else {
      /* Check for NVMe device. */
      COMP_STATIC_REGEX(regexNvme, "^.*/nvme\\d+$", gErr, finished)

      if (!g_regex_match(regexNvme, realPath, 0, &matchInfo)) {
         g_debug("%s: block disk device pattern not found\n", __FUNCTION__);
         goto finished;
      }
      if (!GuestInfoNvmeDevice(devPath, pciDevPath, &unit)) {
         g_debug("%s: NVMe disk device could not be determined.\n",
                 __FUNCTION__);
         goto finished;
      }
   }

   /* At this point only IDE disks would have a completed device name. */
   if (devName[0] == '\0') {
      /* Access the PCI device "label" file for the controller name. */
      GuestInfoGetPCIName(pciDevPath, unit, devName, sizeof devName);
   }

finished:
   /*
    * Add the device name, whether found or not, to the device list for
    * this mounted file system.  After processing the single partition of
    * a file system mounted on a block device or all the slave devices of
    * an LVM, the presence of a zero-length name indicates not all devices
    * have been correctly determined.
    */
   GuestInfoAddDeviceName(devName, partEntry, devNum);

   g_match_info_free(matchInfo);
   g_clear_error(&gErr);
   g_free(unit);
   free(realPath);
   g_debug("%s: Filesystem of interest found on device \"%s\"\n",
          __FUNCTION__, devName[0] == '\0' ? "** unknown **" : devName);
}


/*
 ******************************************************************************
 * GuestInfoIsLinuxLvmDevice --                                          */ /**
 *
 * Determine if the fsName is a Linux LVM and if so, determine the disk
 * device or devices associated with this LVM based filesystem.  If for
 * any reason the full set of LVM "slaves" cannot be determined, an
 * incomplete list of disk device names will not be provided.
 *
 * @param[in]     fsName     Name of the block device or LVM mapper name of
 *                           the filesystem of interest.
 * @param[in/out] partEntry  Pointer to the PartitionEntryInt structure to
 *                           receive the disk device names.
 * @return TRUE if the "slaves" directory has been located.  It does not
 *         indicate that LVM processing was successful, only that this
 *         does appear to be an LVM filesystem.  If the disk device names
 *         for all slaves cannot be located, partial disk mapping is not
 *         reported.
 *
 ******************************************************************************
 */

static Bool
GuestInfoIsLinuxLvmDevice(const char *fsName,
                          PartitionEntryInt *partEntry)
{
   char *realPath;
   char **fileNameList = NULL;
   int numFiles = 0;
   int devIndx;
   char slavesPath[PATH_MAX];
   char devPath[PATH_MAX];

   /*
    * If a logical volume, the fsName will be a symbolic link to the
    * /dev/dm-<n>.  Use the "dm-<n>" name to access the logical volume
    * entry in the sysfs/device manager (/sys/class/block).
    */
   if ((realPath = Posix_RealPath(fsName)) == NULL) {
      return FALSE;
   }
   Str_Snprintf(slavesPath, PATH_MAX, "%s/%s/slaves", LINUX_SYS_BLOCK_DIR,
                strrchr(realPath, '/') + 1);
   free(realPath);
   if (!File_IsDirectory(slavesPath)) {
      return FALSE;
   }
   numFiles = File_ListDirectory(slavesPath, &fileNameList);
   if (numFiles == 0) {
      /* An empty "slaves" directory happens at a disk device node; this
       * certainly is not an LVM.
       */
      return FALSE;
   }
   if (numFiles < 0) {
      g_debug("%s: Unable to list entries in \"%s\" directory.\n", __FUNCTION__,
              slavesPath);
      return TRUE;
   }

   /* Create a device name entry for each slave device found. */
   partEntry->diskDevCnt = numFiles;
   partEntry->diskDevNames = Util_SafeRealloc(partEntry->diskDevNames,
                                              numFiles *
                                              sizeof *partEntry->diskDevNames);

   for (devIndx = 0; devIndx < numFiles; devIndx++) {
      /*
       * Each slave device will be based on the disk or disk partition of
       * a virtual disk device.  Start the block device search from the
       * "slaves" path.
       */
      Str_Snprintf(devPath, PATH_MAX, "%s/%s", slavesPath,
                   fileNameList[devIndx]);
      GuestInfoLinuxBlockDevice(devPath, partEntry, devIndx + 1);
   }

   if (fileNameList != NULL) {
      Util_FreeStringList(fileNameList, numFiles);
   }
   return TRUE;
}

#endif /* __linux__ */

/*
 ******************************************************************************
 * GuestInfoGetDiskDevice --                                             */ /**
 *
 * Determine the OS disk device for the block device or the disk devices of
 * the logical volume mapper name provided.
 *
 * @param[in]     fsName     Name of the block device or LVM mapper name of
 *                           the filesystem of interest.
 * @param[in/out] partEntry  Pointer to the PartitionEntryInt structure to
 *                           receive the disk device names.
 *
 * Currently only processing disks on a Linux guest.
 *
 * TODO: Other available controllers and their importance on vSphere hosted
 *       guests were discussed in review board posting:
 *          https://reviewboard.eng.vmware.com/r/1520060/
 *       PR 2356195 has been filed to track these issues post 11.0.0 FC.
 ******************************************************************************
 */

static void
GuestInfoGetDiskDevice(const char *fsName,
                       PartitionEntryInt *partEntry)
{
#if defined (__linux__)
   int indx;
#endif /* __linux__ */

   ASSERT(fsName);
   ASSERT(partEntry);
   g_debug("%s: looking up device(s) for file system on \"%s\".\n",
           __FUNCTION__, fsName);

#if defined (__linux__)
   /*
    * Determine if this is a filesystem on a block device or on a logical
    * volume (such as /dev/mapper/...).
    */

   /* Check first for an LVM filesystem. */
   if (!GuestInfoIsLinuxLvmDevice(fsName, partEntry)) {

      /* Not an LVM; check if a basic block device. */
      char blockDevPath[PATH_MAX];

      Str_Snprintf(blockDevPath, PATH_MAX,  "%s/%s", LINUX_SYS_BLOCK_DIR,
                   strrchr(fsName, '/') + 1);
      GuestInfoLinuxBlockDevice(blockDevPath, partEntry, 1 /* first and only*/);
   }

   /*
    * Check that all expected devices have been found.  If not, reset the
    * device count to zero so that partial disk mapping will not happen.
    */
   for (indx = 0; indx < partEntry->diskDevCnt; indx++) {
      if (partEntry->diskDevNames[indx][0] == '\0') {
         g_warning("%s: Missing disk device name; VMDK mapping unavailable "
                   "for \"%s\", fsName: \"%s\"\n", __FUNCTION__,
                   partEntry->name, fsName);
         partEntry->diskDevCnt = 0;
         free(partEntry->diskDevNames);
         partEntry->diskDevNames = NULL;
         break;
      }
   }
#endif /* __linux__ */

   g_debug("%s: found %d devices(s) for file system on \"%s\".\n",
           __FUNCTION__, partEntry->diskDevCnt, fsName);
}


/*
 ******************************************************************************
 * GuestInfoGetDiskInfoWiper --                                          */ /**
 *
 * Uses wiper library to enumerate fixed volumes and lookup utilization data.
 *
 * @return Pointer to a GuestDiskInfoInt structure on success or NULL on failure.
 *         Caller should free returned pointer with GuestInfo_FreeDiskInfo.
 *
 ******************************************************************************
 */

GuestDiskInfoInt *
GuestInfoGetDiskInfoWiper(Bool includeReserved,  // IN
                          Bool reportDevices)    // IN
{
   WiperPartition_List pl;
   DblLnkLst_Links *curr;
   unsigned int partCount = 0;
   uint64 freeBytes = 0;
   uint64 totalBytes = 0;
   unsigned int partNameSize = 0;
   Bool success = FALSE;
   GuestDiskInfoInt *di;

   /* Get partition list. */
   if (!WiperPartition_Open(&pl, FALSE)) {
      g_warning("GetDiskInfo: ERROR: could not get partition list\n");
      return FALSE;
   }

   di = Util_SafeCalloc(1, sizeof *di);
   partNameSize = sizeof (di->partitionList)[0].name;

   DblLnkLst_ForEach(curr, &pl.link) {
      WiperPartition *part = DblLnkLst_Container(curr, WiperPartition, link);

      if (part->type != PARTITION_UNSUPPORTED) {
         PartitionEntryInt *newPartitionList;
         PartitionEntryInt *partEntry;
         unsigned char *error;
         if (includeReserved) {
            error = WiperSinglePartition_GetSpace(part, NULL,
                                                  &freeBytes, &totalBytes);
         } else {
            error = WiperSinglePartition_GetSpace(part, &freeBytes,
                                                  NULL, &totalBytes);
         }
         if (strlen(error)) {
            g_warning("GetDiskInfo: ERROR: could not get space info for "
                      "partition %s: %s\n", part->mountPoint, error);
            goto out;
         }

         if (strlen(part->mountPoint) + 1 > partNameSize) {
            g_debug("GetDiskInfo: Partition name '%s' too large, truncating\n",
                    part->mountPoint);
         }

         newPartitionList = Util_SafeRealloc(di->partitionList,
                                             (partCount + 1) *
                                             sizeof *di->partitionList);

         partEntry = &newPartitionList[partCount++];
         Str_Strncpy(partEntry->name, partNameSize,
                     part->mountPoint, partNameSize - 1);
         partEntry->freeBytes = freeBytes;
         partEntry->totalBytes = totalBytes;
         Str_Strncpy(partEntry->fsType, sizeof (di->partitionList)[0].fsType,
                     part->fsType, strlen(part->fsType));

         /* Start with an empty set of disk device names. */
         partEntry->diskDevCnt = 0;
         partEntry->diskDevNames = NULL;

         if (reportDevices) {
            GuestInfoGetDiskDevice(part->fsName, partEntry);
         }

         di->partitionList = newPartitionList;
         g_debug("%s added partition #%d %s type %d fstype %s (mount point %s) "
                 "free %"FMT64"u total %"FMT64"u\n",
                 __FUNCTION__, partCount, partEntry->name, part->type,
                 partEntry->fsType, part->fsName,
                 partEntry->freeBytes, partEntry->totalBytes);
      } else {
         g_debug("%s ignoring unsupported partition %s %s\n",
                 __FUNCTION__, part->mountPoint,
                 part->comment ? part->comment : "");
      }
   }

   di->numEntries = partCount;
   success = TRUE;

out:
   if (!success) {
      GuestInfo_FreeDiskInfo(di);
      di = NULL;
   }
   WiperPartition_Close(&pl);
   return di;
}

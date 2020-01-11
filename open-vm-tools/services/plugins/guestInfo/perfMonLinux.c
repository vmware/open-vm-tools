/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "vm_basic_defs.h"
#include "vmware.h"
#include "str.h"
#include "strutil.h"
#include "debug.h"
#include "guestInfoInt.h"
#include "guestStats.h"
#include "posix.h"
#include "hashTable.h"
#include "conf.h"

#define GUEST_INFO_PREALLOC_SIZE 4096
#define INT_AS_HASHKEY(x) ((const void *)(uintptr_t)(x))
#define KEY_FORMAT  "%s|%s"

#define STAT_FILE        "/proc/stat"
#define VMSTAT_FILE      "/proc/vmstat"
#define UPTIME_FILE      "/proc/uptime"
#define MEMINFO_FILE     "/proc/meminfo"
#define ZONEINFO_FILE    "/proc/zoneinfo"
#define SWAPPINESS_FILE  "/proc/sys/vm/swappiness"
#define DISKSTATS_FILE   "/proc/diskstats"

#define SYSFS_BLOCK_FOLDER  "/sys/block"

/*
 * For now, all data collection is of uint64 values. Rates are always returned
 * as a double, derived from the uint64 data.
 */

static Bool gReleased = TRUE;
static Bool gInternal = FALSE;
#if PUBLISH_EXPERIMENTAL_STATS
static Bool gExperimental = PUBLISH_EXPERIMENTAL_STATS;
#endif
#if ADD_NEW_STATS
static Bool gUnstable = FALSE;
#endif

#define DECLARE_STAT(publish, file, isRegExp, locatorString, reportID, units, dataType) \
   { file, publish, isRegExp, locatorString, reportID, units, dataType }

typedef struct {
   const char         *sourceFile;
   const Bool         *publish;
   Bool                isRegExp;
   const char         *locatorString;
   GuestStatToolsID    reportID;
   GuestValueUnits     units;
   GuestValueType      dataType;
} GuestInfoQuery;

GuestInfoQuery guestInfoQuerySpecTable[] = {
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "Hugepagesize",    GuestStatID_HugePageSize,              GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  ZONEINFO_FILE,   TRUE,  "present",         GuestStatID_MemPhysUsable,             GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "MemFree",         GuestStatID_MemFree,                   GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "Active(file)",    GuestStatID_MemActiveFileCache,        GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "SwapFree",        GuestStatID_SwapSpaceRemaining,        GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "HugePages_Total", GuestStatID_Linux_HugePagesTotal,      GuestUnitsHugePages,       GuestTypeUint64),
   DECLARE_STAT(&gReleased,  VMSTAT_FILE,     FALSE, "pgpgin",          GuestStatID_PageInRate,                GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gReleased,  VMSTAT_FILE,     FALSE, "pgpgout",         GuestStatID_PageOutRate,               GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gReleased,  STAT_FILE,       FALSE, "ctxt",            GuestStatID_ContextSwapRate,           GuestUnitsNumberPerSecond, GuestTypeDouble),
   DECLARE_STAT(&gReleased,  NULL,            FALSE, NULL,              GuestStatID_PhysicalPageSize,          GuestUnitsBytes,           GuestTypeUint64),
   DECLARE_STAT(&gReleased,  NULL,            FALSE, NULL,              GuestStatID_MemNeeded,                 GuestUnitsKiB,             GuestTypeUint64),

   DECLARE_STAT(&gReleased,  NULL,            FALSE, NULL,              GuestStatID_MemNeededReservation,      GuestUnitsKiB,             GuestTypeUint64),

   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "MemAvailable",    GuestStatID_Linux_MemAvailable,        GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "SReclaimable",    GuestStatID_Linux_MemSlabReclaim,      GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "Buffers",         GuestStatID_Linux_MemBuffers,          GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "Cached",          GuestStatID_Linux_MemCached,           GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gReleased,  MEMINFO_FILE,    FALSE, "MemTotal",        GuestStatID_Linux_MemTotal,            GuestUnitsKiB,             GuestTypeUint64),

   DECLARE_STAT(&gInternal,  MEMINFO_FILE,    FALSE, "Inactive(file)",  GuestStatID_Linux_MemInactiveFile,     GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gInternal,  ZONEINFO_FILE,   TRUE,  "low",             GuestStatID_Linux_LowWaterMark,        GuestUnitsPages,           GuestTypeUint64),

#if PUBLISH_EXPERIMENTAL_STATS
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "SwapTotal",       GuestStatID_SwapFilesCurrent,          GuestUnitsKiB,             GuestTypeUint64),
   /* GuestStatID_SwapSpaceUsed depends on GuestStatID_SwapFilesCurrent */
   DECLARE_STAT(&gExperimental,  NULL,            FALSE, NULL,              GuestStatID_SwapSpaceUsed,             GuestUnitsKiB,             GuestTypeUint64),
   /* GuestStatID_SwapFilesMax depends on GuestStatID_SwapFilesCurrent */
   DECLARE_STAT(&gExperimental,  NULL,            FALSE, NULL,              GuestStatID_SwapFilesMax,              GuestUnitsKiB,             GuestTypeUint64),

   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Active(anon)",    GuestStatID_Linux_MemActiveAnon,       GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Inactive(anon)",  GuestStatID_Linux_MemInactiveAnon,     GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Inactive",        GuestStatID_Linux_MemInactive,         GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Active",          GuestStatID_Linux_MemActive,           GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Unevictable",     GuestStatID_Linux_MemPinned,           GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Dirty",           GuestStatID_Linux_MemDirty,            GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     FALSE, "pswpin",          GuestStatID_PageSwapInRate,            GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     FALSE, "pswpout",         GuestStatID_PageSwapOutRate,           GuestUnitsPagesPerSecond,  GuestTypeDouble),
   /* Not implemented
   DECLARE_STAT(&gExperimental,  NULL,            FALSE, NULL,              GuestStatID_ThreadCreationRate,        GuestUnitsNumberPerSecond, GuestTypeDouble),
    */
   DECLARE_STAT(&gExperimental,  SWAPPINESS_FILE, FALSE, NULL,              GuestStatID_Linux_Swappiness,          GuestUnitsPercent,         GuestTypeUint64),

   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "SwapCached",      GuestStatID_Linux_MemSwapCached,       GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "Committed_AS",    GuestStatID_Linux_MemCommitted,        GuestUnitsKiB,             GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  MEMINFO_FILE,    FALSE, "HugePages_Free",  GuestStatID_Linux_HugePagesFree,       GuestUnitsHugePages,       GuestTypeUint64),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     FALSE, "pgfault",         GuestStatID_Linux_PageFaultRate,       GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     FALSE, "pgmajfault",      GuestStatID_Linux_PageMajorFaultRate,  GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     FALSE, "pgfree",          GuestStatID_Linux_PageFreeRate,        GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     TRUE,  "pgsteal_",        GuestStatID_Linux_PageStealRate,       GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     TRUE,  "pgscan_kswapd_",  GuestStatID_Linux_PageSwapScanRate,    GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  VMSTAT_FILE,     TRUE,  "pgscan_direct_",  GuestStatID_Linux_PageDirectScanRate,  GuestUnitsPagesPerSecond,  GuestTypeDouble),
   DECLARE_STAT(&gExperimental,  STAT_FILE,       FALSE, "processes",       GuestStatID_ProcessCreationRate,       GuestUnitsNumberPerSecond, GuestTypeDouble),
#endif

   DECLARE_STAT(&gReleased,  STAT_FILE,       FALSE, "procs_running",   GuestStatID_Linux_CpuRunQueue,         GuestUnitsNumber,          GuestTypeUint64),
   DECLARE_STAT(&gReleased,  NULL,            FALSE, NULL,              GuestStatID_Linux_DiskRequestQueue,    GuestUnitsNumber,          GuestTypeUint64),
   DECLARE_STAT(&gReleased,  NULL,            FALSE, NULL,              GuestStatID_Linux_DiskRequestQueueAvg, GuestUnitsNumber,          GuestTypeDouble),
};

#define N_QUERIES (sizeof guestInfoQuerySpecTable / sizeof(GuestInfoQuery))

typedef struct {
   int              err;    // An errno value
   uint32           count;  // Number of instances found
   uint64           value;
   GuestInfoQuery  *query;
} GuestInfoStat;

typedef struct {
   HashTable       *exactMatches;

   uint32           numRegExps;
   GuestInfoStat  **regExps;

   uint32           numStats;
   GuestInfoStat   *stats;

   HashTable       *reportMap;

   Bool             timeData;
   double           timeStamp;
} GuestInfoCollector;

static GuestInfoCollector *gCurrentCollector = NULL;
static GuestInfoCollector *gPreviousCollector = NULL;

static void
GuestInfoDeriveMemNeeded(GuestInfoCollector *collector);

typedef struct GuestInfoDiskStatsList {
   struct GuestInfoDiskStatsList *next;
   char                          *diskName;
   unsigned int                   weightedTime[2];  // In milliseconds
} GuestInfoDiskStatsList;

static GuestInfoDiskStatsList *gDiskStatsList = NULL;


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoDeleteDiskStatsList --
 *
 *      Delete disk device stats list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoDeleteDiskStatsList(GuestInfoDiskStatsList *head)  // IN/OUT:
{
   GuestInfoDiskStatsList *curr = head;

   while (curr != NULL) {
      GuestInfoDiskStatsList *next = curr->next;
      free(curr->diskName);
      free(curr);
      curr = next;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoIsBlockDevice --
 *
 *      Verify block device name.
 *
 * Results:
 *      TRUE   name is block device name
 *      FALSE  otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
GuestInfoIsBlockDevice(const char *name)  // IN:
{
   char path[PATH_MAX]; // PATH_MAX is defined to 4096

   Str_Sprintf(path, sizeof path, "%s/%s", SYSFS_BLOCK_FOLDER, name);

   return (access(path, F_OK) == 0);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoGetUpTime --
 *
 *      What time is it?
 *
 * Results:
 *      TRUE   Success! *now is populated
 *      FALSE  Failure! *now remains unchanged
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
GuestInfoGetUpTime(double *now)  // OUT:
{
   char line[512];
   Bool result = FALSE;
   FILE *fp = Posix_Fopen(UPTIME_FILE, "r");

   if (fp == NULL) {
      return result;
   }

   if (fgets(line, sizeof line, fp) == line) {
      double idle;

      if (sscanf(line, "%lf %lf", now, &idle) == 2) {
         result = TRUE;
      }
   }

   fclose(fp);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoStoreStat --
 *
 *      Store a stat.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Handles overflow detection.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoStoreStat(GuestInfoStat *stat,   // IN/OUT: stat
                   uint64 value)          // IN: value to be added to stat
{
   ASSERT(stat);

   // TODO: consider supporting regexp here.
   switch (stat->err) {
   case 0:
      ASSERT(stat->count != 0);

      if (((stat->count + 1) < stat->count) ||
          ((stat->value + value) < stat->value)) {
         stat->err = EOVERFLOW;
      } else {
         stat->count++;
         stat->value += value;
      }
      break;

   case ENOENT:
      ASSERT(stat->count == 0);

      stat->err = 0;
      stat->count = 1;
      stat->value = value;
      break;

   default:  // Some sort of error - sorry, thank you for playing...
      break;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoStoreStatByID --
 *
 *      Store a stat value by its ID.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoStoreStatByID(GuestStatToolsID reportID,      // IN:
                       GuestInfoCollector *collector,  // IN/OUT:
                       uint64 value)                   // IN:
{
   GuestInfoStat *stat = NULL;

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(reportID),
                    (void **) &stat);

   GuestInfoStoreStat(stat, value);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoCollectStat --
 *
 *      Collect a stat.
 *
 *      NOTE: Exact match data cannot be used in a regExp. This is a
 *            performance choice.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoCollectStat(const char *pathName,           // IN:
                     GuestInfoCollector *collector,  // IN/OUT:
                     const char *fieldName,          // IN:
                     uint64 value)                   // IN:
{
   GuestInfoStat *stat = NULL;
   char *key = Str_SafeAsprintf(NULL, KEY_FORMAT, pathName, fieldName);

   if (!HashTable_Lookup(collector->exactMatches, key, (void **) &stat)) {
      uint32 i;

      for (i = 0; i < collector->numRegExps; i++) {
         GuestInfoStat *thisOne = collector->regExps[i];

         if (strcmp(pathName, thisOne->query->sourceFile) == 0 &&
             StrUtil_StartsWith(fieldName, thisOne->query->locatorString)) {
            stat = thisOne;
            break;
         }
      }
   }

   free(key);

   if (stat != NULL) {
      GuestInfoStoreStat(stat, value);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoProcData --
 *
 *      Reads a "stat file" and contributes to the collection.
 *
 *      NOTE: If caller specifies a fieldSeparator, it has to be present
 *            in the fieldName being parsed. '\0' represents an unspecified
 *            fieldSeparator.
 *
 * Results:
 *      TRUE   Success!
 *      FALSE  Failure!
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
GuestInfoProcData(const char *pathName,           // IN: path name
                  char fieldSeparator,            // IN/OPT:
                  GuestInfoCollector *collector)  // IN/OUT:
{
   char line[4096];  // Close to length of /proc/stat intr line
   FILE *fp = Posix_Fopen(pathName, "r");

   if (fp == NULL) {
      g_warning("%s: Error opening %s.\n", __FUNCTION__, pathName);
      return FALSE;
   }

   /*
    * If a line inside the file is longer than what the buffer can hold,
    * fgets still succeeds, the next fgets call continues at the previously
    * stopped location in the line.
    */
   while (fgets(line, sizeof line, fp) == line) {
      uint64 value = 0;
      char *savedPtr = NULL;
      char *fieldName = strtok_r(line, " \t", &savedPtr);
      char *fieldData = strtok_r(NULL, " \t", &savedPtr);

      if (fieldName == NULL) {
         continue;
      }

      if (fieldSeparator != '\0') {
         char *p = strrchr(fieldName, fieldSeparator);
         if (p == NULL) {
            /*
             * When fieldSeparator is specified, fieldName is expected
             * to have it.
             */
            continue;
         } else {
            *p = '\0';
         }
      }

      if ((fieldData == NULL) ||
          (sscanf(fieldData, "%"FMT64"u", &value) != 1)) {
         continue;
      }

      GuestInfoCollectStat(pathName, collector, fieldName, value);
   }

   fclose(fp);

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoProcSimpleValue --
 *
 *      Reads the stat /proc file, extracts a single, simple value and
 *      adds it to the collection.
 *
 * Results:
 *      TRUE   Success!
 *      FALSE  Failure!
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
#if PUBLISH_EXPERIMENTAL_STATS

static Bool
GuestInfoProcSimpleValue(GuestStatToolsID reportID,      // IN:
                         GuestInfoCollector *collector)  // IN/OUT:
{
   char line[512];
   uint64 value;
   FILE *fp;
   Bool success = FALSE;
   GuestInfoStat *stat = NULL;

   HashTable_Lookup(collector->reportMap, INT_AS_HASHKEY(reportID),
                    (void **) &stat);
   ASSERT(stat);
   if (stat == NULL) {
      g_warning("%s: Error stat ID %d not found.\n", __FUNCTION__, reportID);
      return success;
   }

   ASSERT(stat->query->sourceFile);
   fp = Posix_Fopen(stat->query->sourceFile, "r");
   if (fp == NULL) {
      g_warning("%s: Error opening %s.\n",
                __FUNCTION__, stat->query->sourceFile);
      return success;
   }

   if (fgets(line, sizeof line, fp) == line) {
      value = 0;
      if (sscanf(line, "%"FMT64"u", &value) == 1) {
         stat->err = 0;
         stat->count = 1;
         stat->value = value;

         success = TRUE;
      }
   }

   fclose(fp);

   return success;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoDeriveSwapData --
 *
 *      Update the swap stats that are calculated rather than fetched.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
#if PUBLISH_EXPERIMENTAL_STATS

static void
GuestInfoDeriveSwapData(GuestInfoCollector *collector)  // IN/OUT:
{
   uint64 swapFree = 0;
   uint64 swapTotal = 0;
   uint64 swapUsed = 0;
   GuestInfoStat *swapSpaceRemaining = NULL;
   GuestInfoStat *swapSpaceUsed = NULL;
   GuestInfoStat *swapFilesCurrent = NULL;
   GuestInfoStat *swapFilesMax = NULL;

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_SwapFilesMax),
                    (void **) &swapFilesMax);

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_SwapFilesCurrent),
                    (void **) &swapFilesCurrent);

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_SwapSpaceUsed),
                    (void **) &swapSpaceUsed);

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_SwapSpaceRemaining),
                    (void **) &swapSpaceRemaining);

   /*
    * Start by getting SwapTotal (from Id_SwapFilesCurrent).
    * Set Id_SwapFilesMax to that if it doesn't have its own opinion.
    */
   if ((swapFilesCurrent != NULL) && (swapFilesCurrent->err == 0)) {
      swapTotal = swapFilesCurrent->value;

      if ((swapFilesMax != NULL) && (swapFilesMax->err != 0)) {
         swapFilesMax->value = swapTotal;
         swapFilesMax->count = 1;
         swapFilesMax->err = 0;
      }

      /*
       * Get SwapFree (from Id_SwapSpaceRemaining)
       * Set Id_SwapSpaceUsed to SwapTotal-SwapFree if it doesn't have its
       * own opinion.
       */
      if ((swapSpaceRemaining != NULL) && (swapSpaceRemaining->err == 0)) {
         swapFree = swapSpaceRemaining->value;

         ASSERT(swapTotal >= swapFree);
         swapUsed = (swapTotal >= swapFree) ? swapTotal - swapFree : 0;
         if ((swapSpaceUsed != NULL) && (swapSpaceUsed->err != 0)) {
            swapSpaceUsed->value = swapUsed;
            swapSpaceUsed->count = 1;
            swapSpaceUsed->err = 0;
         }
      }
   }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoDecreaseCpuRunQueueByOne --
 *
 *      Exclude the collector thread, make the result be consistent with
 *      "sar -q".
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoDecreaseCpuRunQueueByOne(GuestInfoCollector *collector)  // IN/OUT:
{
   GuestInfoStat *stat = NULL;

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_Linux_CpuRunQueue),
                    (void **) &stat);

   ASSERT(stat != NULL);
   ASSERT(stat->err == 0);
   ASSERT(stat->count == 1);
   if (stat != NULL && stat->err == 0 && stat->count == 1) {
      if (stat->value > 0) {
         stat->value--;
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoProcDiskStatsData --
 *
 *      Reads /proc/diskstats, extract disk request queue stats and
 *      adds them to the collection.
 *
 * Results:
 *      TRUE   Success!
 *      FALSE  Failure!
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
GuestInfoProcDiskStatsData(GuestInfoCollector *collector)  // IN/OUT:
{
   static int curr = 0;

   int prev;
   GuestInfoDiskStatsList **listItem;
   uint64 inflightIOsSum;
   Bool setStats; // Only when no disk device change in between

   char line[512];
   FILE *fp = Posix_Fopen(DISKSTATS_FILE, "r");

   if (fp == NULL) {
      g_warning("%s: Error opening " DISKSTATS_FILE ".\n", __FUNCTION__);
      return FALSE;
   }

   prev = curr ^ 1;  // curr = 0 => prev = 1; curr = 1 => prev = 0
   listItem = &gDiskStatsList;
   inflightIOsSum = 0;
   setStats = (gDiskStatsList != NULL) ? TRUE : FALSE;

   while (fgets(line, sizeof line, fp) == line) {
      /*
       * Linux kernel diskstats_show format string:
       * "%4d %7d %s %lu %lu %lu %u %lu %lu %lu %u %u %u %u\n"
       *
       * Matching data types are selected to calculate accumulating fields'
       * increment correctly in case of overflow for both 32 and 64 bit
       * Linux systems.
       */
      int assignedCount;
      char diskName[NAME_MAX + 1]; // NAME_MAX is defined to 255
      long unsigned int readIOs;   // # of reads completed
      long unsigned int writeIOs;  // # of writes completed
      unsigned int inflightIOs;    // # of I/Os currently in progress
      unsigned int weightedTime;   // Weighted # of milliseconds
                                   // spent in doing I/Os
      assignedCount = sscanf(line,
                             "%*d %*d %" XSTR(NAME_MAX) "s "
                             "%lu %*u %*u %*u "
                             "%lu %*u %*u %*u "
                             "%u %*u %u",
                             diskName, // '\0' is added automatically
                             &readIOs,
                             &writeIOs,
                             &inflightIOs, &weightedTime);
      if (assignedCount != 5 ||
          (readIOs == 0 && writeIOs == 0) ||
          !GuestInfoIsBlockDevice(diskName)) {
         continue;
      }

      inflightIOsSum += inflightIOs;

      if (*listItem != NULL) {
         if (strcmp((*listItem)->diskName, diskName) == 0) {
            (*listItem)->weightedTime[curr] = weightedTime;
         } else {
            /* Disk hot plug/unplug, rebuild the rest of the list. */
            GuestInfoDeleteDiskStatsList(*listItem);
            *listItem = NULL;
         }
      }

      if (*listItem == NULL) {
         *listItem = (GuestInfoDiskStatsList *)
                         Util_SafeMalloc(sizeof **listItem);
         (*listItem)->next = NULL;
         (*listItem)->diskName = Util_SafeStrdup(diskName);
         (*listItem)->weightedTime[curr] = weightedTime;
         (*listItem)->weightedTime[prev] = 0;

         /* Also covers disk hot plug at the end of the list. */
         setStats = FALSE;
      }

      listItem = &((*listItem)->next);
   }

   fclose(fp);

   if (listItem == &gDiskStatsList // No qualified disk device found
       || *listItem != NULL) {     // Disk hot unplug at the end of the list
      GuestInfoDeleteDiskStatsList(*listItem);
      *listItem = NULL;
      setStats = FALSE;
   }

   if (setStats) {
      GuestInfoDiskStatsList *currDiskStats = gDiskStatsList;
      uint64 weightedTimeDeltaSum = 0;

      while (currDiskStats != NULL) {
         unsigned int weightedTimeDelta = currDiskStats->weightedTime[curr] -
                                          currDiskStats->weightedTime[prev];
         weightedTimeDeltaSum += weightedTimeDelta;
         currDiskStats = currDiskStats->next;
      }

      GuestInfoStoreStatByID(GuestStatID_Linux_DiskRequestQueue,
                             collector,
                             inflightIOsSum);
      GuestInfoStoreStatByID(GuestStatID_Linux_DiskRequestQueueAvg,
                             collector,
                             weightedTimeDeltaSum);
   }

   curr = prev;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoCollect --
 *
 *      Fill the specified collector with as much sampled data as possible.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoCollect(GuestInfoCollector *collector)  // IN/OUT:
{
   uint32 i;
   GuestInfoStat *stat;
   uint64 pageSize = sysconf(_SC_PAGESIZE);

   /* Reset all values */
   for (i = 0; i < collector->numStats; i++) {
      GuestInfoStat *stat = &collector->stats[i];

      stat->err = ENOENT;  // There is no data here
      stat->count = 0;
      stat->value = 0;
   }

   /* Collect new values */
   GuestInfoProcData(MEMINFO_FILE, ':', collector);
   GuestInfoProcData(VMSTAT_FILE, '\0', collector);
   GuestInfoProcData(STAT_FILE, '\0', collector);
   GuestInfoProcData(ZONEINFO_FILE, '\0', collector);
#if PUBLISH_EXPERIMENTAL_STATS
   GuestInfoProcSimpleValue(GuestStatID_Linux_Swappiness, collector);
   GuestInfoDeriveSwapData(collector);
#endif

   collector->timeData = GuestInfoGetUpTime(&collector->timeStamp);

   /*
    * We make sure physical page size is always present.
    */
   GuestInfoStoreStatByID(GuestStatID_PhysicalPageSize,
                          collector,
                          pageSize);

   /*
    * Attempt to fix up memPhysUsable if it is not available.
    */

   stat = NULL;
   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_MemPhysUsable),
                    (void **) &stat);

   ASSERT(stat != NULL);  // Must be in table

   if (stat->err == 0) {
      stat->value *= (pageSize / 1024); // Convert pages to KiB
   } else {
      GuestInfoStat *memTotal = NULL;

      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_MemTotal),
                       (void **) &memTotal);

      if ((memTotal != NULL) && (memTotal->err == 0)) {
         stat->err = 0;
         stat->count = 1;
         stat->value = memTotal->value;
      }
   }

   GuestInfoDeriveMemNeeded(collector);
   GuestInfoDecreaseCpuRunQueueByOne(collector);
   GuestInfoProcDiskStatsData(collector);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoLegacy --
 *
 *      Fill in the legacy portion of the data to be returned.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoLegacy(GuestInfoCollector *current,  // IN: current collection
                GuestMemInfoLegacy *legacy)   // OUT: data filled out
{
   GuestInfoStat *stat;

   memset(legacy, 0, sizeof *legacy);

   legacy->version = GUESTMEMINFO_V5;
   legacy->flags   = 0;

   stat = NULL;
   HashTable_Lookup(current->reportMap,
                    INT_AS_HASHKEY(GuestStatID_MemPhysUsable),
                    (void **) &stat);

   if ((stat != NULL) && (stat->err == 0)) {
      legacy->memTotal = stat->value;
      legacy->flags |= MEMINFO_MEMTOTAL;
   }

   stat = NULL;
   HashTable_Lookup(current->reportMap,
                    INT_AS_HASHKEY(GuestStatID_Linux_HugePagesTotal),
                    (void **) &stat);

   if ((stat != NULL) && (stat->err == 0)) {
      legacy->hugePagesTotal = stat->value;
      legacy->flags |= MEMINFO_HUGEPAGESTOTAL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoBytesNeededUIntDatum --
 *
 * Results:
 *      Returns the number of bytes needed to encode a UInt.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static uint16
GuestInfoBytesNeededUIntDatum(uint64 value)  // IN:
{
   if (value == 0) {
      return 0;
   } else if (value <= MAX_UINT8) {
      return sizeof(uint8);
   } else if (value <= MAX_UINT16) {
      return sizeof(uint16);
   } else if (value <= MAX_UINT32) {
      return sizeof(uint32);
   } else {
      return sizeof(uint64);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoAppendStat --
 *
 *      Append information about the specified stat to the DynBuf of stat
 *      data.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory may be dynamically allocated (via DynBuf).
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoAppendStat(int errnoValue,                // IN:
                    Bool emitNameSpace,            // IN:
                    GuestStatToolsID reportID,     // IN:
                    GuestValueUnits units,         // IN:
                    GuestValueType valueType,      // IN:
                    void *value,                   // IN:
                    size_t valueSize,              // IN:
                    DynBuf *stats)                 // IN/OUT:
{
   const char *NameSpace = GUEST_TOOLS_NAMESPACE;
   uint64 value64;
   GuestStatHeader header;
   GuestDatumHeader datum;

   header.datumFlags = GUEST_DATUM_ID |
                       GUEST_DATUM_VALUE_TYPE_ENUM |
                       GUEST_DATUM_VALUE_UNIT_ENUM;
   if (emitNameSpace) {
      header.datumFlags |= GUEST_DATUM_NAMESPACE;
   }
   if (errnoValue == 0) {
      header.datumFlags |= GUEST_DATUM_VALUE;
   }
   DynBuf_Append(stats, &header, sizeof header);

   if (header.datumFlags & GUEST_DATUM_NAMESPACE) {
      size_t nameSpaceLen = strlen(NameSpace) + 1;
      datum.dataSize = nameSpaceLen;
      DynBuf_Append(stats, &datum, sizeof datum);
      DynBuf_Append(stats, NameSpace, nameSpaceLen);
   }

   if (header.datumFlags & GUEST_DATUM_ID) {
      value64 = reportID;
      datum.dataSize = GuestInfoBytesNeededUIntDatum(value64);
      DynBuf_Append(stats, &datum, sizeof datum);
      DynBuf_Append(stats, &value64, datum.dataSize);
   }

   if (header.datumFlags & GUEST_DATUM_VALUE_TYPE_ENUM) {
      value64 = valueType;
      datum.dataSize = GuestInfoBytesNeededUIntDatum(value64);
      DynBuf_Append(stats, &datum, sizeof datum);
      DynBuf_Append(stats, &value64, datum.dataSize);
   }

   if (header.datumFlags & GUEST_DATUM_VALUE_UNIT_ENUM) {
      value64 = units;
      datum.dataSize = GuestInfoBytesNeededUIntDatum(value64);
      DynBuf_Append(stats, &datum, sizeof datum);
      DynBuf_Append(stats, &value64, datum.dataSize);
   }

   if (header.datumFlags & GUEST_DATUM_VALUE) {
      datum.dataSize = valueSize;
      DynBuf_Append(stats, &datum, sizeof datum);
      DynBuf_Append(stats, value, valueSize);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoAppendRate --
 *
 *      Compute a rate and then append it to the stat buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoAppendRate(Bool emitNameSpace,             // IN:
                    GuestStatToolsID reportID,      // IN: ID of the stat
                    GuestInfoCollector *current,    // IN: current collection
                    GuestInfoCollector *previous,   // IN: previous collection
                    DynBuf *statBuf)                // IN/OUT: stat data
{
   double valueDouble = 0.0;
   int errnoValue = ENOENT;
   GuestInfoStat *currentStat = NULL;
   GuestInfoStat *previousStat = NULL;

   HashTable_Lookup(current->reportMap,
                    INT_AS_HASHKEY(reportID),
                    (void **) &currentStat);

   HashTable_Lookup(previous->reportMap,
                    INT_AS_HASHKEY(reportID),
                    (void **) &previousStat);

   if (current->timeData &&
       previous->timeData &&
       ((currentStat != NULL) && (currentStat->err == 0)) &&
       ((previousStat != NULL) && (previousStat->err == 0))) {
      double timeDelta = current->timeStamp - previous->timeStamp;
      double valueDelta;

      /*
       * DiskRequestQueueAvg GuestInfoStat::value is weighted number of
       * milliseconds delta in uint64 type, need to divide it by 1000 to
       * turn the number in seconds.
       *
       * Host side drops the fraction part of double data type. Therefore,
       * we preserve 2 decimal points by scaling up the value 100x.
       * The consumers of this stat need to divide it by 100 to retrieve
       * two digits after decimal point.
       *
       * (value / 1000) * 100 = value / 10
       */
      if (reportID == GuestStatID_Linux_DiskRequestQueueAvg) {
         valueDelta = ((double)(currentStat->value)) / 10;
      } else {
         /*
          * The /proc FS stat can be uint32 type in the kernel on both x86
          * and x64 Linux, it is parsed and stored as uint64 in tools, so we
          * also need to handle uint32 overflow here.
          */
         if (currentStat->value < previousStat->value &&
             previousStat->value <= MAX_UINT32) {
            valueDelta = (uint32)(currentStat->value) -
                         (uint32)(previousStat->value);
         } else {
            valueDelta = currentStat->value - previousStat->value;
         }
      }

      valueDouble = valueDelta / timeDelta;

      errnoValue = 0;
   }

   if (currentStat != NULL) {
      float valueFloat;
      void *valuePointer;
      size_t valueSize;

      if (valueDouble == 0) {
         valuePointer = NULL;
         valueSize = 0;
      } else {
         valueFloat = (float)valueDouble;
         if ((double)valueFloat == valueDouble) {
            valuePointer = &valueFloat;
            valueSize = sizeof valueFloat;
         } else {
            valuePointer = &valueDouble;
            valueSize = sizeof valueDouble;
         }
      }

      GuestInfoAppendStat(errnoValue, emitNameSpace, reportID,
                          currentStat->query->units, GuestTypeDouble,
                          valuePointer, valueSize, statBuf);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoDeriveMemNeeded --
 *
 *      Update memory needed stats that are calculated
 *      rather than fetched.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoDeriveMemNeeded(GuestInfoCollector *collector)  // IN/OUT:
{
   uint64 memNeeded;
   uint64 memNeededReservation;
   uint64 memAvailable = 0;
   GuestInfoStat *memAvail = NULL;
   GuestInfoStat *memPhysUsable = NULL;

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_MemPhysUsable),
                    (void **) &memPhysUsable);

   ASSERT(memPhysUsable != NULL);

   HashTable_Lookup(collector->reportMap,
                    INT_AS_HASHKEY(GuestStatID_Linux_MemAvailable),
                    (void **) &memAvail);

   if ((memAvail != NULL) && (memAvail->err == 0)) {
      memAvailable = memAvail->value;
   } else {
      GuestInfoStat *memFree = NULL;
      GuestInfoStat *memCache = NULL;
      GuestInfoStat *memBuffers = NULL;
      GuestInfoStat *memActiveFile = NULL;
      GuestInfoStat *memSlabReclaim = NULL;
      GuestInfoStat *memInactiveFile = NULL;
      GuestInfoStat *lowWaterMark = NULL;

      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_MemFree),
                       (void **) &memFree);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_MemCached),
                       (void **) &memCache);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_MemBuffers),
                       (void **) &memBuffers);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_MemActiveFileCache),
                       (void **) &memActiveFile);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_MemSlabReclaim),
                       (void **) &memSlabReclaim);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_MemInactiveFile),
                       (void **) &memInactiveFile);
      HashTable_Lookup(collector->reportMap,
                       INT_AS_HASHKEY(GuestStatID_Linux_LowWaterMark),
                       (void **) &lowWaterMark);

      if (((memFree != NULL) && (memFree->err == 0)) &&
          ((memCache != NULL) && (memCache->err == 0)) &&
          ((memBuffers != NULL) && (memBuffers->err == 0)) &&
          (memActiveFile != NULL) &&
          (memSlabReclaim != NULL) &&
          (memInactiveFile != NULL) &&
          ((lowWaterMark != NULL) && (lowWaterMark->err == 0))) {
         uint64 pageCache;
         unsigned long kbPerPage = sysconf(_SC_PAGESIZE) / 1024UL;
         uint64 lowWaterMarkValue = lowWaterMark->value * kbPerPage;

         memAvailable = memFree->value - lowWaterMarkValue;

         if ((memActiveFile->err == 0) && (memInactiveFile->err == 0)) {
            pageCache = memActiveFile->value + memInactiveFile->value;
         } else {
            /*
             * If the kernel is too old to expose Active/Inactive file,
             * this is the best approxmation for pageCache.
             */
            pageCache = memCache->value + memBuffers->value;
         }

         pageCache -= MIN(pageCache / 2, lowWaterMarkValue);
         memAvailable += pageCache;

         if (memSlabReclaim->err == 0) {
            memAvailable += memSlabReclaim->value -
                            MIN(memSlabReclaim->value / 2, lowWaterMarkValue);
         }

         if ((int64)memAvailable < 0) {
            memAvailable = 0;
         }

         GuestInfoStoreStatByID(GuestStatID_Linux_MemAvailable,
                                collector,
                                memAvailable);
      }
   }

   if (memPhysUsable->err == 0) {
      /*
       * Reserve 5% of physical RAM for surges.
       */
      memNeededReservation = memPhysUsable->value / 20;

      if (memAvailable > memNeededReservation) {
         memAvailable -= memNeededReservation;
      } else {
         memAvailable = 0;
      }

      /*
       * We got these values from one read of /proc/meminfo. Everything should
       * really be coherent.
       */
      ASSERT(memPhysUsable->value >= memAvailable);
      memNeeded = memPhysUsable->value - memAvailable;
   } else {
      memNeeded = 0;
      memNeededReservation = 0;
   }

   GuestInfoStoreStatByID(GuestStatID_MemNeeded,
                          collector,
                          memNeeded);

   GuestInfoStoreStatByID(GuestStatID_MemNeededReservation,
                          collector,
                          memNeededReservation);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoEncodeStats --
 *
 *      Encode the guest stats.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoEncodeStats(GuestInfoCollector *current,   // IN: current collection
                     GuestInfoCollector *previous,  // IN: previous collection
                     DynBuf *statBuf)               // IN/OUT: stats data
{
   uint32 i;
   GuestMemInfoLegacy legacy;
   Bool emitNameSpace = TRUE;

   /* Provide legacy data for backwards compatibility */
   GuestInfoLegacy(current, &legacy);

   DynBuf_Append(statBuf, &legacy, sizeof legacy);

   /* Provide data in the new, extensible format. */
   for (i = 0; i < current->numStats; i++) {
      GuestInfoStat *stat = &current->stats[i];

      if (!*(stat->query->publish)) {
         continue;
      }

      if (stat->query->dataType == GuestTypeDouble) {
         GuestInfoAppendRate(emitNameSpace, stat->query->reportID,
                             current, previous, statBuf);
      } else {
         ASSERT(stat->query->dataType == GuestTypeUint64);
         ASSERT((stat->query->units & GuestUnitsModifier_Rate) == 0);
         GuestInfoAppendStat(stat->err,
                             emitNameSpace,
                             stat->query->reportID,
                             stat->query->units,
                             stat->query->dataType,
                             &stat->value,
                             GuestInfoBytesNeededUIntDatum(stat->value),
                             statBuf);
      }

      emitNameSpace = FALSE; // use the smallest representation
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoDestroyCollector --
 *
 *      Destroy the collector representation.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoDestroyCollector(GuestInfoCollector *collector)  // IN:
{
   if (collector != NULL) {
      HashTable_Free(collector->exactMatches);
      HashTable_Free(collector->reportMap);
      free(collector->regExps);
      free(collector->stats);
      free(collector);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoConstructCollector --
 *
 *      Construct a collector.
 *
 * Results:
 *      NULL Failure!
 *     !NULL Success! Collector representation.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static GuestInfoCollector *
GuestInfoConstructCollector(GuestInfoQuery *queries,  // IN:
                            uint32 numQueries)        // IN:
{
   uint32 i;
   uint32 regExp = 0;
   GuestInfoCollector *collector = Util_SafeCalloc(1, sizeof *collector);

   if (collector == NULL) {
      return NULL;
   }

   collector->reportMap = HashTable_Alloc(256, HASH_INT_KEY, NULL);

   collector->exactMatches = HashTable_Alloc(256,
                                           HASH_STRING_KEY | HASH_FLAG_COPYKEY,
                                             NULL);

   collector->numRegExps = 0;
   for (i = 0; i < numQueries; i++) {
      if (queries[i].isRegExp) {
         collector->numRegExps++;
      }
   }

   collector->numStats = numQueries;
   collector->stats = Util_SafeCalloc(numQueries, sizeof *collector->stats);
   collector->regExps = Util_SafeCalloc(collector->numRegExps,
                                        sizeof(GuestInfoStat *));

   if ((collector->exactMatches == NULL) ||
       (collector->reportMap == NULL) ||
       ((collector->numRegExps != 0) && (collector->regExps == NULL)) ||
       ((collector->numStats != 0) && (collector->stats == NULL))) {
      GuestInfoDestroyCollector(collector);
      return NULL;
   }

   regExp = 0;

   for (i = 0; i < numQueries; i++) {
      GuestInfoQuery *query = &queries[i];
      GuestInfoStat *stat = &collector->stats[i];

      ASSERT(query->reportID);

      stat->query = query;

      if (query->isRegExp) {
         ASSERT(query->sourceFile);
         ASSERT(query->locatorString);

         collector->regExps[regExp++] = stat;
      } else {
         if (query->sourceFile != NULL && query->locatorString != NULL) {
            char *key = Str_SafeAsprintf(NULL, KEY_FORMAT, query->sourceFile,
                                         query->locatorString);
            HashTable_Insert(collector->exactMatches, key, stat);
            free(key);
         }
      }

      /* The report lookup */
      HashTable_Insert(collector->reportMap, INT_AS_HASHKEY(query->reportID),
                       stat);
   }

   return collector;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoTakeSample --
 *
 *      Gather performance stats.
 *
 * Results:
 *      TRUE   Success! statBuf contains collected data
 *      FALSE  Failure! statBuf contains no collected data
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestInfoTakeSample(DynBuf *statBuf)  // IN/OUT: inited, ready to fill
{
   GuestInfoCollector *temp;

   ASSERT(statBuf && DynBuf_GetSize(statBuf) == 0);

   /* Preallocate space to minimize realloc operations. */
   if (!DynBuf_Enlarge(statBuf, GUEST_INFO_PREALLOC_SIZE)) {
      return FALSE;
   }

   /* First time through, allocate all necessary memory */
   if (gPreviousCollector == NULL) {
      gCurrentCollector = GuestInfoConstructCollector(guestInfoQuerySpecTable,
                                            N_QUERIES);

      gPreviousCollector = GuestInfoConstructCollector(guestInfoQuerySpecTable,
                                             N_QUERIES);
   }

   if ((gCurrentCollector == NULL) ||
       (gPreviousCollector == NULL)) {
      GuestInfoDestroyCollector(gCurrentCollector);
      gCurrentCollector = NULL;
      GuestInfoDestroyCollector(gPreviousCollector);
      gPreviousCollector = NULL;
      return FALSE;
   }

   /* Collect the current data */
   GuestInfoCollect(gCurrentCollector);

   /* Encode the captured data */
   GuestInfoEncodeStats(gCurrentCollector, gPreviousCollector, statBuf);

   /* Switch the collections for next time. */
   temp = gCurrentCollector;
   gCurrentCollector = gPreviousCollector;
   gPreviousCollector = temp;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_StatProviderPoll --
 *
 *      Called when a new stat sample is requested. GuestInfo_ReportStats
 * should be called once the sample is available. If gathering is taking
 * longer than sampling frequency, the request may be ignored.
 *
 * @param[in]  data     The application context.
 *
 * @return TRUE to indicate that the timer should be rescheduled.
 *
 *----------------------------------------------------------------------
 */

gboolean
GuestInfo_StatProviderPoll(gpointer data)
{
   ToolsAppCtx *ctx = data;
   DynBuf stats;

   g_debug("Entered guest info stats gather.\n");

#if ADD_NEW_STATS
   gUnstable = g_key_file_get_boolean(ctx->config,
                                      CONFGROUPNAME_GUESTINFO,
                                      "enable-unstable-stats",
                                      NULL);
#endif

   /* Send the vmstats to the VMX. */
   DynBuf_Init(&stats);

   if (!GuestInfoTakeSample(&stats)) {
      g_warning("Failed to get vmstats.\n");
   } else if (!GuestInfo_ServerReportStats(ctx, &stats)) {
      g_warning("Failed to send vmstats.\n");
   }

   DynBuf_Destroy(&stats);
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_StatProviderShutdown --
 *
 *      Clean up the resource acquired by perfMonLinux.
 *      Nothing to do at the moment.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
GuestInfo_StatProviderShutdown(void)
{
   GuestInfoDeleteDiskStatsList(gDiskStatsList);
   gDiskStatsList = NULL;

   GuestInfoDestroyCollector(gCurrentCollector);
   gCurrentCollector = NULL;
   GuestInfoDestroyCollector(gPreviousCollector);
   gPreviousCollector = NULL;
}

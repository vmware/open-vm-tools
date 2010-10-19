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

#ifndef VMWARE_LOG_H
#define VMWARE_LOG_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE

#include "includeCheck.h"

#include <stdarg.h>

/*
 * The log levels are taken from POSIXen syslog and extended. We hijack the
 * standard values on POSIXen and provide equivalent defines on all other
 * platforms. Then the debugging levels are extended "down" to allow for
 * multiple levels of debugging "noise".
 *
 * NOTE: The reuse of the syslog defines is temporary. The levels will be
 *       moved to a private name space soon.
 *
 * The conceptual model is as follows:
 *
 * LOG_EMERG       0   (highest priority)
 * LOG_ALERT       1
 * LOG_CRIT        2
 * LOG_ERR         3
 * LOG_WARNING     4   (<= this priority is written to stderr by default)
 * LOG_NOTICE      5
 * LOG_INFO        6   (<= this priority are logged by default)
 * LOG_DEBUG_00    7   (noisiest level of debugging; also LOG_DEBUG)
 * LOG_DEBUG_01    8 
 * LOG_DEBUG_02    9 
 * LOG_DEBUG_03    10 
 * LOG_DEBUG_04    11 
 * LOG_DEBUG_05    12 
 * LOG_DEBUG_06    13 
 * LOG_DEBUG_07    14 
 * LOG_DEBUG_08    15 
 * LOG_DEBUG_09    16 
 * LOG_DEBUG_10    17  (lowest priority; least noisy debugging level)
 */

#if defined(_WIN32) || defined(VMM)
#define LOG_EMERG    0
#define LOG_ALERT    1
#define LOG_CRIT     2
#define LOG_ERR      3
#define LOG_WARNING  4
#define LOG_NOTICE   5
#define LOG_INFO     6
#define LOG_DEBUG    7
#else
#   include <syslog.h>
#endif

#define LOG_DEBUG_00    LOG_DEBUG + 0
#define LOG_DEBUG_01    LOG_DEBUG + 1
#define LOG_DEBUG_02    LOG_DEBUG + 2
#define LOG_DEBUG_03    LOG_DEBUG + 3
#define LOG_DEBUG_04    LOG_DEBUG + 4
#define LOG_DEBUG_05    LOG_DEBUG + 5
#define LOG_DEBUG_06    LOG_DEBUG + 6
#define LOG_DEBUG_07    LOG_DEBUG + 7
#define LOG_DEBUG_08    LOG_DEBUG + 8
#define LOG_DEBUG_09    LOG_DEBUG + 9
#define LOG_DEBUG_10    LOG_DEBUG + 10

typedef void (LogBasicFunc)(const char *fmt,
                            va_list args);


typedef enum {
   LOG_SYSTEM_LOGGER_NONE,
   LOG_SYSTEM_LOGGER_ADJUNCT,
   LOG_SYSTEM_LOGGER_ONLY
} SysLogger;

typedef struct
{
   const char    *fileName;             // File name, if known
   const char    *config;               // Config variable to look up
   const char    *suffix;               // Suffix to generate log file name
   const char    *appName;              // App name for log header
   const char    *appVersion;           // App version for log header

   Bool           append;               // Append to log file
   Bool           switchFile;           // Switch the initial log file
   Bool           useThreadName;        // Thread name on log line
   Bool           useTimeStamp;         // Use a log line time stamp
   Bool           useMilliseconds;      // Show milliseconds in time stamp
   Bool           fastRotation;         // ESX log rotation optimization
   Bool           preventRemove;        // prevert Log_RemoveFile(FALSE)

   int32          stderrMinLevel;       // This level and above to stderr
   int32          logMinLevel;          // This level and above to log

   uint32         keepOld;              // Number of old logs to keep
   uint32         throttleThreshold;    // Threshold for throttling
   uint32         throttleBPS;          // BPS for throttle
   uint32         rotateSize;           // Size at which log should be rotated

   SysLogger      systemLoggerUse;      // System logger options
   char           systemLoggerID[128];  // Identifier for system logger
} LogInitParams;

void Log_GetInitDefaults(const char *fileName,
                         const char *config,
                         const char *suffix,
                         const char *appPrefix,
                         LogInitParams *params);

Bool Log_Init(const char *fileName,
              const char *config,
              const char *suffix);

Bool Log_InitEx(const LogInitParams *params);

void Log_UpdateFileControl(Bool append,
                           unsigned keepOld,
                           size_t rotateSize,
                           Bool fastRotation,
                           uint32 throttleThreshold,
                           uint32 throttleBPS);

void Log_UpdatePerLine(Bool perLineTimeStamps,
                       Bool perLineMilliseconds,
                       Bool perLineThreadNames);

void Log_Exit(void);
void Log_SetConfigDir(const char *configDir);
void Log_WriteBytes(const char *msg);

Bool Log_Outputting(void);
const char *Log_GetFileName(void);
void Log_SkipLocking(Bool skipLocking);
void Log_SetAlwaysKeep(Bool alwaysKeep);
Bool Log_RemoveFile(Bool alwaysRemove);
void Log_DisableThrottling(void);
void Log_EnableStderrWarnings(Bool stderrOutput);
void Log_BackupOldFiles(const char *fileName, Bool noRename);
Bool Log_CopyFile(const char *fileName);
uint32 Log_MaxLineLength(void);

void Log_RegisterBasicFunctions(LogBasicFunc *log,
                                LogBasicFunc *warning);

Bool Log_SetOutput(const char *fileName,
                   const char *config,
                   Bool copy,
                   uint32 systemLoggerUse,
                   char *systemLoggerID);

size_t Log_MakeTimeString(Bool millisec,
                          char *buf,
                          size_t max);

void LogV(int level,
          const char *fmt,
          va_list args);


static INLINE void
Log_Level(int level,
          const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(level, fmt, ap);
   va_end(ap);
}

/*
 * Handy wrapper functions.
 *
 * Log -> LOG_INFO
 * Warning -> LOG_WARNING
 *
 * TODO: even Log and Warning become wrapper functions around LogV.
 */

static INLINE void
Log_Emerg(const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(LOG_EMERG, fmt, ap);
   va_end(ap);
}


static INLINE void
Log_Alert(const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(LOG_ALERT, fmt, ap);
   va_end(ap);
}


static INLINE void
Log_Crit(const char *fmt,
         ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(LOG_CRIT, fmt, ap);
   va_end(ap);
}


static INLINE void
Log_Err(const char *fmt,
        ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(LOG_ERR, fmt, ap);
   va_end(ap);
}


static INLINE void
Log_Notice(const char *fmt,
           ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(LOG_NOTICE, fmt, ap);
   va_end(ap);
}


/* Logging that uses the custom guest throttling configuration. */
void GuestLog_Init(void);
void GuestLog_Log(const char *fmt, ...) PRINTF_DECL(1, 2);


/*
 * How many old log files to keep around.
 *
 * ESX needs more old log files for bug fixing (and vmotion).
 */

#ifdef VMX86_SERVER
#define LOG_DEFAULT_KEEPOLD 6
#else
#define LOG_DEFAULT_KEEPOLD 3
#endif

#define LOG_NO_BPS_LIMIT             0xFFFFFFFF
#define LOG_NO_ROTATION_SIZE         0
#define LOG_NO_THROTTLE_THRESHOLD    0

#ifdef VMX86_RELEASE
#define LOG_DEFAULT_THROTTLE_BPS       1000
#else
#define LOG_DEFAULT_THROTTLE_BPS       LOG_NO_BPS_LIMIT
#endif

#define LOG_DEFAULT_THROTTLE_THRESHOLD 1000000


/*
 * Debugging
 */

void Log_HexDump(const char *prefix,
                 const uint8 *data,
                 int size);

void Log_Time(VmTimeType *time,
              int count,
              const char *message);

void Log_Histogram(uint32 n,
                   uint32 histo[],
                   int nbuckets,
                   const char *message,
                   int *count,
                   int limit);

#endif /* VMWARE_LOG_H */

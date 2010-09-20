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
 * The log levels are taken from POSIXen syslog. We hijack their values
 * on POSIXen and provide equivalent defines on all other platforms.
 */

#if defined(_WIN32)
#   include <windows.h>

#   define LOG_INFO       EVENTLOG_INFORMATION_TYPE
#   define LOG_WARNING    EVENTLOG_WARNING_TYPE
#   define LOG_ERR        EVENTLOG_ERROR_TYPE
#else
#   if !defined(VMM)
#      include <syslog.h>
#   endif
#endif


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
   Bool           stderrWarnings;       // warnings also go to stderr

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

void LogV(const char *fmt,
          va_list args);

void WarningV(const char *fmt,
              va_list args);

void Log_Level(int level,
               const char *fmt,
               ...);

void Log_LevelV(int level,
                const char *fmt,
                va_list args);

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

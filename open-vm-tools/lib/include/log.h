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
 * The bora/lig Log Facility log level model.
 * This the same as the vmacore/hostd Log Facility.
 *
 * The VMW_LOG_BASE is chosen to ensure that on all platforms commonly
 * used system logger values will be invalid and the errant usage caught.
 */

#define VMW_LOG_BASE      100
#define VMW_LOG_PANIC     VMW_LOG_BASE     +  0  // highest priority
#define VMW_LOG_ERROR     VMW_LOG_BASE     +  5
#define VMW_LOG_WARNING   VMW_LOG_BASE     + 10  // <= goes to stderr by default
#define VMW_LOG_AUDIT     VMW_LOG_BASE     + 15  // *ALWAYS* output to the log
#define VMW_LOG_INFO      VMW_LOG_BASE     + 20  // <= goes to log by default
#define VMW_LOG_VERBOSE   VMW_LOG_BASE     + 25
#define VMW_LOG_TRIVIA    VMW_LOG_BASE     + 30 
#define VMW_LOG_DEBUG_00  VMW_LOG_BASE     + 35  // noisiest level of debugging
#define VMW_LOG_DEBUG_01  VMW_LOG_DEBUG_00 +  1
#define VMW_LOG_DEBUG_02  VMW_LOG_DEBUG_00 +  2
#define VMW_LOG_DEBUG_03  VMW_LOG_DEBUG_00 +  3
#define VMW_LOG_DEBUG_04  VMW_LOG_DEBUG_00 +  4
#define VMW_LOG_DEBUG_05  VMW_LOG_DEBUG_00 +  5
#define VMW_LOG_DEBUG_06  VMW_LOG_DEBUG_00 +  6
#define VMW_LOG_DEBUG_07  VMW_LOG_DEBUG_00 +  7
#define VMW_LOG_DEBUG_08  VMW_LOG_DEBUG_00 +  8
#define VMW_LOG_DEBUG_09  VMW_LOG_DEBUG_00 +  9
#define VMW_LOG_DEBUG_10  VMW_LOG_DEBUG_00 + 10  // lowest priority; least noisy

void
LogVNoRoute(int level,
            const char *fmt,
            va_list args);


void LogV(uint32 routing,
          const char *fmt,
          va_list args);


/*
 * Handy wrapper functions.
 *
 * Log -> VMW_LOG_INFO
 * Warning -> VMW_LOG_WARNING
 *
 * TODO: even Log and Warning become wrapper functions around LogV.
 */

static INLINE void PRINTF_DECL(2, 3)
Log_Level(uint32 routing,
          const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(routing, fmt, ap);
   va_end(ap);
}


static INLINE void
Log_String(uint32 routing,
           const char *string)
{
   Log_Level(routing, "%s", string);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Panic(const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_PANIC, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Audit(const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_AUDIT, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Error(const char *fmt,
          ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_ERROR, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Verbose(const char *fmt,
            ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_VERBOSE, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Trivia(const char *fmt,
           ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_TRIVIA, fmt, ap);
   va_end(ap);
}

#if !defined(VMM)

/* Forward decl */
struct MsgList;

typedef void (LogOutputFunc)(int level,
                             const char *fmt,
                             va_list args);


typedef enum {
   LOG_SYSTEM_LOGGER_NONE,
   LOG_SYSTEM_LOGGER_ADJUNCT,
   LOG_SYSTEM_LOGGER_ONLY
} SysLogger;

typedef struct
{
   uint32         signature;            // initialization signature

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
   Bool           useLevelDesignator;   // Show level designator
   Bool           fastRotation;         // ESX log rotation optimization
   Bool           preventRemove;        // prevent Log_RemoveFile(FALSE)
   Bool           syncAfterWrite;       // Sync after a write. Expensive!

   int32          stderrMinLevel;       // This level and above to stderr
   int32          logMinLevel;          // This level and above to log

   uint32         keepOld;              // Number of old logs to keep
   uint32         throttleThreshold;    // Threshold for throttling
   uint32         throttleBPS;          // BPS for throttle
   uint32         rotateSize;           // Size at which log should be rotated

   SysLogger      systemLoggerUse;      // System logger options
   char           systemLoggerID[128];  // Identifier for system logger
} LogInitParams;

void Log_GetStaticDefaults(LogInitParams *params);

void Log_ApplyConfigValues(const char *appPrefix,
                           LogInitParams *params);

Bool Log_InitEx(const LogInitParams *params);

Bool Log_InitWithFile(const char *fileName,
                      const char *appPrefix);

Bool Log_InitWithConfig(const char *appPrefix);

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

Bool Log_Outputting(void);
const char *Log_GetFileName(void);
void Log_SkipLocking(Bool skipLocking);
void Log_SetAlwaysKeep(Bool alwaysKeep);
Bool Log_RemoveFile(Bool alwaysRemove);
void Log_DisableThrottling(void);
void Log_EnableStderrWarnings(Bool stderrOutput);
void Log_BackupOldFiles(const char *fileName, Bool noRename);
Bool Log_CopyFile(const char *fileName, struct MsgList **errs);
uint32 Log_MaxLineLength(void);

void Log_RegisterOutputFunction(LogOutputFunc *func);

Bool Log_SetOutput(const char *fileName,
                   const char *config,
                   Bool copy,
                   uint32 systemLoggerUse,
                   char *systemLoggerID,
                   struct MsgList **errs);

size_t Log_MakeTimeString(Bool millisec,
                          char *buf,
                          size_t max);


/* Logging that uses the custom guest throttling configuration. */
void GuestLog_Init(void);
void GuestLog_Log(const char *fmt, ...) PRINTF_DECL(1, 2);


/*
 * How many old log files to keep around.
 *
 * ESX needs more old log files for bug fixing (and vmotion).
 */

#if defined(VMX86_SERVER)
#define LOG_DEFAULT_KEEPOLD 6
#else
#define LOG_DEFAULT_KEEPOLD 3
#endif

#define LOG_NO_BPS_LIMIT               0xFFFFFFFF
#define LOG_NO_ROTATION_SIZE           0
#define LOG_NO_THROTTLE_THRESHOLD      0

#if defined(VMX86_RELEASE)
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

#endif /* !VMM */
#endif /* VMWARE_LOG_H */

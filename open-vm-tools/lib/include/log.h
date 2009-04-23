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
#define INCLUDE_ALLOW_VMMEXT
#include "includeCheck.h"

#include <stdarg.h>


typedef void (LogBasicFunc)(const char *fmt, va_list args);

typedef struct
{
   const char *fileName;             // File name, if known
   const char *config;               // Config variable to look up
   const char *suffix;               // Suffix to generate log file name
   const char *appName;              // App name for log header
   const char *appVersion;           // App version for log header
   Bool logging;                     // Logging is enabled or not
   Bool append;                      // Append to log file
   unsigned int keepOld;             // Number of old logs to keep
   unsigned int throttleThreshold;   // Threshold for throttling
   unsigned int throttleBytesPerSec; // BPS for throttle
   Bool switchFile;                  // Switch the initial log file
   unsigned int rotateSize;          // Size at which log should be rotated
} LogInitParams;

EXTERN void Log_GetInitDefaults(const char *fileName, const char *config,
                                const char *suffix, LogInitParams *params);

EXTERN Bool Log_Init(const char *fileName, const char *config,
                     const char *suffix);
EXTERN Bool Log_InitForApp(const char *fileName, const char *config,
                           const char *suffix, const char *appName,
                           const char *appVersion);
EXTERN Bool Log_InitEx(const LogInitParams *params);
EXTERN void Log_Exit(void);
EXTERN void Log_SetConfigDir(const char *configDir);
EXTERN void Log_SetLockFunc(void (*f)(Bool locking));
EXTERN void Log_WriteLogFile(const char *msg);

EXTERN Bool Log_Enabled(void);
EXTERN const char *Log_GetFileName(void);

EXTERN void Log_Flush(void);
EXTERN void Log_SetAlwaysKeep(Bool alwaysKeep);
EXTERN Bool Log_RemoveFile(Bool alwaysRemove);
EXTERN void Log_DisableThrottling(void);

EXTERN Bool Log_GetQuietWarning(void);
EXTERN void Log_SetQuietWarning(Bool quiet);
EXTERN void Log_RegisterBasicFunctions(LogBasicFunc *log,
                                       LogBasicFunc *warning);

EXTERN void Log_BackupOldFiles(const char *fileName);
EXTERN void Log_UpdateState(Bool enable, Bool append, unsigned keepOld,
                            size_t rotateSize, Bool fastRotation);
EXTERN Bool Log_SwitchFile(const char *fileName, const char *config, Bool copy);
EXTERN Bool Log_CopyFile(const char *fileName);

/* Logging that uses the custom guest throttling configuration. */
EXTERN void GuestLog_Init(void);
EXTERN void GuestLog_Log(const char *fmt, ...) PRINTF_DECL(1, 2);

// I left DEFAULT_DEBUG in here because the vmx is still using it for now
#if defined(VMX86_DEVEL)
#define DEFAULT_MONITOR    "debug"
#define DEFAULT_DEBUG      1
#elif defined(VMX86_BETA)
#define DEFAULT_MONITOR    "debug"
#define DEFAULT_DEBUG      1
#else
#define DEFAULT_MONITOR    "release"
#define DEFAULT_DEBUG      0
#endif


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

#define LOG_NO_BPS_LIMIT        0xFFFFFFFF

#ifdef VMX86_RELEASE
#define LOG_DEFAULT_THROTTLE_BPS       1000
#else
#define LOG_DEFAULT_THROTTLE_BPS       LOG_NO_BPS_LIMIT
#endif

#define LOG_DEFAULT_THROTTLE_THRESHOLD 1000000


/*
 * Debugging
 */

EXTERN void Log_HexDump(const char *prefix, const uint8 *data, int size);
EXTERN void Log_Time(VmTimeType *time, int count, const char *message);
EXTERN void Log_Histogram(uint32 n, uint32 histo[], int nbuckets,
			  const char *message, int *count, int limit);

#endif /* VMWARE_LOG_H */

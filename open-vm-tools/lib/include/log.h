/*********************************************************
 * Copyright (C) 1998-2015 VMware, Inc. All rights reserved.
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
#include "productState.h"

#include <stdarg.h>


/*
 * The bora/lib Log Facility log level model.
 * This the same as the vmacore/hostd Log Facility.
 *
 * The VMW_LOG_BASE is chosen to ensure that on all platforms commonly
 * used system logger values will be invalid and the errant usage caught.
 */

#define VMW_LOG_BASE     100
#define VMW_LOG_PANIC    (VMW_LOG_BASE     +  0) // highest priority
#define VMW_LOG_ERROR    (VMW_LOG_BASE     +  5)
#define VMW_LOG_WARNING  (VMW_LOG_BASE     + 10) // <= goes to stderr by default
#define VMW_LOG_AUDIT    (VMW_LOG_BASE     + 15) // *ALWAYS* output to the log
#define VMW_LOG_INFO     (VMW_LOG_BASE     + 20) // <= goes to log by default
#define VMW_LOG_VERBOSE  (VMW_LOG_BASE     + 25)
#define VMW_LOG_TRIVIA   (VMW_LOG_BASE     + 30) 
#define VMW_LOG_DEBUG_00 (VMW_LOG_BASE     + 35) // noisiest level of debugging
#define VMW_LOG_DEBUG_01 (VMW_LOG_DEBUG_00 +  1)
#define VMW_LOG_DEBUG_02 (VMW_LOG_DEBUG_00 +  2)
#define VMW_LOG_DEBUG_03 (VMW_LOG_DEBUG_00 +  3)
#define VMW_LOG_DEBUG_04 (VMW_LOG_DEBUG_00 +  4)
#define VMW_LOG_DEBUG_05 (VMW_LOG_DEBUG_00 +  5)
#define VMW_LOG_DEBUG_06 (VMW_LOG_DEBUG_00 +  6)
#define VMW_LOG_DEBUG_07 (VMW_LOG_DEBUG_00 +  7)
#define VMW_LOG_DEBUG_08 (VMW_LOG_DEBUG_00 +  8)
#define VMW_LOG_DEBUG_09 (VMW_LOG_DEBUG_00 +  9)
#define VMW_LOG_DEBUG_10 (VMW_LOG_DEBUG_00 + 10) // lowest priority; least noisy

void LogV(uint32 routing,
          const char *fmt,
          va_list args);

void Log_Level(uint32 routing,
               const char *fmt,
               ...) PRINTF_DECL(2, 3);


/*
 * Handy wrapper functions.
 *
 * Log -> VMW_LOG_INFO
 * Warning -> VMW_LOG_WARNING
 *
 * TODO: even Log and Warning become wrapper functions around LogV.
 */

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
struct Dictionary;
struct CfgInterface;

typedef struct LogOutput LogOutput;

struct CfgInterface *Log_CfgInterface(void);

int32 Log_SetStderrLevel(int32 level);

int32 Log_GetStderrLevel(void);

LogOutput *Log_NewStdioOutput(const char *appPrefix,
                              struct Dictionary *params,
                              struct CfgInterface *cfgIf);

LogOutput *Log_NewSyslogOutput(const char *appPrefix,
                               const char *instanceName,
                               struct Dictionary *params,
                               struct CfgInterface *cfgIf);

LogOutput *Log_NewFileOutput(const char *appPrefix,
                             const char *instanceName,
                             struct Dictionary *params,
                             struct CfgInterface *cfgIf);

typedef void (LogCustomMsgFunc)(int level,
                                const char *msg);

LogOutput *Log_NewCustomOutput(LogCustomMsgFunc *msgFunc,
                               int minLogLevel);

Bool Log_FreeOutput(LogOutput *toOutput);

Bool Log_AddOutput(LogOutput *output);

Bool Log_ReplaceOutput(LogOutput *fromOutput,
                       LogOutput *toOutput,
                       Bool copyOver);

int32 Log_SetOutputLevel(LogOutput *output,
                         int32 level);

/*
 * The most common Log Facility client usage is via the "InitWith" functions.
 * These functions - not the "Int" versions - handle informing the Log
 * Facility of the ProductState (product description) via inline code. This is
 * done to avoid making the Log Facility depend on the ProductState library -
 * the product should have the dependency, not an underlying library.
 *
 * In complex cases, where an "InitWith" is not sufficient and Log_AddOutput
 * must be used directly, the client should call Log_SetProductState, passing
 * the appropriate parameters, so the log file header information will be
 * correct.
 */

void Log_SetProductInfo(const char *appName,
                        const char *appVersion,
                        const char *buildNumber,
                        const char *compilationOption);

LogOutput *Log_InitWithCustomInt(struct CfgInterface *cfgIf,
                                 LogCustomMsgFunc *msgFunc,
                                 int minLogLevel);

static INLINE LogOutput *
Log_InitWithCustom(struct CfgInterface *cfgIf,
                   LogCustomMsgFunc *msgFunc,
                   int minLogLevel)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());

   return Log_InitWithCustomInt(cfgIf, msgFunc, minLogLevel);
}

LogOutput *Log_InitWithFileInt(const char *appPrefix,
                               struct Dictionary *dict,
                               struct CfgInterface *cfgIf,
                               Bool boundNumFiles);

static INLINE LogOutput *
Log_InitWithFile(const char *appPrefix,
                 struct Dictionary *dict,
                 struct CfgInterface *cfgIf,
                 Bool boundNumFiles)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());

   return Log_InitWithFileInt(appPrefix, dict, cfgIf, boundNumFiles);
}

LogOutput *Log_InitWithFileSimpleInt(const char *appPrefix,
                                     struct CfgInterface *cfgIf,
                                     const char *fileName);

static INLINE LogOutput *
Log_InitWithFileSimple(const char *fileName,
                       const char *appPrefix)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());

   return Log_InitWithFileSimpleInt(appPrefix, Log_CfgInterface(), fileName);
}

LogOutput *Log_InitWithSyslogInt(const char *appPrefix,
                                 struct Dictionary *dict,
                                 struct CfgInterface *cfgIf);

static INLINE LogOutput *
Log_InitWithSyslog(const char *appPrefix,
                   struct Dictionary *dict,
                   struct CfgInterface *cfgIf)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());

   return Log_InitWithSyslogInt(appPrefix, dict, cfgIf);
}

LogOutput *Log_InitWithSyslogSimpleInt(const char *appPrefix,
                                       struct CfgInterface *cfgIf,
                                       const char *syslogID);

static INLINE LogOutput *
Log_InitWithSyslogSimple(const char *syslogID,
                         const char *appPrefix)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());

   return Log_InitWithSyslogSimpleInt(appPrefix, Log_CfgInterface(), syslogID);
}

void Log_Exit(void);

Bool Log_Outputting(void);

const char *Log_GetFileName(void);
const char *Log_GetOutputFileName(LogOutput *output);

void Log_SkipLocking(Bool skipLocking);
void Log_DisableThrottling(void);
uint32 Log_MaxLineLength(void);

size_t Log_MakeTimeString(Bool millisec,
                          char *buf,
                          size_t max);

typedef Bool (LogOwnerFunc)(void *userData,
                            const char *fileName);

Bool Log_BoundNumFiles(struct LogOutput *output,
                       LogOwnerFunc *func,
                       void *userData);

/* Logging that uses the custom guest throttling configuration. */
void GuestLog_Init(void);
void GuestLog_Log(const char *fmt,
                  ...) PRINTF_DECL(1, 2);


#if defined(VMX86_SERVER)
#define LOG_KEEPOLD 6  // Old log files to keep around; ESX value
#else
#define LOG_KEEPOLD 3  // Old log files to keep around; non-ESX value
#endif

#define LOG_NO_KEEPOLD                 0  // Keep no old log files
#define LOG_NO_ROTATION_SIZE           0  // Do not rotate based on file size
#define LOG_NO_THROTTLE_THRESHOLD      0  // No threshold before throttling
#define LOG_NO_BPS_LIMIT               0xFFFFFFFF  // unlimited input rate


/*
 * Debugging
 */

void Log_HexDump(const char *prefix,
                 const void *data,
                 size_t size);

void Log_HexDumpLevel(uint32 routing,
                      const char *prefix,
                      const void *data,
                      size_t size);

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

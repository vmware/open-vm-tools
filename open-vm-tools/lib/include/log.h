/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif


/*
 * The bora/lib Log Facility log level model.
 *
 * Each log entry has a level associated with it. The Log Facility filters
 * entries as they arrive by their level; only levels equal to or below
 * (smaller values) the filter level will be accepted by the Log Facility for
 * processing.
 *
 * By default, the Log Facility will output any entry which is submitted at
 * level VMW_LOG_WARNING and below to the "standard error". This may be
 * controlled via Log_SetStderrLevel (see function header) or configuration
 * parameter (see comments in log.c).
 *
 * The VMW_LOG_AUDIT level is used to log something that requires an audit
 * at a later date. It is *ALWAYS* logged and *NEVER* outputs to the "standard
 * error".
 *
 * NOTE: The log levels must have sequential values with no "holes" and start
 *       with zero (0).
 *
 *      Level              Comments
 *-------------------------------------------------
 */

typedef enum {
   VMW_LOG_AUDIT    = 0,   // ALWAYS LOGGED; NO STDERR
   VMW_LOG_PANIC    = 1,   // Quietest level
   VMW_LOG_ERROR    = 2,
   VMW_LOG_WARNING  = 3,
   VMW_LOG_NOTICE   = 4,
   VMW_LOG_INFO     = 5,   // Default filter level for release builds
   VMW_LOG_VERBOSE  = 6,   // Default filter level for debug builds
   VMW_LOG_TRIVIA   = 7,
   VMW_LOG_DEBUG_00 = 8,   // least noisy debug level
   VMW_LOG_DEBUG_01 = 9,
   VMW_LOG_DEBUG_02 = 10,
   VMW_LOG_DEBUG_03 = 11,  // debug levels grow
   VMW_LOG_DEBUG_04 = 12,  // increasingly noisy
   VMW_LOG_DEBUG_05 = 13,  // as the debug number
   VMW_LOG_DEBUG_06 = 14,  // increases
   VMW_LOG_DEBUG_07 = 15,
   VMW_LOG_DEBUG_08 = 16,
   VMW_LOG_DEBUG_09 = 17,
   VMW_LOG_DEBUG_10 = 18,  // Noisiest level
} VmwLogLevel;

#if defined(VMX86_DEBUG) || defined(VMX86_DEVEL)
   #define LOG_FILTER_DEFAULT_LEVEL VMW_LOG_VERBOSE
#else
   #define LOG_FILTER_DEFAULT_LEVEL VMW_LOG_INFO
#endif

/*
 * The "routing" parameter contains the level in the low order bits; the
 * higher order bits specify the module where the log call came from.
 */

#define VMW_LOG_LEVEL_BITS 5  // Log level bits (32 levels max)
#define VMW_LOG_LEVEL_MASK ((int)(1 << VMW_LOG_LEVEL_BITS) - 1)

#define VMW_LOG_LEVEL(routing)  ((routing) & VMW_LOG_LEVEL_MASK)
#define VMW_LOG_MODULE(routing) (((routing) >> VMW_LOG_LEVEL_BITS))

void
LogV(uint32 routing,
     const char *fmt,
     va_list args);

void
Log_Level(uint32 routing,
          const char *fmt,
          ...) PRINTF_DECL(2, 3);

/*
 * Log     = Log_Info
 * Warning = Log_Warning
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
Log_Warning(const char *fmt,
            ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_WARNING, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Notice(const char *fmt,
           ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_NOTICE, fmt, ap);
   va_end(ap);
}


static INLINE void PRINTF_DECL(1, 2)
Log_Info(const char *fmt,
         ...)
{
   va_list ap;

   va_start(ap, fmt);
   LogV(VMW_LOG_INFO, fmt, ap);
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

/*
 * Structure contains all the pointers to where value can be updated
 * Making VmxStats as a struct has its own advantage, such as updating
 * 'droppedChars' from the struct instead within LogFile.
 */
typedef struct {
   uint64 *numTimesDrop; // total time char dropped
   uint64 *droppedChars; // Number of drop char
   uint64 *bytesLogged;  // Total logged
} VmxStatsInfo;


typedef struct LogOutput LogOutput;

struct CfgInterface *
Log_CfgInterface(void);

int32
Log_SetStderrLevel(int32 level);

int32
Log_GetStderrLevel(void);

LogOutput *
Log_NewStdioOutput(const char *appPrefix,
                   struct Dictionary *params,
                   struct CfgInterface *cfgIf);

LogOutput *
Log_NewSyslogOutput(const char *appPrefix,
                    const char *instanceName,
                    struct Dictionary *params,
                    struct CfgInterface *cfgIf);

LogOutput *
Log_NewFileOutput(const char *appPrefix,
                  const char *instanceName,
                  struct Dictionary *params,
                  struct CfgInterface *cfgIf);

typedef void (LogCustomMsgFunc)(int level,
                                const char *msg);

LogOutput *
Log_NewCustomOutput(const char *instanceName,
                    LogCustomMsgFunc *msgFunc,
                    int minLogLevel);

Bool
Log_FreeOutput(LogOutput *toOutput);

Bool
Log_AddOutput(LogOutput *output);

Bool
Log_ReplaceOutput(LogOutput *fromOutput,
                  LogOutput *toOutput,
                  Bool copyOver);

int32
Log_SetOutputLevel(LogOutput *output,
                   int32 level);

Bool
Log_SetVmxStatsData(LogOutput *output,
                    VmxStatsInfo *vmxStats);

/*
 * The most common Log Facility client usage is via the "InitWith" functions.
 * These functions - not the "Int" versions - handle informing the Log
 * Facility of the ProductState (product description) via inline code. This is
 * done to avoid making the Log Facility depend on the ProductState library -
 * the product should have the dependency, not an underlying library.
 *
 * In complex cases, where an "InitWith" is not sufficient and Log_AddOutput
 * must be used directly, the client should call Log_SetProductInfo, passing
 * the appropriate parameters, so the log file header information will be
 * correct.
 */

void
Log_SetProductInfo(const char *appName,
                   const char *appVersion,
                   const char *buildNumber,
                   const char *compilationOption);

static INLINE void
Log_SetProductInfoSimple(void)
{
   Log_SetProductInfo(ProductState_GetName(),
                      ProductState_GetVersion(),
                      ProductState_GetBuildNumberString(),
                      ProductState_GetCompilationOption());
}


LogOutput *
Log_InitWithCustomInt(struct CfgInterface *cfgIf,
                      LogCustomMsgFunc *msgFunc,
                      int minLogLevel);


static INLINE LogOutput *
Log_InitWithCustom(struct CfgInterface *cfgIf,
                   LogCustomMsgFunc *msgFunc,
                   int minLogLevel)
{
   Log_SetProductInfoSimple();

   return Log_InitWithCustomInt(cfgIf, msgFunc, minLogLevel);
}

LogOutput *
Log_InitWithFileInt(const char *appPrefix,
                    struct Dictionary *dict,
                    struct CfgInterface *cfgIf,
                    Bool boundNumFiles);

static INLINE LogOutput *
Log_InitWithFile(const char *appPrefix,
                 struct Dictionary *dict,
                 struct CfgInterface *cfgIf,
                 Bool boundNumFiles)
{
   Log_SetProductInfoSimple();

   return Log_InitWithFileInt(appPrefix, dict, cfgIf, boundNumFiles);
}

LogOutput *
Log_InitWithFileSimpleInt(const char *appPrefix,
                          struct CfgInterface *cfgIf,
                          const char *fileName);

static INLINE LogOutput *
Log_InitWithFileSimple(const char *fileName,
                       const char *appPrefix)
{
   Log_SetProductInfoSimple();

   return Log_InitWithFileSimpleInt(appPrefix, Log_CfgInterface(), fileName);
}

LogOutput *
Log_InitWithSyslogInt(const char *appPrefix,
                      struct Dictionary *dict,
                      struct CfgInterface *cfgIf);

static INLINE LogOutput *
Log_InitWithSyslog(const char *appPrefix,
                   struct Dictionary *dict,
                   struct CfgInterface *cfgIf)
{
   Log_SetProductInfoSimple();

   return Log_InitWithSyslogInt(appPrefix, dict, cfgIf);
}

LogOutput *
Log_InitWithSyslogSimpleInt(const char *appPrefix,
                            struct CfgInterface *cfgIf,
                            const char *syslogID);

static INLINE LogOutput *
Log_InitWithSyslogSimple(const char *syslogID,
                         const char *appPrefix)
{
   Log_SetProductInfoSimple();

   return Log_InitWithSyslogSimpleInt(appPrefix, Log_CfgInterface(), syslogID);
}

LogOutput *
Log_InitWithStdioSimpleInt(const char *appPrefix,
                           struct CfgInterface *cfgIf,
                           const char *minLevel,
                           Bool withLinePrefix);

static INLINE LogOutput *
Log_InitWithStdioSimple(const char *appPrefix,
                        const char *minLevel,
                        Bool withLinePrefix)
{
   Log_SetProductInfoSimple();

   return Log_InitWithStdioSimpleInt(appPrefix, Log_CfgInterface(), minLevel,
                                     withLinePrefix);
}

void
Log_Exit(void);

Bool
Log_Outputting(void);

Bool
Log_IsLevelOutputting(int level);

const char *
Log_GetFileName(void);

const char *
Log_GetOutputFileName(LogOutput *output);

void
Log_SkipLocking(Bool skipLocking);

void
Log_DisableThrottling(void);

void
Log_DisableVmxStats(void);

uint32
Log_MaxLineLength(void);

size_t
Log_MakeTimeString(Bool millisec,
                   char *buf,
                   size_t max);

typedef Bool (LogOwnerFunc)(void *userData,
                            const char *fileName);

Bool
Log_BoundNumFiles(struct LogOutput *output,
                LogOwnerFunc *func,
                void *userData);

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
 * Assemble a line.
 */

void *
Log_BufBegin(void);

void
Log_BufAppend(void *acc,
              const char *fmt,
              ...) PRINTF_DECL(2, 3);

void
Log_BufEndLevel(void *acc,
                uint32 routing);


/*
 * Debugging
 */

void
Log_HexDump(const char *prefix,
            const void *data,
            size_t size);

void
Log_HexDumpLevel(uint32 routing,
                 const char *prefix,
                 const void *data,
                 size_t size);

void
Log_Time(VmTimeType *time,
         int count,
         const char *message);

void
Log_Histogram(uint32 n,
              uint32 histo[],
              int nbuckets,
              const char *message,
              int *count,
              int limit);

typedef Bool (GetOpId)(size_t maxStringLen,
                       char *opId);

void
Log_RegisterOpIdFunction(GetOpId *getOpIdFunc);

void Log_LoadModuleFilters(struct CfgInterface *cfgIf);

#endif /* !VMM */

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* VMWARE_LOG_H */

/*
 * To use the Log Facility module specific filters:
 *
 *  1) Use LogV or Log_Level and use the LOG_ROUTING_BITS macro.
 *
 *  2) Have LOGLEVEL_MODULE defined before the include of "log.h".
 *
 *     For many files, this involves moving the include "log.h" after
 *     the include of "loglevel_user.h".
 */

#if !defined(VMW_LOG_MODULE_LEVELS)
   #include "vm_basic_defs.h"
   #include "loglevel_userVars.h"

   #define LOGFACILITY_MODULEVAR(mod) XCONC(_logFacilityModule_, mod)

   enum LogFacilityModuleValue {
      LOGLEVEL_USER(LOGFACILITY_MODULEVAR)
   };

   #define VMW_LOG_MODULE_LEVELS
#endif

#if defined(LOG_ROUTING_BITS)
   #undef LOG_ROUTING_BITS
#endif

#if defined(LOGLEVEL_MODULE)
   /* Module bits of zero (0) indicate no module has been specified */
   #define LOG_ROUTING_BITS(level) \
      (((LOGFACILITY_MODULEVAR(LOGLEVEL_MODULE) + 1) << VMW_LOG_LEVEL_BITS) | level)
#else
   #define LOG_ROUTING_BITS(level) (level)
#endif



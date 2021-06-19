/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif


/**
 * Log Facility
 *
 * The Log Facility exists to record events of program execution for purposes
 * of auditing, debugging, and monitoring. Any non-trivial program should use
 * logging, to enable those purposes.
 *
 * Events recorded by the Log Facility (i.e. calls to here-declared functions)
 * are automatically filtered, time-stamped, and persisted.
 *
 * Configuration for field engineers is documented at
 *   https://wiki.eng.vmware.com/PILogFacility
 *
 * For full details on configurable parameters and their semantics,
 * use the source, Luke -- starting at lib/log/logFacility.c
 *
 * The events are explicitly annotated by the developer (i.e. you)
 * with a level, an optional module, and a message.
 *
 * Notes:
 * - Log() is defined as Log_Info()
 * - Warning() is defined as Log_Warning()
 *
 * Log Level
 *
 * The Log Level indicates the (in)significance of the event, with larger
 * numbers indicating lesser significance. The level, whether explicit
 * (e.g. Log_Level(level, ...)) or implicit (e.g. Log_Info(...)),
 * should be chosen with some care. A Log_Info() message containing the word
 * "warning" or "error" is almost certainly mis-routed.
 *
 * The following rules of thumb provide a rough guide to choice of level.
 * * VMW_LOG_AUDIT -- always logged, for auditing purposes
 *  + change to authorization
 *  + change to configuration
 * * VMW_LOG_PANIC -- system broken; cannot exit gracefully
 *  + wild pointer; corrupt arena
 *  + error during error exit
 * * VMW_LOG_ERROR -- system broken; must exit
 *  + required resource inaccessible (memory; storage; network)
 *  + incorrigible internal inconsistency
 * * VMW_LOG_WARNING -- unexpected condition; may require immediate attention
 *  + inconsistency corrected or ignored
 *  + timeout or slow operation
 * * VMW_LOG_NOTICE -- unexpected condition; may require eventual attention
 *  + missing config; default used
 *  + lower level error ignored
 * * VMW_LOG_INFO -- expected condition
 *  + non-standard configuration
 *  + alternate path taken (e.g. on lower level error)
 * * VMW_LOG_VERBOSE -- normal operation; potentially useful information
 *  + system health observation, for monitoring
 *  + unexpected non-error state
 * * VMW_LOG_TRIVIA -- normal operation; excess information
 *  + vaguely interesting note
 *  + anything else the developer thinks might be useful
 * * VMW_LOG_DEBUG_* -- flow and logic tracing
 *  + routine entry, with parameters; routine exit, with return value
 *  + intermediate values or decisions
 *
 * Log Module
 *
 * The Log routines accept an explicit level (see below)
 * as well as an optional module parameter.
 * This affords another degree of freedom in filtering.
 *
 * To use the module parameter:
 *
 * (0) The module identifier must already exist, or be invented and defined,
 *     in public/loglevel_userVars.h
 *
 * (1) Define LOGLEVEL_MODULE before the include of "log.h".
 *     The value must be a valid module identifier (see above).
 *     It is a good idea to see if "log.h" is included more than once,
 *     and if so, to remove any extra inclusions.
 *
 *     If all uses of LOG() are converted to use Log_ variants,
 *     remember to remove "loglevel_user.h".
 *
 * (2) Pass the module information to the Log Facility.
 *     LOGLEVEL_MODULE is actually encoded in the level parameter by
 *     the LOG_ROUTING_BITS(level) macro.
 *
 *     Use
 *        Log_LevelModule(level, ...);
 *     OR
 *        LogV_Module(level, ...);
 *     OR
 *        Log_Level(LOG_ROUTING_BITS(level), ...);
 *     OR
 *        Log_V(LOG_ROUTING_BITS(level), ...);
 *
 * Log Message
 *
 * Every Log message should be unique, unambiguously describing the event
 * being logged, and should include all relevant data, in human-readable form.
 * Source line number is *not* useful.
 * + printf-style arguments
 * + function name as prefix (controversial)
 * + format pure number (e.g. error number) in decimal
 * + format bitfield (e.g. compound error code) in hex
 * + format disk size or offset in hex; specify units if not bytes
 * + quote string arguments (e.g. pathnames) which can contain spaces
 *
 * Log Filter
 *
 * The Log Facility filters events as they arrive, by level and by module,
 * discarding events above the configured filter level.
 * This is similar to how syslog and the vmacore logger handle levels.
 *
 * There is a global filter and assorted module filters.
 * The global filter is used for all entries to the Log Facility
 * UNLESS a module is specified.
 *
 * Module filters are limited to a module (name specific) context;
 * they do not fall within the global context. This allows entries to be
 * controlled by module AND level. This is similar to LOG(), for those
 * familiar with it ... except that module filters are available in
 * all build types.
 *
 * Each module is a separate configuration namespace, with module-specific
 * filter levels. The default module filter level is none (VMW_LOG_AUDIT)
 * i.e. module filters by default discard all events (just like LOG()).
 *
 * Regardless of what level of filtering is specified, the VMW_LOG_AUDIT
 * level is used to log something that requires an audit at a later date.
 * It is *ALWAYS* logged and *NEVER* outputs to the "standard error".
 */
/*
 * Level            Value    Comments
 *---------------------------------------------------------------------------
 */

typedef enum {
   VMW_LOG_AUDIT    = 0,  // Always output; never to stderr
   VMW_LOG_PANIC    = 1,  // Desperation
   VMW_LOG_ERROR    = 2,  // Irremediable error
   VMW_LOG_WARNING  = 3,  // Unexpected condition; may need immediate attention
   VMW_LOG_NOTICE   = 4,  // Unexpected condition; may need eventual attention
   VMW_LOG_INFO     = 5,  // Expected condition
   VMW_LOG_VERBOSE  = 6,  // Extra information
   VMW_LOG_TRIVIA   = 7,  // Excess information
   VMW_LOG_DEBUG_00 = 8,
   VMW_LOG_DEBUG_01 = 9,
   VMW_LOG_DEBUG_02 = 10,
   VMW_LOG_DEBUG_03 = 11,
   VMW_LOG_DEBUG_04 = 12,
   VMW_LOG_DEBUG_05 = 13,
   VMW_LOG_DEBUG_06 = 14,
   VMW_LOG_DEBUG_07 = 15,
   VMW_LOG_DEBUG_08 = 16,
   VMW_LOG_DEBUG_09 = 17,
   VMW_LOG_DEBUG_10 = 18,
   VMW_LOG_DEBUG_11 = 19,
   VMW_LOG_DEBUG_12 = 20,
   VMW_LOG_DEBUG_13 = 21,
   VMW_LOG_DEBUG_14 = 22,
   VMW_LOG_DEBUG_15 = 23,
   VMW_LOG_MAX      = 24,
} VmwLogLevel;

#if defined(VMX86_DEBUG)
   #define LOG_FILTER_DEFAULT_LEVEL VMW_LOG_VERBOSE
#else
   #define LOG_FILTER_DEFAULT_LEVEL VMW_LOG_INFO
#endif

#ifdef VMX86_SERVER
/* WORLD_MAX_OPID_STRING_SIZE */
#define LOG_MAX_OPID_LENGTH 128
#else
/* We do not expect long opIDs in non-ESX environments. 32 should be enough. */
#define LOG_MAX_OPID_LENGTH 32
#endif

/*
 * The "routing" parameter contains the level in the low order bits;
 * the higher order bits specify the module of the log call.
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
typedef struct {
   int32   legalLevelValue;
   char   *legalName;
   char   *levelIdStr;
} LogLevelData;

const LogLevelData *
Log_MapByLevel(VmwLogLevel level);

const LogLevelData *
Log_MapByName(const char *name);

typedef struct LogOutput LogOutput;

/* Forward decl */
struct Dictionary;
struct CfgInterface;

struct CfgInterface *
Log_CfgInterface(void);

int32
Log_SetStderrLevel(uint32 module,
                   int32 level);

int32
Log_GetStderrLevel(uint32 module);

int32
Log_SetLogLevel(uint32 module,
                int32 level);

int32
Log_GetLogLevel(uint32 module);

uint32
Log_LookupModuleNumber(const char *moduleName);

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

typedef struct {
   uint8 level;
   Bool  additionalLine;
   char  timeStamp[64];
   char  threadName[32];
   char  opID[LOG_MAX_OPID_LENGTH + 1];  // Will be empty string on hosted products
} LogLineMetadata;

typedef void (LogCustomMsgFuncEx)(const LogLineMetadata * const metadata,
                                  const char *msg);

LogOutput *
Log_NewCustomOutputEx(const char *instanceName,
                      LogCustomMsgFuncEx *msgFunc,
                      int minLogLevel);

#if defined(VMX86_SERVER)
LogOutput *
Log_NewEsxKernelLogOutput(const char *appPrefix,
                          struct Dictionary *params,
                          struct CfgInterface *cfgIf);

LogOutput *
Log_NewCrxSyslogOutput(const char *appPrefix,
                       struct Dictionary *params,
                       struct CfgInterface *cfgIf);
#endif

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
Log_IsEnabled(uint32 routing);

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
Log_BoundNumFiles(const LogOutput *output,
                  LogOwnerFunc *func,
                  void *userData);

typedef struct {
#if defined(_WIN32)
   HANDLE handle;
#else
   int fd;
#endif
} LogFileObject;

Bool
Log_GetFileObject(const LogOutput *output,
                  LogFileObject *result);

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

void
Log_LoadModuleFilters(const char *appPrefix,
                      struct CfgInterface *cfgIf);

long
Log_OffsetUtc(void);

#endif /* !VMM */

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* VMWARE_LOG_H */

/*
 * To use the Log Facility Module Specific Filters:
 *
 *  1) Modify the file (C/C++) slightly.
 *
 *     Define LOGLEVEL_MODULE before the include of "log.h". It's a good
 *     idea to see if "log.h" is included more than once and, if so, to
 *     remove any extra inclusions.
 *
 *     If all uses of LOG are converted to use the module-specific filters,
 *     remember to remove "loglevel_user.h".
 *
 *  2) Pass the LOGLEVEL_MODULE information to the Log Facility.
 *
 *     Use LogV_Module and/or Log_LevelModule.
 *
 *     OR
 *
 *     Use the LOG_ROUTING_BITS macro as part of a call to LogV and/or
 *     Log_Level.
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

/*
 * Helper functions for module level filters.
 */

#if defined(Log_LevelModule)
   #undef Log_LevelModule
#endif

#define Log_LevelModule(level, ...) \
   Log_Level(LOG_ROUTING_BITS(level), __VA_ARGS__)

#if defined(LogV_Module)
   #undef LogV_Module
#endif

#define LogV_Module(level, ...) \
   LogV(LOG_ROUTING_BITS(level), __VA_ARGS__)

#if defined(Log_IsEnabledModule)
   #undef Log_IsEnabledModule
#endif

#define Log_IsEnabledModule(level) \
   Log_IsEnabled(LOG_ROUTING_BITS(level))

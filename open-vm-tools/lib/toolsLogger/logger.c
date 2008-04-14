/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

/*
 * logger.c --
 *
 *    All purpose logging mechanism used by Tools user-level applications 
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#if !defined(N_PLAT_NLM) && !defined(_WIN32)                                              
#   include <syslog.h>                                                                       
#endif                                                                                    

#ifdef _MSC_VER
#   include <io.h>
#   include <windows.h>
#endif

#if defined(N_PLAT_NLM)                                                                   
#   include "vmwtool.h"                                                                   
#endif                              

#include "vm_version.h"
#include "vm_assert.h"
#include "str.h"
#include "vmware.h"
#include "rpcvmx.h"
#include "util.h"
#include "fileIO.h"
#include "system.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "toolsLogger.h"
#include "toolsLoggerInt.h"


/*
 * Internal function prototypes
 */
static void ToolsLoggerToFile(const char *str);
static void ToolsLoggerToHost(const char *str);
static void ToolsLoggerToSyslog(int type, const char *str);
static void ToolsLoggerToStderr(const char *str);
static void ToolsLoggerToConsole(const char *str);
static Bool ToolsLoggerGetDictEntryBool(ToolsLogType logType, 
                                        ToolsLogSink logSink, 
                                        Bool defaultVal);
static Bool ToolsLoggerGetLogFilePath(char *path, int len);

static char *logTypePrefix[] = {
   "PANIC",
   "WARNING",
   "LOG"
};

#if !defined(N_PLAT_NLM) && !defined(_WIN32)
static const int syslogFlag[TOOLSLOG_TYPE_LAST] = {
   [TOOLSLOG_TYPE_PANIC]     = LOG_EMERG,
   [TOOLSLOG_TYPE_WARNING]   = LOG_WARNING,
   [TOOLSLOG_TYPE_LOG]       = LOG_INFO
};
#endif

static Bool filterMatrix[TOOLSLOG_TYPE_LAST][TOOLSLOG_SINK_LAST] = {
   /* file, console, syslog, host,  stderr */
   {  TRUE, TRUE,    TRUE,   TRUE,  TRUE},  /* PANIC */
   {  TRUE, FALSE,   TRUE,   TRUE,  TRUE},  /* WARNING */
   {  TRUE, FALSE,   FALSE,  FALSE, FALSE}  /* LOG */
};

/* Map TOOLSLOG_TYPE_x to string */
static const char *logTypeName[] = {
   "panic",     /* PANIC */
   "warning",   /* WARNING */
   "log"        /* LOG */
};

/* Map TOOLSLOG_SINK_x to string */
static const char *logSinkName[] = {
   "file",      /* FILE */
   "console",   /* CONSOLE */
   "syslog",    /* SYSLOG */
   "host",      /* HOST */
   "stderr"     /* STDERR */
};

static GuestApp_Dict *gConfDict = NULL;
static FileIODescriptor gFdLog;
static const char *gProgName = NULL; 
static Bool gInVirtualWorld = FALSE;

#if !defined(N_PLAT_NLM) && !defined(_WIN32)
static const char *gSyslogIdent = NULL;
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLogger_Init --
 * 
 *    Init the logger. An application has to call ToolsLogger_init() before
 *    calls ToolsLogger_log.
 *
 * Results:
 *    TRUE/FALSE.
 *
 * Side effects:
 *    The log file is opened. gConfDict, gProgName, and gSyslogIdent are
 *    initialized with the passed-in parameters.
 *
 *-----------------------------------------------------------------------------
 */

Bool 
ToolsLogger_Init(const char *progName,      // IN
                 GuestApp_Dict *confDict)   // IN
{
   FileIOResult rval;
   ToolsLogType logType;
   ToolsLogSink logSink;
   char path[PATH_MAX];

   ASSERT(progName);
   ASSERT(confDict);

   gProgName = progName;
   gConfDict = confDict;

   /* Initialize filterMatrix */
   for (logType = TOOLSLOG_TYPE_PANIC; logType < TOOLSLOG_TYPE_LAST; logType++) {
      for (logSink = TOOLSLOG_SINK_FILE; logSink < TOOLSLOG_SINK_LAST; logSink++) {
         filterMatrix[logType][logSink] = ToolsLoggerGetDictEntryBool(logType, logSink,
                                             filterMatrix[logType][logSink]);
      }
   }

   /* Enable logging to host if possible. */
   if (VmCheck_IsVirtualWorld()) {
      gInVirtualWorld = TRUE;
   }

   /* Open syslog if it is unix */
#if !defined(N_PLAT_NLM) && !defined(_WIN32)
   gSyslogIdent = progName;
   openlog(gSyslogIdent, 0, LOG_USER);
#endif

   /* Open (create if not exist) the log file */
   if (!ToolsLoggerGetLogFilePath(path, sizeof path)) {
      return FALSE;
   }
   
   FileIO_Invalidate(&gFdLog); 
   rval = FileIO_Open(&gFdLog, path, FILEIO_OPEN_ACCESS_WRITE, FILEIO_OPEN_CREATE);
   if (rval != FILEIO_SUCCESS) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 * ToolsLogger_Log --
 *
 *    Wrapper for ToolsLogger_LogV.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsLogger_Log(ToolsLogType type,      // IN: message type
                const char *fmt,        // IN: format string
                ...)                    // IN: arguments
{
   va_list args;

   va_start(args, fmt);
   ToolsLogger_LogV(type, fmt, args);
   va_end(args);
}


/*
 *-----------------------------------------------------------------------------
 * ToolsLogger_LogV --
 *
 *    Output a message to several logging destinations.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsLogger_LogV(ToolsLogType type,      // IN: message type
                 const char *fmt,        // IN: format string
                 va_list args)           // IN: arguments
{
   char msg[256];
   char str[256];
   const char *progName;

   ASSERT(type >= 0 && type < TOOLSLOG_TYPE_LAST);
   ASSERT(fmt);

   /* 
    * We need to be able to support some logging even without initialization.
    * This is because the logger initialization routines make calls to
    * Debug/Warning/Panic that most people will probably implement in terms
    * of this function.
    */

   /* Format the message */
   Str_Vsnprintf(msg, sizeof msg, fmt, args);

   if (gProgName) {
      progName = gProgName;
   } else {
      progName = "unknown";
   }

   Str_Snprintf(str, sizeof str, "[%s] %s: %s",
                progName,
                logTypePrefix[type],
                msg);

   /* Dispatch the message */
   if (filterMatrix[type][TOOLSLOG_SINK_FILE]) {
      ToolsLoggerToFile(str);
   }

   if (filterMatrix[type][TOOLSLOG_SINK_STDERR]) {
      ToolsLoggerToStderr(str);
   }

   if (filterMatrix[type][TOOLSLOG_SINK_CONSOLE]) {
      ToolsLoggerToConsole(str);
   }

   if (filterMatrix[type][TOOLSLOG_SINK_HOST]) {
      ToolsLoggerToHost(str);
   }

   if (filterMatrix[type][TOOLSLOG_SINK_SYSLOG]) {
      ToolsLoggerToSyslog(type, str);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLogger_Cleanup --
 *
 *    Reclaim resource used by the tools logger. 
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The log file and the syslog are closed. All global pointers are
 *    reset.
 *
 *
 *-----------------------------------------------------------------------------
 */

void 
ToolsLogger_Cleanup(void)
{
#if !defined(N_PLAT_NLM) && !defined(_WIN32)
   if (gProgName != NULL) {
      closelog();
      gSyslogIdent = NULL;
   }
#endif
   gProgName = NULL;
   gConfDict = NULL;

   FileIO_Close(&gFdLog);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLoggerToFile --
 *
 *    Write the message to the log file. Does nothing if we haven't yet
 *    initialized the logging infrastructure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsLoggerToFile(const char *str)              // IN: logging message
{
   size_t bytesWritten;                                                                   

   /* When uninitialized, do not log to file. */
   if (!gProgName) {
      return;
   }

   if (FileIO_Seek(&gFdLog, 0, FILEIO_SEEK_END) != FILEIO_SUCCESS) {
      return;
   }

   FileIO_Write(&gFdLog, str, strlen(str), &bytesWritten);                                
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLoggerToStderr --
 *
 *    Write the message to STDERR. For WIN32, we write to STDOUT. 
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsLoggerToStderr(const char *str)            // IN: logging message
{
#ifdef _WIN32
   printf("%s", str);
   fflush(stdout);
#else
   fprintf(stderr, "%s", str);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLoggerToHost --
 *
 *    Write the message to vmx log file. Only do so if we've proven to
 *    ourselves that we're actually in a guest.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsLoggerToHost(const char *str)              // IN: logging message
{
   if (gInVirtualWorld) {
      RpcVMX_Log("%s", str);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLoggerToConsole --
 *
 *    Write the message to the system console. (NO WIN32 SUPPORT)
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ToolsLoggerToConsole(const char *str)           // IN: logging message
{
#ifdef N_PLAT_NLM                                                                         
   OutputToScreenWithAttribute(VMwareScreen, BOLD_RED, "%s", str);                        
#else                                                                                     
#ifndef _WIN32                                                                            
   FileIOResult fr;                                                                       
   FileIODescriptor fd;                                                                  
   size_t bytesWritten;                                                                   
                                                                                           
   FileIO_Invalidate(&fd);                                                                 
                                                                                           
   fr = FileIO_Open(&fd, "/dev/console", FILEIO_OPEN_ACCESS_WRITE, 0);                     
   if (fr != FILEIO_SUCCESS) {                                                            
      goto done;                                                                          
   }                                                                                      
                                                                             
   fr = FileIO_Write(&fd, str, strlen(str), &bytesWritten);                                
   FileIO_Close(&fd);

done:
   return;
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsLoggerToSyslog --
 *
 *    Write the message to the syslog daemon. Does nothing if we haven't yet
 *    initialized the logging infrastructure.
 *
 * Side effects:
 *
 *    None
 *
 *-----------------------------------------------------------------------------
 */
static void
ToolsLoggerToSyslog(int type,                   // IN: message type
                    const char *str)            // IN: logging message
{
#ifdef _WIN32
   OutputDebugString(str);
#else
#ifndef N_PLAT_NLM

   /* When uninitialized, do not log to syslog. */
   if (gProgName != NULL) {
      syslog(LOG_USER|syslogFlag[type], "%s", str);
   }
#endif
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * ToolsLoggerGetDictEntryBool --
 *
 *    Get a logging dict entry's value & convert it to an Bool.
 *
 * Results:
 *    Returns TRUE is the dict entry is a case-insensitive match
 *    to "TRUE", FALSE otherwise. If there is no entry, for
 *    "name", return "defaultVal".
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Bool
ToolsLoggerGetDictEntryBool(ToolsLogType logType,   // IN: log type
                            ToolsLogSink logSink,   // IN: log sink
                            Bool defaultVal)        // IN: default value
{
   char name[64];
   const char *value; 

   Str_Snprintf(name, sizeof name, "log.%s.%s.enable",
      logTypeName[logType], logSinkName[logSink]);

   value = GuestApp_GetDictEntry(gConfDict, name);
   if (!value) {
      return defaultVal;
   }

#if  (defined N_PLAT_NLM || defined _WIN32)
   return (stricmp(value, "TRUE") == 0);
#else
   return (strcasecmp(value, "TRUE") == 0);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * ToolsLoggerGetLogFilePath --
 *
 *    Return the log file path. If the user specifies it in the config
 *    file, we load it from there. Otherwise, we use the default
 *    value defined by CONFVAL_LOGFILE_DEFAULT.
 *
 * Results:
 *    Return TRUE if the given buf 'path' is big enough to store the 
 *    path. Otherwise, return FALSE. 
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static Bool
ToolsLoggerGetLogFilePath(char *path,   // OUT
                          int size)     // IN
{
   const char *logFile;
   char *logPath;

   logFile = GuestApp_GetDictEntry(gConfDict, CONFNAME_LOGFILE);
   if (logFile) {
      Str_Strcpy(path, logFile, size);
   } else {
      char *defaultLogFile;

      logPath = GuestApp_GetLogPath();
      if (logPath == NULL) {
         return FALSE;
      }
      defaultLogFile = Str_Asprintf(NULL, "%s%c%s", logPath, DIRSEPC, 
                                    CONFVAL_LOGFILE_DEFAULT);
      if (defaultLogFile == NULL) {
         free(logPath);
         return FALSE;
      }

      Str_Strcpy(path, defaultLogFile, size);

      free(logPath);
      free(defaultLogFile);
   }

   return TRUE;
}

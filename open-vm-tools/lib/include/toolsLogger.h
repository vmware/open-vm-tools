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
 * toolsLogger.h --
 *
 *    All-purpose logging facility. The API is not intended to be
 *    called from the application code. Instead the application
 *    should re-implement DLWP with ToolsLogger_Log() (before, they
 *    use printf()/fprintf()/OutputDebugString()).
 *
 *    A sample:
 *
 *    #include "toolsLogger.h"
 *
 *    GuestApp_Dict *confDict = Conf_Load();
 *
 *    ToolsLogger_Init(progName, confDict);
 *
 *    Log("a log msg") // == ToolsLogger_Log(TOOLSLOG_TYPE_LOG, "a log msg");
 *    Warning("a warning msg"); // == ToolsLogger_Log(TOOLSLOG_TYPE_WARNING, 
 *                              //       "a warning msg");
 * 
 *    Panic("a panic msg");     // == ToolsLogger_Log(TOOLSLOG_TYPE_PANIC,
 *                              //       "a panic msg"); exit(1);
 *
 *    ToolsLogger_Cleanup();
 */


#ifndef __TOOLSLOGGER_H__
#   define __TOOLSLOGGER_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/*
 * Log types
 */
typedef enum {
   TOOLSLOG_TYPE_PANIC,
   TOOLSLOG_TYPE_WARNING,
   TOOLSLOG_TYPE_LOG,
   
   TOOLSLOG_TYPE_LAST       /* Must be the last one */
} ToolsLogType;


Bool ToolsLogger_Init(const char *progName, GuestApp_Dict *conf);
 
void ToolsLogger_Log(ToolsLogType type, 
                     const char *fmt, 
                     ...);

void ToolsLogger_LogV(ToolsLogType type,
                      const char *fmt,
                      va_list args);

void ToolsLogger_Cleanup(void);


#ifdef __cplusplus
}
#endif


#endif /* __TOOLSLOGGER_H__ */

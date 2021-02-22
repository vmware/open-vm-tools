/*********************************************************
 * Copyright (C) 2006-2021 VMware, Inc. All rights reserved.
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
 * deployPkgLog.c --
 *
 *    logger for both windows and posix versions of deployPkg
 */

#include "deployPkgInt.h"
#include "imgcust-common/log.h"
#include "util.h"
#include "file.h"
#include "str.h"
#include "vmware/tools/utils.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include "win32Access.h"
#endif

#define G_LOG_DOMAIN "deployPkg"

static FILE* _file = NULL;


/*
 *----------------------------------------------------------------------
 *
 * DeployPkgLog_Open --
 *
 *    Init the log. Creates a file in %temp%/vmware and
 *    opens it for writing. On linux, only root own r/w right.
 *    On error, the file will not be opened and logging
 *    will be disabled.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    On success, the log file is opened and managed by _file.
 *
 *----------------------------------------------------------------------
 */

void
DeployPkgLog_Open()
{
   char logPath[2048];

#ifdef _WIN32
   DWORD ret = GetTempPathA(sizeof logPath, logPath);

   if (ret == 0) {
      return;
   }

   Str_Strcat(logPath, "vmware-imc", sizeof logPath);
#else
   Str_Strcpy(logPath, "/var/log/vmware-imc", sizeof logPath);
#endif

   if (File_CreateDirectoryHierarchy(logPath, NULL)) {
      Str_Strcat(logPath, DIRSEPS "toolsDeployPkg.log", sizeof logPath);
      _file = fopen(logPath, "w");
      if (_file != NULL) {
#ifndef _WIN32
         setlinebuf(_file);
         (void) chmod(logPath, 0600);
#else
         (void)Win32Access_SetFileOwnerRW(logPath);
#endif
         DeployPkgLog_Log(log_debug, "## Starting deploy pkg operation");
      } else {
         g_debug("%s: failed to open DeployPkg log file: %s\n",
                   __FUNCTION__, logPath);
      }
   } else {
      g_debug("%s: failed to create DeployPkg log directory: %s\n",
                   __FUNCTION__, logPath);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * DeployPkgLog_Close --
 *
 *    Close the log.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    File descriptor managed by _s is closed
 *
 *----------------------------------------------------------------------
 */

void
DeployPkgLog_Close()
{
   if (_file != NULL) {
      DeployPkgLog_Log(log_debug, "## Closing log");
      fclose(_file);
      _file = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * DeployPkgLog_Log --
 *
 *    If the log file was opened successfully, write to it.
 *    Otherwise call the glib logger, messages are logged
 *    per tools logging configuration.
 *    Note: since g_error() is always fatal and terminate the application,
 *    log_error will be logged as g_warning to avoid terminating the
 *    application.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
DeployPkgLog_Log(int level,          // IN
                 const char *fmtstr, // IN
                 ...)                // IN
{
   va_list args;

   if (fmtstr == NULL) {
      return;
   }

   va_start(args, fmtstr);

   if (_file != NULL) {
      const char *logLevel;
      gchar *tstamp;
      size_t fmtstrLen = strlen(fmtstr);
      switch (level) {
         case log_debug:
            logLevel = "debug";
            break;
         case log_info:
            logLevel = "info";
            break;
         case log_warning:
            logLevel = "warning";
            break;
         case log_error:
            logLevel = "error";
            break;
         default:
            logLevel = "unknown";
            break;
      }

      tstamp = VMTools_GetTimeAsString();
      fprintf(_file, "[%s] [%8s] ",
              (tstamp != NULL) ? tstamp : "no time", logLevel);
      vfprintf(_file, fmtstr, args);
      if (fmtstrLen > 0 && fmtstr[fmtstrLen - 1] != '\n') {
         fprintf(_file, "\n");
      }
      g_free(tstamp);
   } else {
      GLogLevelFlags glogLevel;
      switch (level) {
         case log_debug:
            glogLevel = G_LOG_LEVEL_DEBUG;
            break;
         case log_info:
            glogLevel = G_LOG_LEVEL_INFO;
            break;
         case log_warning:
         case log_error:
            glogLevel = G_LOG_LEVEL_WARNING;
            break;
         default:
            glogLevel = G_LOG_LEVEL_INFO;
            break;
      }

      g_logv(G_LOG_DOMAIN, glogLevel, fmtstr, args);
   }

   va_end(args);
}


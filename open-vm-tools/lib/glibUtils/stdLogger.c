/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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

/**
 * @file stdLogger.c
 *
 * A very simplified version of a file logger that uses the standard output
 * streams (stdout / stderr).
 */

#include "glibUtils.h"
#include <stdio.h>

#if defined(_WIN32)
static GMutex gConsoleLock;
static gint gRefCount = 0;
#endif

typedef struct StdLogger {
   GlibLogger  handler;
#if defined(_WIN32)
   gboolean    attached;
#endif
} StdLogger;


/*
 *******************************************************************************
 * StdLoggerLog --                                                        */ /**
 *
 * Logs a message to stdout or stderr depending on its severity.
 *
 * @param[in] domain    Unused.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] data      Logger data.
 *
 *******************************************************************************
 */

static void
StdLoggerLog(const gchar *domain,
             GLogLevelFlags level,
             const gchar *message,
             gpointer data)
{
   gchar *local;
   FILE *dest = (level < G_LOG_LEVEL_MESSAGE) ? stderr : stdout;

#if defined(_WIN32)
   StdLogger *sdata = data;

   if (!sdata->attached) {
      g_mutex_lock(&gConsoleLock);
      if (gRefCount != 0 || GlibUtils_AttachConsole()) {
         gRefCount++;
         sdata->attached = TRUE;
      }
      g_mutex_unlock(&gConsoleLock);
   }

   if (!sdata->attached) {
      return;
   }
#endif

   local = g_locale_from_utf8(message, -1, NULL, NULL, NULL);
   if (local != NULL) {
      fputs(local, dest);
      g_free(local);
   } else {
      fputs(message, dest);
   }
}


/*
 *******************************************************************************
 * StdLoggerDestroy --                                                    */ /**
 *
 * Cleans up the internal state of the logger.
 *
 * @param[in] data   Logger data.
 *
 *******************************************************************************
 */

static void
StdLoggerDestroy(gpointer data)
{
#if defined(_WIN32)
   StdLogger *sdata = data;
   g_mutex_lock(&gConsoleLock);
   if (sdata->attached && --gRefCount == 0) {
      FreeConsole();
   }
   g_mutex_unlock(&gConsoleLock);
#endif
   g_free(data);
}


#if defined(_WIN32)
/*
 *******************************************************************************
 * GlibUtilsIsRedirected --                                               */ /**
 *
 * Checks whether given standard device (standard input, standard output,
 * or standard error) has been redirected to an on-disk file/pipe.
 * Win32-only.
 *
 * @param[in] nStdHandle          The standard device number.
 *
 * @return TRUE if device redirected to a file/pipe.
 *
 *******************************************************************************
 */

static gboolean
GlibUtilsIsRedirected(DWORD nStdHandle)
{
   HANDLE handle = GetStdHandle(nStdHandle);
   DWORD type = handle ? GetFileType(handle) : FILE_TYPE_UNKNOWN;

   return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
}


/*
 *******************************************************************************
 * GlibUtils_AttachConsole --                                             */ /**
 *
 * Attaches a console to the current process. If the parent process already has
 * a console open, reuse it. Otherwise, create a new console for the current
 * process. Win32-only.
 *
 * It's safe to call this function multiple times (it won't do anything if
 * the process already has a console).
 *
 * @note Attaching to the parent process's console is only available on XP and
 * later.
 *
 * @return Whether the process is attached to a console.
 *
 *******************************************************************************
 */

gboolean
GlibUtils_AttachConsole(void)
{
   typedef BOOL (WINAPI *AttachConsoleFn)(DWORD);
   gboolean ret = TRUE;
   AttachConsoleFn _AttachConsole;
   BOOL reopenStdout;
   BOOL reopenStderr;

   if (GetConsoleWindow() != NULL) {
      goto exit;
   }

   reopenStdout = !GlibUtilsIsRedirected(STD_OUTPUT_HANDLE);
   reopenStderr = !GlibUtilsIsRedirected(STD_ERROR_HANDLE);
   if (!reopenStdout && !reopenStderr) {
      goto exit;
   }

   _AttachConsole = (AttachConsoleFn) GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                                     "AttachConsole");
   if ((_AttachConsole != NULL && _AttachConsole(ATTACH_PARENT_PROCESS)) ||
       AllocConsole()) {
      FILE *fptr;

      if (reopenStdout) {
         fptr = _wfreopen(L"CONOUT$", L"a", stdout);
         if (fptr == NULL) {
            g_warning("_wfreopen failed for stdout/CONOUT$: %d (%s)",
                      errno, strerror(errno));
            ret = FALSE;
         }
      }

      if (reopenStderr) {
         fptr = _wfreopen(L"CONOUT$", L"a", stderr);
         if (fptr == NULL) {
            g_warning("_wfreopen failed for stderr/CONOUT$: %d (%s)",
                      errno, strerror(errno));
            ret = FALSE;
         } else {
            setvbuf(fptr, NULL, _IONBF, 0);
         }
      }
   } else {
      ret = FALSE;
   }

exit:
   if (!ret) {
      g_warning("Console redirection unavailable.");
   }
   return ret;
}
#endif


/*
 *******************************************************************************
 * GlibUtils_CreateStdLogger --                                           */ /**
 *
 * @brief Configures a new std logger.
 *
 * @return A new logger instance.
 *
 *******************************************************************************
 */

GlibLogger *
GlibUtils_CreateStdLogger(void)
{
   StdLogger *data = g_new0(StdLogger, 1);
   data->handler.logfn = StdLoggerLog;
   data->handler.addsTimestamp = FALSE;
   data->handler.shared = FALSE;
   data->handler.dtor = StdLoggerDestroy;
   return &data->handler;
}


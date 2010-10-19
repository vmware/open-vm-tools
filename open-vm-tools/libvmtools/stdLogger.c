/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#include "vmtoolsInt.h"
#include <stdio.h>

#if defined(_WIN32)
static GStaticMutex gConsoleLock = G_STATIC_MUTEX_INIT;
static gint gRefCount = 0;
#endif

typedef struct StdLoggerData {
   LogHandlerData    handler;
#if defined(_WIN32)
   gboolean          attached;
#endif
} StdLoggerData;


/*
 ******************************************************************************
 * VMStdLoggerLog --                                                    */ /**
 *
 * Logs a message to stdout or stderr depending on its severity.
 *
 * @param[in] domain    Unused.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] data      Logger data.
 * @param[in] errfn     Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static gboolean
VMStdLoggerLog(const gchar *domain,
               GLogLevelFlags level,
               const gchar *message,
               LogHandlerData *data,
               LogErrorFn errfn)
{
   FILE *dest = (level < G_LOG_LEVEL_MESSAGE) ? stderr : stdout;

#if defined(_WIN32)
   StdLoggerData *sdata = (StdLoggerData *) data;

   if (!sdata->attached) {
      g_static_mutex_lock(&gConsoleLock);
      if (gRefCount != 0 || VMTools_AttachConsole()) {
         gRefCount++;
         sdata->attached = TRUE;
      }
      g_static_mutex_unlock(&gConsoleLock);
   }

   if (!sdata->attached) {
      return FALSE;
   }
#endif

   fputs(message, dest);
   return TRUE;
}


/*
 *******************************************************************************
 * VMStdLoggerDestroy --                                                  */ /**
 *
 * Cleans up the internal state of the logger.
 *
 * @param[in] data   Logger data.
 *
 *******************************************************************************
 */

static void
VMStdLoggerDestroy(LogHandlerData *data)
{
#if defined(_WIN32)
   StdLoggerData *sdata = (StdLoggerData *) data;
   g_static_mutex_lock(&gConsoleLock);
   if (sdata->attached && --gRefCount == 0) {
      FreeConsole();
   }
   g_static_mutex_unlock(&gConsoleLock);
#endif
   g_free(data);
}


/*
 ******************************************************************************
 * VMStdLoggerConfig --                                                 */ /**
 *
 * Configures a new std logger.
 *
 * @param[in] defaultDomain   Unused.
 * @param[in] domain          Name of log domain.
 * @param[in] name            Name of log handler.
 * @param[in] cfg             Configuration data.
 *
 * @return The std logger data.
 *
 ******************************************************************************
 */

LogHandlerData *
VMStdLoggerConfig(const gchar *defaultDomain,
                  const gchar *domain,
                  const gchar *name,
                  GKeyFile *cfg)
{
   StdLoggerData *data = g_new0(StdLoggerData, 1);
   data->handler.logfn = VMStdLoggerLog;
   data->handler.convertToLocal = TRUE;
   data->handler.timestamp = TRUE;
   data->handler.shared = FALSE;
   data->handler.copyfn = NULL;
   data->handler.dtor = VMStdLoggerDestroy;
   return &data->handler;
}


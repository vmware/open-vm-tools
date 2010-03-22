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


/*
 ******************************************************************************
 * VMStdLoggerLog --                                                    */ /**
 *
 * Logs a message to stdout or stderr depending on its severity.
 *
 * @param[in] domain    Unused.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     Unused.
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
               LogHandlerData *_data,
               LogErrorFn errfn)
{
   FILE *dest = (level < G_LOG_LEVEL_MESSAGE) ? stderr : stdout;
   fputs(message, dest);
   return TRUE;
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
   LogHandlerData *data;

#if defined(_WIN32)
   if (!VMTools_AttachConsole()) {
      return NULL;
   }
#endif

   data = g_new0(LogHandlerData, 1);
   data->logfn = VMStdLoggerLog;
   data->convertToLocal = TRUE;
   data->timestamp = TRUE;
   data->shared = FALSE;
   data->copyfn = NULL;
   data->dtor = (LogHandlerDestroyFn) g_free;
   return data;
}


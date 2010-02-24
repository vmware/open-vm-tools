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
 * @file fileLogger.c
 *
 * Logger that uses file streams and provides optional log rotation.
 */

#include "vmtoolsInt.h"
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#if defined(G_PLATFORM_WIN32)
#  include <process.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include "vm_assert.h"
#include "hostinfo.h"

typedef struct FileLoggerData {
   LogHandlerData    handler;
   FILE             *file;
   gchar            *path;
   gboolean          append;
   gboolean          error;
} FileLoggerData;


/*
 ******************************************************************************
 * VMFileLoggerOpen --                                                  */ /**
 *
 * Opens a log file for writing, backing up the existing log file if one is
 * present. Only one old log file is preserved.
 *
 * @param[in] path   Path to log file.
 * @param[in] append Whether to open the log for appending (if TRUE, a backup
 *                   file is not generated).
 *
 * @return File pointer for writing to the file (NULL on error).
 *
 ******************************************************************************
 */

static FILE *
VMFileLoggerOpen(const gchar *path,
                 gboolean append)
{
   FILE *logfile = NULL;
   gchar *pathLocal;

   ASSERT(path != NULL);
   pathLocal = VMTOOLS_GET_FILENAME_LOCAL(path, NULL);

   if (!append && g_file_test(pathLocal, G_FILE_TEST_EXISTS)) {
      /* Back up existing log file. */
      gchar *bakFile = g_strdup_printf("%s.old", pathLocal);
      if (!g_file_test(bakFile, G_FILE_TEST_IS_DIR) &&
          (!g_file_test(bakFile, G_FILE_TEST_EXISTS) ||
           g_unlink(bakFile) == 0)) {
         g_rename(pathLocal, bakFile);
      } else {
         g_unlink(pathLocal);
      }
      g_free(bakFile);
   }

   logfile = g_fopen(pathLocal, append ? "a" : "w");
   VMTOOLS_RELEASE_FILENAME_LOCAL(pathLocal);
   return logfile;
}


/*
 ******************************************************************************
 * VMFileLoggerLog --                                                   */ /**
 *
 * Logs a message to the configured destination file. Also opens the file for
 * writing if it hasn't been done yet.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     LogHandlerData pointer.
 * @param[in] errfn     Error log handler.
 *
 * @return Whether the message was successfully written.
 *
 ******************************************************************************
 */

static gboolean
VMFileLoggerLog(const gchar *domain,
                GLogLevelFlags level,
                const gchar *message,
                LogHandlerData *_data,
                LogErrorFn errfn)
{
   gboolean ret = FALSE;
   FileLoggerData *data = (FileLoggerData *) _data;

   if (data->error) {
      goto exit;
   }

   if (data->file == NULL) {
      if (data->path == NULL) {
         /* We should only get in this situation if the domain's log level is "none". */
         ASSERT(data->handler.mask == 0);
         errfn(domain, level, message);
         ret = TRUE;
         goto exit;
      } else {
         data->file = VMFileLoggerOpen(data->path, data->append);
         if (data->file == NULL) {
            data->error = TRUE;
            errfn(domain, G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION,
                  "Unable to open log file %s for domain %s.\n",
                  data->path, data->handler.domain);
            goto exit;
         }
      }
   }

   if (fputs(message, data->file) >= 0) {
      fflush(data->file);
      ret = TRUE;
   }

exit:
   return ret;
}


/*
 ******************************************************************************
 * VMFileLoggerCopy --                                                  */ /**
 *
 * Duplicates the state of the old config data into the new one, if their
 * configurations match.
 *
 * @param[in] _current  New config data.
 * @param[in] _old      Config data from where to copy state.
 *
 ******************************************************************************
 */

static void
VMFileLoggerCopy(LogHandlerData *_current,
                 LogHandlerData *_old)
{
   FileLoggerData *current = (FileLoggerData *) _current;
   FileLoggerData *old = (FileLoggerData *) _old;

   ASSERT(current->file == NULL);
   if (current->path != NULL &&
       old->path != NULL &&
       old->file != NULL &&
       strcmp(current->path, old->path) == 0) {
      g_free(current->path);
      current->file = old->file;
      current->path = old->path;
      old->file = NULL;
      old->path = NULL;
   }
}


/*
 ******************************************************************************
 * VMFileLoggerDestroy --                                               */ /**
 *
 * Cleans up the internal state of a file logger.
 *
 * @param[in] _data     File logger data.
 *
 ******************************************************************************
 */

static void
VMFileLoggerDestroy(LogHandlerData *_data)
{
   FileLoggerData *data = (FileLoggerData *) _data;
   if (data->file != NULL) {
      fclose(data->file);
   }
   g_free(data->path);
   g_free(data);
}


/*
 ******************************************************************************
 * VMFileLoggerConfig --                                                */ /**
 *
 * Configures a new file logger based on the given configuration.
 *
 * @param[in] domain    Name of log domain.
 * @param[in] name      Name of log handler.
 * @param[in] cfg       Configuration data.
 *
 * @return The file logger data, or NULL on failure.
 *
 ******************************************************************************
 */

LogHandlerData *
VMFileLoggerConfig(const gchar *domain,
                   const gchar *name,
                   GKeyFile *cfg)
{
   gchar *logpath = NULL;
   FileLoggerData *data = NULL;
   gchar *level;
   gchar key[128];

   g_snprintf(key, sizeof key, "%s.level", domain);
   level = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
   if (strcmp(level, "none") != 0) {
      g_snprintf(key, sizeof key, "%s.data", domain);

      logpath = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      if (logpath == NULL) {
         g_warning("Missing log path for file handler (%s).\n", domain);
         goto exit;
      } else {
         /*
          * Do some variable expansion in the input string. Currently only
          * ${USER} and ${PID} are expanded.
          */
         gchar *vars[] = {
            "${USER}",  NULL,
            "${PID}",   NULL
         };
         size_t i;

         vars[1] = Hostinfo_GetUser();
         vars[3] = g_strdup_printf("%"FMTPID, getpid());

         for (i = 0; i < ARRAYSIZE(vars); i += 2) {
            char *last = logpath;
            char *start;
            while ((start = strstr(last, vars[i])) != NULL) {
               gchar *tmp;
               char *end = start + strlen(vars[i]);
               size_t offset = (start - last) + strlen(vars[i+1]);

               *start = '\0';
               tmp = g_strdup_printf("%s%s%s", logpath, vars[i+1], end);
               g_free(logpath);
               logpath = tmp;
               last = logpath + offset;
            }
         }

         vm_free(vars[1]);
         g_free(vars[3]);
      }
   }
   g_free(level);

   data = g_new0(FileLoggerData, 1);
   data->handler.logfn = VMFileLoggerLog;
   data->handler.convertToLocal = FALSE;
   data->handler.timestamp = TRUE;
   data->handler.shared = FALSE;
   data->handler.copyfn = VMFileLoggerCopy;
   data->handler.dtor = VMFileLoggerDestroy;

   data->path = logpath;
   data->append = (name != NULL && strcmp(name, "file+") == 0);

exit:
   return (data != NULL) ? &data->handler : NULL;
}


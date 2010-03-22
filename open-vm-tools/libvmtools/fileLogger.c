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

typedef struct FileLoggerData {
   LogHandlerData    handler;
   FILE             *file;
   gchar            *path;
   gint              logSize;
   gint              maxSize;
   guint             maxFiles;
   gboolean          append;
   gboolean          error;
   GStaticRWLock     lock;
} FileLoggerData;


/*
 ******************************************************************************
 * VMFileLoggerGetPath --                                               */ /**
 *
 * Parses the given template file name and expands embedded variables, and
 * places the log index information at the right position.
 *
 * The following variables are expanded:
 *
 *    - ${USER}:  user's login name.
 *    - ${PID}:   current process's pid.
 *    - ${IDX}:   index of the log file (for rotation).
 *
 * @param[in] data         Log handler data.
 * @param[in] index        Index of the log file.
 *
 * @return The expanded log file path.
 *
 ******************************************************************************
 */

static gchar *
VMFileLoggerGetPath(FileLoggerData *data,
                    gint index)
{
   gboolean hasIndex = FALSE;
   gchar indexStr[11];
   gchar *logpath;
   gchar *vars[] = {
      "${USER}",  NULL,
      "${PID}",   NULL,
      "${IDX}",   indexStr,
   };
   gchar *tmp;
   size_t i;

   logpath = g_strdup(data->path);
   vars[1] = (char *) g_get_user_name();
   vars[3] = g_strdup_printf("%"FMTPID, getpid());
   g_snprintf(indexStr, sizeof indexStr, "%d", index);

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

         /* XXX: ugly, but well... */
         if (i == 4) {
            hasIndex = TRUE;
         }
      }
   }

   g_free(vars[3]);

   /*
    * Always make sure we add the index if it's not 0, since that's used for
    * backing up old log files.
    */
   if (index != 0 && !hasIndex) {
      char *sep = strrchr(logpath, '.');
      char *pathsep = strrchr(logpath, '/');

      if (pathsep == NULL) {
         pathsep = strrchr(logpath, '\\');
      }

      if (sep != NULL && sep > pathsep) {
         *sep = '\0';
         sep++;
         tmp = g_strdup_printf("%s.%d.%s", logpath, index, sep);
      } else {
         tmp = g_strdup_printf("%s.%d", logpath, index);
      }
      g_free(logpath);
      logpath = tmp;
   }

   return logpath;
}


/*
 ******************************************************************************
 * VMFileLoggerOpen --                                                  */ /**
 *
 * Opens a log file for writing, backing up the existing log file if one is
 * present. Only one old log file is preserved.
 *
 * @note Make sure this function is called with the write lock held.
 *
 * @param[in] data   Log handler data.
 *
 * @return Log file pointer (NULL on error).
 *
 ******************************************************************************
 */

static FILE *
VMFileLoggerOpen(FileLoggerData *data)
{
   FILE *logfile = NULL;
   gchar *path;

   ASSERT(data != NULL);
   path = VMFileLoggerGetPath(data, 0);

   if (g_file_test(path, G_FILE_TEST_EXISTS)) {
      struct stat fstats;
      if (g_stat(path, &fstats) > -1) {
         g_atomic_int_set(&data->logSize, (gint) fstats.st_size);
      }

      if (!data->append || g_atomic_int_get(&data->logSize) >= data->maxSize) {
         /*
          * Find the last log file and iterate back, changing the indices as we go,
          * so that the oldest log file has the highest index (the new log file
          * will always be index "0"). When not rotating, "maxFiles" is 1, so we
          * always keep one backup.
          */
         gchar *log;
         guint id;
         GPtrArray *logfiles = g_ptr_array_new();

         /*
          * Find the id of the last log file. The pointer array will hold
          * the names of all existing log files + the name of the last log
          * file, which may or may not exist.
          */
         for (id = 0; id < data->maxFiles; id++) {
            log = VMFileLoggerGetPath(data, id);
            g_ptr_array_add(logfiles, log);
            if (!g_file_test(log, G_FILE_TEST_IS_REGULAR)) {
               break;
            }
         }

         /* Rename the existing log files, increasing their index by 1. */
         for (id = logfiles->len - 1; id > 0; id--) {
            gchar *dest = g_ptr_array_index(logfiles, id);
            gchar *src = g_ptr_array_index(logfiles, id - 1);

            if (!g_file_test(dest, G_FILE_TEST_IS_DIR) &&
                (!g_file_test(dest, G_FILE_TEST_EXISTS) ||
                 g_unlink(dest) == 0)) {
               g_rename(src, dest);
            } else {
               g_unlink(src);
            }
         }

         /* Cleanup. */
         for (id = 0; id < logfiles->len; id++) {
            g_free(g_ptr_array_index(logfiles, id));
         }
         g_ptr_array_free(logfiles, TRUE);
         g_atomic_int_set(&data->logSize, 0);
         data->append = FALSE;
      }
   }

   logfile = g_fopen(path, data->append ? "a" : "w");
   g_free(path);
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

   g_static_rw_lock_reader_lock(&data->lock);

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
         /*
          * We need to drop the read lock and acquire a write lock to open
          * the log file.
          */
         g_static_rw_lock_reader_unlock(&data->lock);
         g_static_rw_lock_writer_lock(&data->lock);
         if (data->file == NULL) {
            data->file = VMFileLoggerOpen(data);
         }
         g_static_rw_lock_writer_unlock(&data->lock);
         g_static_rw_lock_reader_lock(&data->lock);
         if (data->file == NULL) {
            data->error = TRUE;
            errfn(domain, G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION,
                  "Unable to open log file %s for domain %s.\n",
                  data->path, data->handler.domain);
            goto exit;
         }
      }
   }

   /* Write the log file and do log rotation accounting. */
   if (fputs(message, data->file) >= 0) {
      if (data->maxSize > 0) {
         g_atomic_int_add(&data->logSize, strlen(message));
#if defined(_WIN32)
         /* Account for \r. */
         g_atomic_int_add(&data->logSize, 1);
#endif
         if (g_atomic_int_get(&data->logSize) >= data->maxSize) {
            /* Drop the reader lock, grab the writer lock and re-check. */
            g_static_rw_lock_reader_unlock(&data->lock);
            g_static_rw_lock_writer_lock(&data->lock);
            if (g_atomic_int_get(&data->logSize) >= data->maxSize) {
               fclose(data->file);
               data->append = FALSE;
               data->file = VMFileLoggerOpen(data);
            }
            g_static_rw_lock_writer_unlock(&data->lock);
            g_static_rw_lock_reader_lock(&data->lock);
         } else {
            fflush(data->file);
         }
      } else {
         fflush(data->file);
      }
      ret = TRUE;
   }

exit:
   g_static_rw_lock_reader_unlock(&data->lock);
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
      current->logSize = old->logSize;
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
   g_static_rw_lock_free(&data->lock);
   g_free(data->path);
   g_free(data);
}


/*
 ******************************************************************************
 * VMFileLoggerConfig --                                                */ /**
 *
 * Configures a new file logger based on the given configuration.
 *
 * @param[in] defaultDomain   Unused.
 * @param[in] domain          Name of log domain.
 * @param[in] name            Name of log handler.
 * @param[in] cfg             Configuration data.
 *
 * @return The file logger data, or NULL on failure.
 *
 ******************************************************************************
 */

LogHandlerData *
VMFileLoggerConfig(const gchar *defaultDomain,
                   const gchar *domain,
                   const gchar *name,
                   GKeyFile *cfg)
{
   gchar *logpath = NULL;
   FileLoggerData *data = NULL;
   gchar *level;
   gchar key[128];
   GError *err = NULL;

   g_snprintf(key, sizeof key, "%s.level", domain);
   level = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
   if (strcmp(level, "none") != 0) {
      g_snprintf(key, sizeof key, "%s.data", domain);

      logpath = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      if (logpath == NULL) {
         g_warning("Missing log path for file handler (%s).\n", domain);
         goto exit;
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

   data->append = (name != NULL && strcmp(name, "file+") == 0);
   g_static_rw_lock_init(&data->lock);

   if (logpath != NULL) {
      data->path = g_filename_from_utf8(logpath, -1, NULL, NULL, NULL);
      ASSERT(data->path != NULL);
      g_free(logpath);

      /*
       * Read the rolling file configuration. By default, log rotation is enabled
       * with a max file size of 10MB and a maximum of 10 log files kept around.
       */
      g_snprintf(key, sizeof key, "%s.maxOldLogFiles", domain);
      data->maxFiles = (guint) g_key_file_get_integer(cfg, LOGGING_GROUP, key, &err);
      if (err != NULL) {
         g_clear_error(&err);
         data->maxFiles = 10;
      } else if (data->maxFiles < 1) {
         data->maxFiles = 1;
      }

      /* Add 1 to account for the active log file. */
      data->maxFiles += 1;

      g_snprintf(key, sizeof key, "%s.maxLogSize", domain);
      data->maxSize = (gsize) g_key_file_get_integer(cfg, LOGGING_GROUP, key, &err);
      if (err != NULL) {
         g_clear_error(&err);
         data->maxSize = 10;
      }
      data->maxSize = data->maxSize * 1024 * 1024;
   }

exit:
   return (data != NULL) ? &data->handler : NULL;
}


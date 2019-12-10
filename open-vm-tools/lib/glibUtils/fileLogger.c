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
 * @file fileLogger.c
 *
 * Logger that uses file streams and provides optional log rotation.
 */

#include "glibUtils.h"
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#if defined(G_PLATFORM_WIN32)
#  include <process.h>
#  include <windows.h>
#  include "win32Access.h"
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif


typedef struct FileLogger {
   GlibLogger     handler;
   GIOChannel    *file;
   gchar         *path;
   gint           logSize;
   guint64        maxSize;
   guint          maxFiles;
   gboolean       append;
   gboolean       error;
   GMutex         lock;
} FileLogger;


#if !defined(_WIN32)
/*
 *******************************************************************************
 * FileLoggerIsValid --                                                   */ /**
 *
 * Checks that the file descriptor backing this logger is still valid.
 *
 * This is a racy workaround for an issue with glib code; or, rather, two
 * issues. The first issue is that we can't intercept G_LOG_FLAG_RECURSION,
 * and glib just aborts when that happens (see gnome bug 618956). The second
 * is that if a GIOChannel channel write fails, that calls
 * g_io_channel_error_from_errno, which helpfully logs stuff, causing recursion.
 * Don't get me started on why that's, well, at least questionable.
 *
 * This is racy because between the check and the actual GIOChannel operation,
 * the state of the FD may have changed. In reality, since the bug this is
 * fixing happens in very special situations where code outside this file is
 * doing weird things like closing random fds, it should be OK.
 *
 * We may still get other write errors from the GIOChannel than EBADF, but
 * those would be harder to work around. Hopefully this handles the most usual
 * cases.
 *
 * See bug 783999 for some details about what triggers the bug.
 *
 * @param[in] logger The logger instance.
 *
 * @return TRUE if the I/O channel is still valid.
 *
 *******************************************************************************
 */

static gboolean
FileLoggerIsValid(FileLogger *logger)
{
   if (logger->file != NULL) {
      int fd = g_io_channel_unix_get_fd(logger->file);
      return fcntl(fd, F_GETFD) >= 0;
   }

   return FALSE;
}

#else

#define FileLoggerIsValid(logger) TRUE

#endif


/*
 * This function is a temporary workaround for a broken glib version
 * 2.42.2 that is missing g_get_user_name_utf8 export. After glib
 * is fixed this needs to be removed and call to GlibGetUserName
 * replaced by g_get_user_name. See bug 1434059 for details.
 */
const char* GlibGetUserName()
{
#if !defined(_WIN32)
   return g_get_user_name();
#else
   wchar_t buffer[256] = { 0 };
   DWORD len = ARRAYSIZE(buffer);
   static char* user_name_utf8 = NULL;
   if (!user_name_utf8) {
      if (GetUserNameW(buffer, &len)) {
         user_name_utf8 = g_utf16_to_utf8(buffer, -1, NULL, NULL, NULL);
      }
   }
   return user_name_utf8;
#endif
}


/*
 *******************************************************************************
 * FileLoggerGetPath --                                                   */ /**
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
FileLoggerGetPath(FileLogger *data,
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
   vars[1] = (char *) GlibGetUserName();
   vars[3] = g_strdup_printf("%u", (unsigned int) getpid());
   g_snprintf(indexStr, sizeof indexStr, "%d", index);

   for (i = 0; i < G_N_ELEMENTS(vars); i += 2) {
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
 *******************************************************************************
 * FileLoggerOpen --                                                      */ /**
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
 *******************************************************************************
 */

static GIOChannel *
FileLoggerOpen(FileLogger *data)
{
   GIOChannel *logfile = NULL;
   gchar *path;

   g_return_val_if_fail(data != NULL, NULL);
   path = FileLoggerGetPath(data, 0);

   if (g_file_test(path, G_FILE_TEST_EXISTS)) {
      /* GStatBuf was added in 2.26. */
#if GLIB_CHECK_VERSION(2, 26, 0)
      GStatBuf fstats;
#else
      struct stat fstats;
#endif

      if (g_stat(path, &fstats) > -1) {
         data->logSize = (gint) fstats.st_size;
      }

      if (!data->append || data->logSize >= data->maxSize) {
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
            log = FileLoggerGetPath(data, id);
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
         data->logSize = 0;
         data->append = FALSE;
      }
   }

   logfile = g_io_channel_new_file(path, data->append ? "a" : "w", NULL);

   if (logfile != NULL) {
      g_io_channel_set_encoding(logfile, NULL, NULL);
#ifdef VMX86_TOOLS
      /*
       * Make the logfile readable only by user and root/administrator.
       * Can't do anything if it fails, so ignore return.
       */
#ifdef _WIN32
      (void) Win32Access_SetFileOwnerRW(path);
#else
      (void) chmod(path, 0600);
#endif
#endif // VMX86_TOOLS
   }
   g_free(path);

   return logfile;
}


/*
 *******************************************************************************
 * FileLoggerLog --                                                       */ /**
 *
 * Logs a message to the configured destination file. Also opens the file for
 * writing if it hasn't been done yet.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] data      File logger.
 *
 *******************************************************************************
 */

static void
FileLoggerLog(const gchar *domain,
              GLogLevelFlags level,
              const gchar *message,
              gpointer data)
{
   FileLogger *logger = data;
   gsize written;

   g_mutex_lock(&logger->lock);

   if (logger->error) {
      goto exit;
   }

   if (logger->file == NULL) {
      if (logger->file == NULL) {
         logger->file = FileLoggerOpen(data);
      }
      if (logger->file == NULL) {
         logger->error = TRUE;
         goto exit;
      }
   }

   if (!FileLoggerIsValid(logger)) {
      logger->error = TRUE;
      goto exit;
   }

   /* Write the log file and do log rotation accounting. */
   if (g_io_channel_write_chars(logger->file, message, -1, &written, NULL) ==
       G_IO_STATUS_NORMAL) {
      if (logger->maxSize > 0) {
         logger->logSize += (gint) written;
         if (logger->logSize >= logger->maxSize) {
            g_io_channel_unref(logger->file);
            logger->append = FALSE;
            logger->file = FileLoggerOpen(logger);
         } else {
            g_io_channel_flush(logger->file, NULL);
         }
      } else {
         g_io_channel_flush(logger->file, NULL);
      }
   }

exit:
   g_mutex_unlock(&logger->lock);
}


/*
 ******************************************************************************
 * FileLoggerDestroy --                                               */ /**
 *
 * Cleans up the internal state of a file logger.
 *
 * @param[in] _data     File logger data.
 *
 ******************************************************************************
 */

static void
FileLoggerDestroy(gpointer data)
{
   FileLogger *logger = data;
   if (logger->file != NULL) {
      g_io_channel_unref(logger->file);
   }
   g_mutex_clear(&logger->lock);
   g_free(logger->path);
   g_free(logger);
}


/*
 *******************************************************************************
 * GlibUtils_CreateFileLogger --                                          */ /**
 *
 * @brief Creates a new file logger based on the given configuration.
 *
 * @param[in] path      Path to log file.
 * @param[in] append    Whether to append to existing log file.
 * @param[in] maxSize   Maximum log file size (in MB, 0 = no limit).
 * @param[in] maxFiles  Maximum number of old files to be kept.
 *
 * @return A new logger, or NULL on error.
 *
 *******************************************************************************
 */

GlibLogger *
GlibUtils_CreateFileLogger(const char *path,
                           gboolean append,
                           guint maxSize,
                           guint maxFiles)
{
   FileLogger *data = NULL;

   g_return_val_if_fail(path != NULL, NULL);

   data = g_new0(FileLogger, 1);
   data->handler.addsTimestamp = FALSE;
   data->handler.shared = FALSE;
   data->handler.logfn = FileLoggerLog;
   data->handler.dtor = FileLoggerDestroy;

   data->path = g_filename_from_utf8(path, -1, NULL, NULL, NULL);
   if (data->path == NULL) {
      g_free(data);
      return NULL;
   }

   data->append = append;
   data->maxSize = maxSize * 1024 * 1024;
   data->maxFiles = maxFiles + 1; /* To account for the active log file. */
   g_mutex_init(&data->lock);

   return &data->handler;
}


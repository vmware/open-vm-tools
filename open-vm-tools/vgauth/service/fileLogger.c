/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
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
 * Heavily 'borrows' from bora-lib/apps/vmtoolslib/fileLogger.c
 */

#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#  include <io.h>
#else
#  include <unistd.h>
#endif
#include "service.h"

typedef struct FileLoggerData {
   FILE             *file;
   gchar            *path;
   gint              logSize;
   gint64            maxSize;
   guint             maxFiles;
   gboolean          append;
   gboolean          error;
   GRWLock           lock;
} FileLoggerData;


/*
 ******************************************************************************
 * ServiceFileLoggerOpen --                                              */ /**
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
ServiceFileLoggerOpen(FileLoggerData *data)
{
   FILE *logfile = NULL;
   gchar *path;

   ASSERT(data != NULL);
   path = g_strdup_printf("%s.%d", data->path, 0);

   if (g_file_test(path, G_FILE_TEST_EXISTS)) {
      /* GStatBuf was added in 2.26. */
      GStatBuf fstats;

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
         gchar *fname;
         guint id;
         GPtrArray *logfiles = g_ptr_array_new();

         /*
          * Find the id of the last log file. The pointer array will hold
          * the names of all existing log files + the name of the last log
          * file, which may or may not exist.
          */
         for (id = 0; id < data->maxFiles; id++) {
            fname = g_strdup_printf("%s.%d", data->path, id);
            g_ptr_array_add(logfiles, fname);
            if (!g_file_test(fname, G_FILE_TEST_IS_REGULAR)) {
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
   /*
    * Make log readable only by root/Administrator.  Just log any error;
    * better a readable log than none at all so any issues are logged.
    */
#ifdef _WIN32
   {
      UserAccessControl uac;

      /* The default access only allows self and administrators */
      if (!UserAccessControl_Default(&uac)) {
         VGAUTH_LOG_WARNING("failed to set up logfile %s access control",
                            path);
      } else {
         BOOL ok;

         ok = WinUtil_SetFileSecurity(path,
                                UserAccessControl_GetSecurityDescriptor(&uac));
         if (!ok) {
            VGAUTH_LOG_WARNING("WinUtil_SetFileSecurity(%s) failed", path);
         }
         UserAccessControl_Destroy(&uac);
      }
   }
#else
   (void) ServiceFileSetPermissions(path, 0600);
#endif
   g_free(path);

#ifndef VMX86_DEBUG
   /*
    * Redirect anything unexpected that uses stderr.
    */
   if (NULL != logfile) {
      if (dup2(fileno(logfile), 2) == -1) {
         fprintf(logfile, "%s: failed to dup stderr to logfile\n", __FUNCTION__);
      }
   }
#endif

   return logfile;
}


/*
 ******************************************************************************
 * ServiceFileLogger_Log --                                              */ /**
 *
 * Logs a message to the configured destination file. Also opens the file for
 * writing if it hasn't been done yet.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     FileLoggerData pointer.
 *
 * @return Whether the message was successfully written.
 *
 ******************************************************************************
 */

gboolean
ServiceFileLogger_Log(const gchar *domain,
                      GLogLevelFlags level,
                      const gchar *message,
                      void *_data)
{
   gboolean ret = FALSE;
   FileLoggerData *data = (FileLoggerData *) _data;

   g_rw_lock_reader_lock(&data->lock);

   if (data->error) {
      goto exit;
   }

   if (data->file == NULL) {
      if (data->path == NULL) {
         /* We should only get in this situation if the domain's log level is "none". */
         ret = TRUE;
         goto exit;
      } else {
         /*
          * We need to drop the read lock and acquire a write lock to open
          * the log file.
          */
         g_rw_lock_reader_unlock(&data->lock);
         g_rw_lock_writer_lock(&data->lock);
         if (data->file == NULL) {
            data->file = ServiceFileLoggerOpen(data);
         }
         g_rw_lock_writer_unlock(&data->lock);
         g_rw_lock_reader_lock(&data->lock);
         if (data->file == NULL) {
            data->error = TRUE;
            fprintf(stderr, "Unable to open log file %s\n", data->path);
            goto exit;
         }
      }
   }

   /* Write the log file and do log rotation accounting. */
   if (fputs(message, data->file) >= 0) {
      if (data->maxSize > 0) {
         g_atomic_int_add(&data->logSize, (int) strlen(message));
#if defined(_WIN32)
         /* Account for \r. */
         g_atomic_int_add(&data->logSize, 1);
#endif
         if (g_atomic_int_get(&data->logSize) >= data->maxSize) {
            /* Drop the reader lock, grab the writer lock and re-check. */
            g_rw_lock_reader_unlock(&data->lock);
            g_rw_lock_writer_lock(&data->lock);
            if (g_atomic_int_get(&data->logSize) >= data->maxSize) {
               fclose(data->file);
               data->append = FALSE;
               data->file = ServiceFileLoggerOpen(data);
            }
            g_rw_lock_writer_unlock(&data->lock);
            g_rw_lock_reader_lock(&data->lock);
         } else {
            fflush(data->file);
         }
      } else {
         fflush(data->file);
      }
      ret = TRUE;
   }

exit:
   g_rw_lock_reader_unlock(&data->lock);
   return ret;
}


/*
 ******************************************************************************
 * ServiceFileLogger_Init --                                             */ /**
 *
 * Initializes the file logger.
 *
 * @return The file logger data, or NULL on failure.
 *
 ******************************************************************************
 */

void *
ServiceFileLogger_Init(void)
{
   gchar *logFileName;
   FileLoggerData *data;
   gchar *defaultFilename;

#ifdef _WIN32
   {
      WCHAR pathW[MAX_PATH];

      if (GetTempPathW(MAX_PATH, pathW) != 0) {
         char *pathA = Convert_Utf16ToUtf8(__FUNCTION__,
                                           __FILE__, __LINE__,
                                           pathW);
         if (NULL == pathA) {
            Warning("%s: out of memory converting filePath\n", __FUNCTION__);
            return NULL;
         }
         defaultFilename = g_strdup_printf("%s%s", pathA, LOGFILENAME_DEFAULT);
         g_free(pathA);
      } else {
         defaultFilename = g_strdup(LOGFILENAME_PATH_DEFAULT);
      }
   }
#else
   defaultFilename = g_strdup(LOGFILENAME_PATH_DEFAULT);
#endif

   logFileName = Pref_GetString(gPrefs,
                                VGAUTH_PREF_NAME_LOGFILE,
                                VGAUTH_PREF_GROUP_NAME_SERVICE,
                                defaultFilename);

   Debug("%s: Using '%s' as logfile\n", __FUNCTION__, logFileName);

   g_free(defaultFilename);
   data = g_malloc0(sizeof(FileLoggerData));

   /*
    * XXX
    *
    * Not sure we want this -- it means we'll append to any existing
    * file, which preserves some data, but it may also cause confusion
    * when the service start isn't at the top of the file.
    */
   data->append = TRUE;

   g_rw_lock_init(&data->lock);

   if (logFileName != NULL) {
      data->path = g_filename_from_utf8(logFileName, -1, NULL, NULL, NULL);
      ASSERT(data->path != NULL);
      g_free(logFileName);

      /*
       * Read the rolling file configuration. By default, log rotation is enabled
       * with a max file size of 10MB and a maximum of 10 log files kept around.
       */
      data->maxFiles = Pref_GetInt(gPrefs,
                                   VGAUTH_PREF_NAME_MAX_OLD_LOGFILES,
                                   VGAUTH_PREF_GROUP_NAME_SERVICE, 10);
      if (data->maxFiles < 1) {
         data->maxFiles = 1;
      }

      /* Add 1 to account for the active log file. */
      data->maxFiles += 1;

      data->maxSize = Pref_GetInt(gPrefs,
                                  VGAUTH_PREF_NAME_MAX_LOGSIZE,
                                  VGAUTH_PREF_GROUP_NAME_SERVICE, 10);
      data->maxSize = data->maxSize * 1024 * 1024;
   }

   return data;
}


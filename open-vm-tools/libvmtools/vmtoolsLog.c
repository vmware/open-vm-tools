/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file vmtoolsLog.c
 *
 *    Defines a logging infrastructure for the vmtools library based
 *    on glib's logging facilities. Wrap the commonly used logging functions
 *    (Log/Warning/Debug), and provides configurability for where logs should
 *    go to.
 *
 *    To choose the logging domain for your source file, define G_LOG_DOMAIN
 *    before including glib.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#if defined(G_PLATFORM_WIN32)
#  include <process.h>
#else
#  include <unistd.h>
#endif

#include "vmtools.h"
#include "codeset.h"
#if defined(G_PLATFORM_WIN32)
#  include "coreDump.h"
#endif
#include "file.h"
#include "hostinfo.h"
#include "system.h"

#if defined(G_PLATFORM_WIN32)
#  define  DEFAULT_HANDLER    VMToolsLogOutputDebugString
#else
#  define  DEFAULT_HANDLER    VMToolsLogFile
#endif

#define LOGGING_GROUP         "logging"
#define MAX_DOMAIN_LEN        64

/** Tells whether the given log level is a fatal error. */
#define IS_FATAL(level) ((level) & G_LOG_FLAG_FATAL)

/**
 * Tells whether a message should be logged. All fatal messages are logged,
 * regardless of what the configuration says. Otherwise, the log domain's
 * configuration is respected.
 */
#define SHOULD_LOG(level, data) (IS_FATAL(level) || \
                                 (gLogEnabled && ((data)->mask & (level))))


static void
VMToolsLogFile(const gchar *domain,
               GLogLevelFlags level,
               const gchar *message,
               gpointer _data);

#if defined(G_PLATFORM_WIN32)
static void
VMToolsLogOutputDebugString(const gchar *domain,
                            GLogLevelFlags level,
                            const gchar *message,
                            gpointer _data);
#endif

typedef struct LogHandlerData {
   gchar            *domain;
   GLogLevelFlags    mask;
   FILE             *file;
   gchar            *path;
   gboolean          append;
   guint             handlerId;
   gboolean          inherited;
   gboolean          error;
} LogHandlerData;

static gchar *gLogDomain = NULL;
static gboolean gEnableCoreDump = TRUE;
static gboolean gLogEnabled = FALSE;
static guint gPanicCount = 0;
static LogHandlerData *gDefaultData = NULL;
static GLogFunc gDefaultLogFunc = DEFAULT_HANDLER;
static GPtrArray *gDomains = NULL;

/* Internal functions. */


/**
 * glib-based version of Str_Asprintf().
 *
 * @param[out] string   Where to store the result.
 * @param[in]  format   String format.
 * @param[in]  ...      String arguments.
 *
 * @return Number of bytes printed.
 */

static INLINE gint
VMToolsAsprintf(gchar **string,
                gchar const *format,
                ...)
{
   gint cnt;
   va_list args;
   va_start(args, format);
   cnt = g_vasprintf(string, format, args);
   va_end(args);
   return cnt;
}


/**
 * Opens a log file for writing, backing up the existing log file if one is
 * present. Only one old log file is preserved.
 *
 * @param[in] path   Path to log file.
 * @param[in] append Whether to open the log for appending (if TRUE, a backup
 *                   file is not generated).
 *
 * @return File pointer for writing to the file (NULL on error).
 */

static FILE *
VMToolsLogOpenFile(const gchar *path,
                   gboolean append)
{
   FILE *logfile = NULL;
   gchar *pathLocal;

   ASSERT(path != NULL);
   pathLocal = VMTOOLS_GET_FILENAME_LOCAL(path, NULL);

   if (!append && g_file_test(path, G_FILE_TEST_EXISTS)) {
      /* Back up existing log file. */
      gchar *bakFile = g_strdup_printf("%s.old", pathLocal);
      if (!g_file_test(bakFile, G_FILE_TEST_IS_DIR) &&
          (!g_file_test(bakFile, G_FILE_TEST_EXISTS) ||
           g_unlink(bakFile) == 0)) {
         g_rename(pathLocal, bakFile);
      }
      g_free(bakFile);
   }

   logfile = g_fopen(pathLocal, append ? "a" : "w");
   VMTOOLS_RELEASE_FILENAME_LOCAL(pathLocal);
   return logfile;
}


/**
 * Creates a formatted message to be logged. The format of the message will be:
 *
 *    [timestamp] [domain] [level] Log message
 *
 * @param[in] message      User log message.
 * @param[in] domain       Log domain.
 * @param[in] level        Log level.
 * @param[in] timestamp    Whether to print the timestamp.
 * @param[in] printAppName Whether to include the app name (a.k.a. the default
 *                         domain) in the message.
 *
 * @return Formatted log message in the current encoding. Should be free()'d.
 */

static char *
VMToolsLogFormat(const gchar *message,
                 const gchar *domain,
                 GLogLevelFlags level,
                 gboolean timestamp,
                 gboolean printAppName)
{
   char *msg = NULL;
   char *msgCurr = NULL;
   const char *slevel;
   size_t len = 0;

   if (domain == NULL) {
      domain = gLogDomain;
   }

   /*
    * glib 2.16 on Windows has a bug where its printf implementations don't
    * like NULL.
    */
   if (message == NULL) {
      message = "<null>";
   }

   switch (level & G_LOG_LEVEL_MASK) {
   case G_LOG_LEVEL_ERROR:
      slevel = "error";
      break;

   case G_LOG_LEVEL_CRITICAL:
      slevel = "critical";
      break;

   case G_LOG_LEVEL_WARNING:
      slevel = "warning";
      break;

   case G_LOG_LEVEL_MESSAGE:
      slevel = "message";
      break;

   case G_LOG_LEVEL_INFO:
      slevel = "info";
      break;

   case G_LOG_LEVEL_DEBUG:
      slevel = "debug";
      break;

   default:
      slevel = "unknown";
   }

   if (timestamp) {
      char *tstamp;

      tstamp = System_GetTimeAsString();
      if (printAppName) {
         len = VMToolsAsprintf(&msg, "[%s] [%8s] [%s:%s] %s\n",
                               (tstamp != NULL) ? tstamp : "no time",
                               slevel, gLogDomain, domain, message);
      } else {
         len = VMToolsAsprintf(&msg, "[%s] [%8s] [%s] %s\n",
                               (tstamp != NULL) ? tstamp : "no time",
                               slevel, domain, message);
      }
      free(tstamp);
   } else {
      if (printAppName) {
         len = VMToolsAsprintf(&msg, "[%8s] [%s:%s] %s\n",
                               slevel, gLogDomain, domain, message);
      } else {
         len = VMToolsAsprintf(&msg, "[%8s] [%s] %s\n", slevel, domain, message);
      }
   }

   if (msg != NULL) {
      size_t msgCurrLen;
      msgCurr = g_locale_from_utf8(msg, strlen(msg), NULL, &msgCurrLen, NULL);

      /*
       * The log messages from glib itself (and probably other libraries based
       * on glib) do not include a trailing new line. Most of our code does. So
       * we detect whether the original message already had a new line, and
       * remove it, to avoid having two newlines when printing our log messages.
       */
      if (msgCurr != NULL && msgCurr[msgCurrLen - 2] == '\n') {
         msgCurr[msgCurrLen - 1] = '\0';
      }
   }

   if (msgCurr != NULL) {
      g_free(msg);
      return msgCurr;
   }
   return msg;
}


/**
 * Aborts the program, optionally creating a core dump.
 */

static INLINE NORETURN void
VMToolsLogPanic(void)
{
   if (gEnableCoreDump) {
#if defined(G_PLATFORM_WIN32)
      CoreDump_CoreDump();
#else
      abort();
#endif
   }
   /* Same behavior as Panic_Panic(). */
   exit(-1);
}


#if defined(G_PLATFORM_WIN32)
/**
 * Logs a message to OutputDebugString.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     LogHandlerData pointer.
 */

static void
VMToolsLogOutputDebugString(const gchar *domain,
                            GLogLevelFlags level,
                            const gchar *message,
                            gpointer _data)
{
   LogHandlerData *data = _data;
   if (SHOULD_LOG(level, data)) {
      char *msg = VMToolsLogFormat(message, domain, level, FALSE, TRUE);
      if (msg != NULL) {
         OutputDebugStringA(msg);
         g_free(msg);
      }
   }
   if (IS_FATAL(level)) {
      VMToolsLogPanic();
   }
}
#endif


/**
 * Logs a message to a file streams. When writing to the standard streams,
 * any level >= MESSAGE will cause the message to go to stdout; otherwise,
 * it will go to stderr.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     LogHandlerData pointer.
 */

static void
VMToolsLogFile(const gchar *domain,
               GLogLevelFlags level,
               const gchar *message,
               gpointer _data)
{
   LogHandlerData *data = _data;
   if (SHOULD_LOG(level, data)) {
      data = data->inherited ? gDefaultData : data;
      if (!data->error && data->file == NULL && data->path != NULL) {
         data->file = VMToolsLogOpenFile(data->path, data->append);
         if (data->file == NULL) {
            /*
             * glib's documentation says that we can set up log handlers that
             * handle G_LOG_FLAG_RECURSION, but looking at the source code of
             * the g_logv() function that's not really true (at least up to
             * current top of tree - glib 2.20?). So we have to avoid recursion
             * here and bypass the log system.
             */
            gchar warn[1024];
            g_snprintf(warn, sizeof warn,
                       "Unable to open log file %s for domain %s.\n",
                       data->path, data->domain);

            data->error = TRUE;
            DEFAULT_HANDLER(domain, G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION,
                            warn, gDefaultData);
         }
      }
      if (!(level & G_LOG_FLAG_RECURSION) && data->error) {
         DEFAULT_HANDLER(domain, level | G_LOG_FLAG_RECURSION, message, gDefaultData);
      } else {
         gchar *msg = VMToolsLogFormat(message, domain, level, TRUE, FALSE);
         if (msg != NULL) {
            FILE *dest = (data->file != NULL) ? data->file
                           : ((level < G_LOG_LEVEL_MESSAGE) ? stderr : stdout);
            fputs(msg, dest);
            fflush(dest);
            g_free(msg);
         }
      }
   }
   if (IS_FATAL(level)) {
      VMToolsLogPanic();
   }
}


/**
 * Configures the given log domain based on the data provided in the given
 * dictionary. If the log domain being configured doesn't match the default
 * (@see VMTools_GetDefaultLogDomain()), and no specific handler is defined
 * for the domain, the handler is inherited from the default domain, instead
 * of using the default handler. This allows reusing the same log file, for
 * example, while maintaining the ability to enable different log levels
 * for different domains.
 *
 * For the above to properly work, the default log domain has to be configured
 * before any other domains.
 *
 * @param[in]  domain      Name of domain being configured.
 * @param[in]  cfg         Dictionary with config data.
 */

static void
VMToolsConfigLogDomain(const gchar *domain,
                       GKeyFile *cfg)
{
   gchar *handler = NULL;
   gchar *level = NULL;
   gchar *logpath = NULL;
   gchar key[128];

   GLogFunc handlerFn = NULL;
   GLogLevelFlags levelsMask;
   LogHandlerData *data;

   /* Arbitrary limit. */
   if (strlen(domain) > MAX_DOMAIN_LEN) {
      g_warning("Domain name too long: %s\n", domain);
      goto exit;
   } else if (strlen(domain) == 0) {
      g_warning("Invalid domain declaration, missing name.\n");
      goto exit;
   }

   g_snprintf(key, sizeof key, "%s.level", domain);
   level = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
   if (level == NULL) {
#ifdef VMX86_DEBUG
      level = g_strdup("message");
#else
      level = g_strdup("warning");
#endif
   }

   /* Parse the handler information. */
   g_snprintf(key, sizeof key, "%s.handler", domain);
   handler = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);

   if (handler == NULL) {
      if (strcmp(domain, VMTools_GetDefaultLogDomain()) == 0) {
         handlerFn = DEFAULT_HANDLER;
      } else {
         handlerFn = gDefaultLogFunc;
      }
   } else if (strcmp(handler, "std") == 0) {
      handlerFn = VMToolsLogFile;
   } else if (strcmp(handler, "file") == 0 ||
              strcmp(handler, "file+") == 0) {
      /* Don't set up the file sink if logging is disabled. */
      if (strcmp(level, "none") != 0) {
         handlerFn = VMToolsLogFile;
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

            free(vars[1]);
            g_free(vars[3]);
         }
      }
#if defined(G_PLATFORM_WIN32)
   } else if (strcmp(handler, "outputdebugstring") == 0) {
      handlerFn = VMToolsLogOutputDebugString;
#endif
   } else {
      g_warning("Unknown log handler: %s\n", handler);
      goto exit;
   }

   /* Parse the log level configuration, and build the mask. */
   if (strcmp(level, "error") == 0) {
      levelsMask = G_LOG_LEVEL_ERROR;
   } else if (strcmp(level, "critical") == 0) {
      levelsMask = G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL;
   } else if (strcmp(level, "warning") == 0) {
      levelsMask = G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING;
   } else if (strcmp(level, "message") == 0) {
      levelsMask = G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING |
                   G_LOG_LEVEL_MESSAGE;
   } else if (strcmp(level, "info") == 0) {
      levelsMask = G_LOG_LEVEL_ERROR |
                   G_LOG_LEVEL_CRITICAL |
                   G_LOG_LEVEL_WARNING |
                   G_LOG_LEVEL_MESSAGE |
                   G_LOG_LEVEL_INFO;
   } else if (strcmp(level, "debug") == 0) {
      levelsMask = G_LOG_LEVEL_MASK;
   } else if (strcmp(level, "none") == 0) {
      levelsMask = 0;
   } else {
      g_warning("Unknown log level (%s): %s\n", domain, level);
      goto exit;
   }

   data = g_malloc0(sizeof *data);
   data->domain = g_strdup(domain);
   data->mask = levelsMask;
   data->path = logpath;
   data->append = (handler != NULL && strcmp(handler, "file+") == 0);
   logpath = NULL;

   if (strcmp(domain, VMTools_GetDefaultLogDomain()) == 0) {
      /*
       * Replace the global log configuration. If the default log domain was
       * logging to a file and the file path hasn't changed, then keep the old
       * file handle open, instead of rotating the log.
       */
      LogHandlerData *old = gDefaultData;

      if (old->file != NULL) {
         ASSERT(old->path);
         if (data->path != NULL && strcmp(data->path, old->path) == 0) {
            g_free(data->path);
            data->file = old->file;
            data->path = old->path;
            old->path = NULL;
         } else {
            fclose(old->file);
            g_free(old->path);
         }
      }

      g_log_set_default_handler(handlerFn, data);
      gDefaultData = data;
      data = NULL;
      gDefaultLogFunc = handlerFn;
      g_free(old);
   } else if (handler == NULL) {
      ASSERT(data->file == NULL);
      data->inherited = TRUE;
   }

   if (data != NULL) {
      if (gDomains == NULL) {
         gDomains = g_ptr_array_new();
      }
      g_ptr_array_add(gDomains, data);
      data->handlerId = g_log_set_handler(domain, 
                                          G_LOG_LEVEL_MASK |
                                          G_LOG_FLAG_FATAL |
                                          G_LOG_FLAG_RECURSION,
                                          handlerFn, data);
   }

exit:
   g_free(handler);
   g_free(logpath);
   g_free(level);
}


/* Public API. */

/**
 * Returns the default log domain for the application.
 *
 * @return  A string with the name of the log domain.
 */

const char *
VMTools_GetDefaultLogDomain(void)
{
   return gLogDomain;
}


/**
 * Sets the default log domain. This only changes the output of the default
 * log handler.
 *
 * @param[in]  domain   The log domain.
 */

void
VMTools_SetDefaultLogDomain(const gchar *domain)
{
   ASSERT(domain != NULL);
   if (gLogDomain != NULL) {
      g_free(gLogDomain);
   }
   gLogDomain = g_strdup(domain);
}


/**
 * Configures the logging system according to the configuration provided from
 * the given dictionary.
 *
 * @param[in] cfg    The configuration data.
 */

void
VMTools_ConfigLogging(GKeyFile *cfg)
{
   gchar **list;
   gchar **curr;

   VMTools_ResetLogging(FALSE);

   if (!g_key_file_has_group(cfg, LOGGING_GROUP)) {
      return;
   }

   /*
    * Configure the default domain first. See function documentation for
    * VMToolsConfigLogDomain() for the reason.
    */
   VMToolsConfigLogDomain(VMTools_GetDefaultLogDomain(), cfg);

   list = g_key_file_get_keys(cfg, LOGGING_GROUP, NULL, NULL);

   for (curr = list; curr != NULL && *curr != NULL; curr++) {
      gchar *domain = *curr;

      /* Check whether it's a domain "declaration". */
      if (!g_str_has_suffix(domain, ".level")) {
         continue;
      }

      /* Trims ".level" from the key to get the domain name. */
      domain[strlen(domain) - 6] = '\0';

      /* Skip the default domain. */
      if (strcmp(domain, VMTools_GetDefaultLogDomain()) == 0) {
         continue;
      }

      VMToolsConfigLogDomain(domain, cfg);
   }

   g_strfreev(list);

   gLogEnabled = g_key_file_get_boolean(cfg, LOGGING_GROUP, "log", NULL);
   if (g_key_file_has_key(cfg, LOGGING_GROUP, "enableCoreDump", NULL)) {
      gEnableCoreDump = g_key_file_get_boolean(cfg, LOGGING_GROUP,
                                               "enableCoreDump", NULL);
   }
}


/**
 * Enables of disables all the log domains configured by the vmtools library.
 * This doesn't affect other log domains that may have configured by other
 * code.
 *
 * @param[in] enable    Whether logging should be enabled.
 */

void
VMTools_EnableLogging(gboolean enable)
{
   gLogEnabled = enable;
}


/**
 * Resets the vmtools logging subsystem, freeing up data and optionally
 * restoring the original glib configuration.
 *
 * @param[in]  cleanDefault   Whether to clean up the default handler and
 *                            restore the original glib handler.
 */

void
VMTools_ResetLogging(gboolean cleanDefault)
{
   gboolean oldLogEnabled = gLogEnabled;
   gchar *currentPath = NULL;
   FILE *currentFile = NULL;

   /* Disable logging while we're playing with the configuration. */
   gLogEnabled = FALSE;

   if (cleanDefault) {
      g_log_set_default_handler(g_log_default_handler, NULL);
   }

   if (gDomains != NULL) {
      guint i;
      for (i = 0; i < gDomains->len; i++) {
         LogHandlerData *data = g_ptr_array_index(gDomains, i);
         g_log_remove_handler(data->domain, data->handlerId);
         if (data->file != NULL) {
            fclose(data->file);
         }
         g_free(data->path);
         g_free(data->domain);
         g_free(data);
      }
      g_ptr_array_free(gDomains, TRUE);
      gDomains = NULL;
   }

   if (gDefaultData != NULL) {
      currentFile = gDefaultData->file;
      currentPath = gDefaultData->path;
      g_free(gDefaultData);
      gDefaultData = NULL;
   }

   if (cleanDefault && gLogDomain != NULL) {
      g_free(gLogDomain);
      gLogDomain = NULL;
   }

   gDefaultLogFunc = DEFAULT_HANDLER;

   if (!cleanDefault) {
      if (gLogDomain == NULL) {
         gLogDomain = g_strdup("vmtools");
      }
      gDefaultData = g_malloc0(sizeof *gDefaultData);
      gDefaultData->mask = G_LOG_LEVEL_ERROR |
                           G_LOG_LEVEL_CRITICAL |
                           G_LOG_LEVEL_WARNING;
#if defined(VMX86_DEBUG)
      gDefaultData->mask |= G_LOG_LEVEL_MESSAGE;
#endif
      gDefaultData->file = currentFile;
      gDefaultData->path = currentPath;
      gLogEnabled = oldLogEnabled;
      g_log_set_default_handler(gDefaultLogFunc, gDefaultData);
   } else {
      if (currentFile != NULL) {
         fclose(currentFile);
      }
      g_free(currentPath);
   }
}


/* Wrappers for VMware's logging functions. */

/**
 * Logs a message using the G_LOG_LEVEL_DEBUG level.
 *
 * @param[in] fmt Log message format.
 */

void
Debug(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(gLogDomain, G_LOG_LEVEL_DEBUG, fmt, args);
   va_end(args);
}


/**
 * Logs a message using the G_LOG_LEVEL_MESSAGE level.
 *
 * @param[in] fmt Log message format.
 */

void
Log(const char *fmt, ...)
{
   /* CoreDump_CoreDump() calls Log(), avoid that message. */
   if (gPanicCount == 0) {
      va_list args;
      va_start(args, fmt);
      g_logv(gLogDomain, G_LOG_LEVEL_MESSAGE, fmt, args);
      va_end(args);
   }
}


/**
 * Logs a message using the G_LOG_LEVEL_ERROR level. In the default
 * configuration, this will cause the application to terminate and,
 * if enabled, to dump core.
 *
 * @param[in] fmt Log message format.
 */

void
Panic(const char *fmt, ...)
{
   va_list args;

   gPanicCount++;
   va_start(args, fmt);
   if (gPanicCount == 1) {
      g_logv(gLogDomain, G_LOG_LEVEL_ERROR, fmt, args);
   } else {
      char *msg;
      g_vasprintf(&msg, fmt, args);
      if (gPanicCount == 2) {
         fprintf(stderr, "Recursive panic: %s\n", msg);
      } else {
         fprintf(stderr, "Recursive panic, giving up: %s\n", msg);
         exit(-1);
      }
      g_free(msg);
   }
   va_end(args);
   /*
    * In case an user-installed custom handler doesn't panic on error, force a
    * core dump. Also force a dump in the recursive case.
    */
   VMToolsLogPanic();
}


/**
 * Logs a message using the G_LOG_LEVEL_WARNING level.
 *
 * @param[in] fmt Log message format.
 */

void
Warning(const char *fmt, ...)
{
   /* CoreDump_CoreDump() may call Warning(), avoid that message. */
   if (gPanicCount == 0) {
      va_list args;
      va_start(args, fmt);
      g_logv(gLogDomain, G_LOG_LEVEL_WARNING, fmt, args);
      va_end(args);
   }
}


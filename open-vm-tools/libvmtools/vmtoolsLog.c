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

#include "vmtoolsInt.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#if defined(G_PLATFORM_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/resource.h>
#  include <sys/time.h>
#endif

#if defined(G_PLATFORM_WIN32)
#  include "coreDump.h"
#endif
#include "system.h"

#define MAX_DOMAIN_LEN        64

/** Alias to retrieve the default handler from the handler array. */
#define DEFAULT_HANDLER (&gHandlers[ARRAYSIZE(gHandlers) - 1])

/** Tells whether the given log level is a fatal error. */
#define IS_FATAL(level) ((level) & G_LOG_FLAG_FATAL)

/**
 * Tells whether a message should be logged. All fatal messages are logged,
 * regardless of what the configuration says. Otherwise, the log domain's
 * configuration is respected.
 */
#define SHOULD_LOG(level, data) (IS_FATAL(level) || \
                                 (gLogEnabled && ((data)->mask & (level))))

/** Clean up the contents of a log handler. */
#define CLEAR_LOG_HANDLER(handler) do { \
   if ((handler) != NULL) {             \
      g_free((handler)->domain);        \
      (handler)->dtor(handler);         \
   }                                    \
} while (0)


#if defined(G_PLATFORM_WIN32)
static void
VMToolsLogOutputDebugString(const gchar *domain,
                            GLogLevelFlags level,
                            const gchar *message,
                            gpointer _data);
#endif

typedef LogHandlerData * (*LogHandlerConfigFn)(const gchar *defaultDomain,
                                               const gchar *domain,
                                               const gchar *name,
                                               GKeyFile *cfg);

typedef struct LogHandler {
   const guint          id;
   const gchar         *name;
   LogHandlerConfigFn   configfn;
} LogHandler;


/**
 * List of available log handlers, mapped to their config file entries.
 * The NULL entry means the default handler (if the config file entry
 * doesn't exist, or doesn't match any existing handler), and must be
 * the last entry.
 */
static LogHandler gHandlers[] = {
   { 0,  "std",               VMStdLoggerConfig },
   { 1,  "file",              VMFileLoggerConfig },
   { 2,  "file+",             VMFileLoggerConfig },
   { 3,  "vmx",               VMXLoggerConfig },
#if defined(_WIN32)
   { 4,  "outputdebugstring", VMDebugOutputConfig },
   { -1, NULL,                VMDebugOutputConfig },
#else
   { 4,  "syslog",            VMSysLoggerConfig },
   { -1, NULL,                VMStdLoggerConfig },
#endif
};


static gchar *gLogDomain = NULL;
static gboolean gEnableCoreDump = TRUE;
static gboolean gLogEnabled = FALSE;
static guint gPanicCount = 0;
static LogHandlerData *gDefaultData = NULL;
static LogHandlerData *gErrorData = NULL;
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

gint
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
 * Creates a formatted message to be logged. The format of the message will be:
 *
 *    [timestamp] [domain] [level] Log message
 *
 * @param[in] message      User log message.
 * @param[in] domain       Log domain.
 * @param[in] level        Log level.
 * @param[in] data         Log handler data.
 *
 * @return Formatted log message according to the log domain's config.
 *         Should be g_free()'d.
 */

static gchar *
VMToolsLogFormat(const gchar *message,
                 const gchar *domain,
                 GLogLevelFlags level,
                 LogHandlerData *data)
{
   char *msg = NULL;
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

   if (data->timestamp) {
      char *tstamp;

      tstamp = System_GetTimeAsString();
      if (data->shared) {
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
      if (data->shared) {
         len = VMToolsAsprintf(&msg, "[%8s] [%s:%s] %s\n",
                               slevel, gLogDomain, domain, message);
      } else {
         len = VMToolsAsprintf(&msg, "[%8s] [%s] %s\n", slevel, domain, message);
      }
   }

   if (msg != NULL && data->convertToLocal) {
      gchar *msgCurr = g_locale_from_utf8(msg, len, NULL, &len, NULL);
      g_free(msg);
      msg = msgCurr;
   }

   /*
    * The log messages from glib itself (and probably other libraries based
    * on glib) do not include a trailing new line. Most of our code does. So
    * we detect whether the original message already had a new line, and
    * remove it, to avoid having two newlines when printing our log messages.
    */
   if (msg != NULL && msg[len - 2] == '\n') {
      msg[len - 1] = '\0';
   }

   return msg;
}


/**
 * Logs a message to the error log domain. This is used when a log handler
 * encounters an error while logging a message, and wants to log information
 * about that error.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] fmt       Message format.
 * @param[in] ...       Message parameters.
 */

static void
VMToolsError(const gchar *domain,
             GLogLevelFlags level,
             const gchar *fmt,
             ...)
{
   gchar *message;
   gchar *formatted;
   va_list args;

   va_start(args, fmt);
   g_vasprintf(&message, fmt, args);
   va_end(args);

   formatted = VMToolsLogFormat(message, domain, level, gErrorData);

   gErrorData->logfn(domain, level, formatted, gErrorData, VMToolsError);
   g_free(formatted);
   g_free(message);
}


/**
 * Aborts the program, optionally creating a core dump.
 */

static INLINE NORETURN void
VMToolsLogPanic(void)
{
   gPanicCount++;
   if (gEnableCoreDump) {
#if defined(_WIN32)
      CoreDump_CoreDump();
#else
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof cwd) != NULL) {
         if (access(cwd, W_OK) == -1) {
            /*
             * Can't write to the working dir. chdir() to the user's home
             * directory as an attempt to get a valid core dump.
             */
            const char *home = getenv("HOME");
            if (home != NULL) {
               if (chdir(home)) {
                  /* Just to make glibc headers happy. */
               }
            }
         }
      }
      abort();
#endif
   }
   /* Same behavior as Panic_Panic(). */
   exit(-1);
}


/**
 * Log handler function that does the common processing of log messages,
 * and delegates the actual printing of the message to the given handler.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] _data     LogHandlerData pointer.
 */

static void
VMToolsLog(const gchar *domain,
           GLogLevelFlags level,
           const gchar *message,
           gpointer _data)
{
   LogHandlerData *data = _data;
   if (SHOULD_LOG(level, data)) {
      gchar *msg = VMToolsLogFormat(message, domain, level, data);
      data = data->inherited ? gDefaultData : data;
      if (!data->logfn(domain, level, msg, data, VMToolsError)) {
         /*
          * The logger for some reason indicated that it couldn't log the
          * message. Use the error handler to do it, and ignore any
          * errors.
          */
         VMToolsError(domain, level | G_LOG_FLAG_RECURSION, message,
                      gErrorData, VMToolsError);
      }
      g_free(msg);
   }
   if (IS_FATAL(level)) {
      VMToolsLogPanic();
   }
}


/**
 * Configures the given log domain based on the data provided in the given
 * dictionary. If the log domain being configured doesn't match the default, and
 * no specific handler is defined for the domain, the handler is inherited from
 * the default domain, instead of using the default handler. This allows reusing
 * the same log file, for example, while maintaining the ability to enable
 * different log levels for different domains.
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
   gchar key[128];
   guint hid;
   size_t i;

   GLogLevelFlags levelsMask;
   LogHandlerConfigFn configfn = NULL;
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

   if (handler != NULL) {
      for (i = 0; i < ARRAYSIZE(gHandlers) - 1; i++) {
         if (handler == gHandlers[i].name ||
             strcmp(handler, gHandlers[i].name) == 0) {
            hid = gHandlers[i].id;
            configfn = gHandlers[i].configfn;
            break;
         }
      }

      if (configfn == NULL) {
         g_warning("Unknown log handler '%s', using default.", handler);
         goto exit;
      }

      data = configfn(gLogDomain, domain, handler, cfg);
   } else if (strcmp(domain, gLogDomain) == 0) {
      /*
       * If no handler defined and we're configuring the default domain,
       * then instantiate the default handler.
       */
      hid = DEFAULT_HANDLER->id;
      configfn = DEFAULT_HANDLER->configfn;
      data = configfn(gLogDomain, domain, NULL, cfg);
      ASSERT(data != NULL);
   } else {
      /* An inherited handler. Just create a dummy instance. */
      ASSERT(gDefaultData != NULL);
      data = g_new0(LogHandlerData, 1);
      data->inherited = TRUE;
      data->logfn = gDefaultData->logfn;
      data->convertToLocal = gDefaultData->convertToLocal;
      data->timestamp = gDefaultData->timestamp;
      data->shared = gDefaultData->shared;
      data->dtor = (LogHandlerDestroyFn) g_free;
      hid = -1;
   }

   ASSERT(data->logfn != NULL);
   ASSERT(data->dtor != NULL);

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

   data->domain = g_strdup(domain);
   data->mask = levelsMask;
   data->type = hid;

   if (strcmp(domain, gLogDomain) == 0) {
      /*
       * Replace the global log configuration. If the default log domain was
       * logging to a file and the file path hasn't changed, then keep the old
       * file handle open, instead of rotating the log.
       */
      LogHandlerData *old = gDefaultData;

      if (old != NULL && old->type == data->type && old->copyfn != NULL) {
         data->copyfn(data, old);
      }

      g_log_set_default_handler(VMToolsLog, data);
      gDefaultData = data;
      CLEAR_LOG_HANDLER(old);
      data = NULL;
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
                                          VMToolsLog,
                                          data);
   }

exit:
   g_free(handler);
   g_free(level);
}


/**
 * Resets the vmtools logging subsystem, freeing up data and restoring the
 * original glib configuration.
 *
 * @param[in]  hard     Whether to do a "hard" reset of the logging system
 *                      (cleaning up any log domain existing state and freeing
 *                      associated memory).
 */

static void
VMToolsResetLogging(gboolean hard)
{
   gLogEnabled = FALSE;
   g_log_set_default_handler(g_log_default_handler, NULL);

   CLEAR_LOG_HANDLER(gErrorData);
   gErrorData = NULL;

   if (gDomains != NULL) {
      guint i;
      for (i = 0; i < gDomains->len; i++) {
         LogHandlerData *data = g_ptr_array_index(gDomains, i);
         g_log_remove_handler(data->domain, data->handlerId);
         if (hard) {
            CLEAR_LOG_HANDLER(data);
         }
      }
      if (hard) {
         g_ptr_array_free(gDomains, TRUE);
         gDomains = NULL;
      }
   }

   if (hard && gDefaultData != NULL) {
      CLEAR_LOG_HANDLER(gDefaultData);
      gDefaultData = NULL;
   }

   if (gLogDomain != NULL) {
      g_free(gLogDomain);
      gLogDomain = NULL;
   }
}


/**
 * Restores the logging configuration in the given config data. This means doing
 * the following:
 *
 * . if the old log domain exists in the current configuration, and in case both
 *   the old and new configuration used log files, then re-use the file that was
 *   already opened.
 * . if they don't use the same configuration, close the log file for the old
 *   configuration.
 * . if an old log domain doesn't exist in the new configuration, then
 *   release any resources the old configuration was using for that domain.
 *
 * @param[in] oldDefault    Data for the old default domain.
 * @param[in] oldDomains    List of old log domains.
 */

static void
VMToolsRestoreLogging(LogHandlerData *oldDefault,
                      GPtrArray *oldDomains)
{
   /* First, restore what needs to be restored. */
   if (gDomains != NULL && oldDomains != NULL) {
      guint i;
      for (i = 0; i < gDomains->len; i++) {
         guint j;
         LogHandlerData *data = g_ptr_array_index(gDomains, i);

         /* Try to find the matching old config. */
         for (j = 0; j < oldDomains->len; j++) {
            LogHandlerData *olddata = g_ptr_array_index(oldDomains, j);
            if (data->type == olddata->type &&
                strcmp(data->domain, olddata->domain) == 0) {
               if (!data->inherited && data->copyfn != NULL) {
                  data->copyfn(data, olddata);
               }
               break;
            }
         }
      }
   }

   if (gDefaultData != NULL &&
       oldDefault != NULL &&
       gDefaultData->copyfn != NULL &&
       gDefaultData->type == oldDefault->type) {
      gDefaultData->copyfn(gDefaultData, oldDefault);
   }

   /* Second, clean up the old configuration data. */
   if (oldDomains != NULL) {
      while (oldDomains->len > 0) {
         LogHandlerData *data = g_ptr_array_remove_index_fast(oldDomains,
                                                              oldDomains->len - 1);
         CLEAR_LOG_HANDLER(data);
      }
   }

   if (oldDefault != NULL) {
      CLEAR_LOG_HANDLER(oldDefault);
   }
}


/* Public API. */


#if defined(_WIN32)
/**
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
 */

gboolean
VMTools_AttachConsole(void)
{
   typedef BOOL (WINAPI *AttachConsoleFn)(DWORD);
   gboolean ret = FALSE;
   AttachConsoleFn _AttachConsole;

   if (GetConsoleWindow() != NULL) {
      return TRUE;
   }

   _AttachConsole = (AttachConsoleFn) GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                                     "AttachConsole");
   if ((_AttachConsole != NULL && _AttachConsole(ATTACH_PARENT_PROCESS)) ||
       AllocConsole()) {
      FILE* fptr;

      fptr = _wfreopen(L"CONOUT$", L"a", stdout);
      if (fptr == NULL) {
         g_warning("_wfreopen failed for stdout/CONOUT$: %d (%s)",
                   errno, strerror(errno));
         goto exit;
      }

      fptr = _wfreopen(L"CONOUT$", L"a", stderr);
      if (fptr == NULL) {
         g_warning("_wfreopen failed for stderr/CONOUT$: %d (%s)",
                   errno, strerror(errno));
         goto exit;
      }
      setvbuf(fptr, NULL, _IONBF, 0);
      ret = TRUE;
   }

exit:
   if (!ret) {
      g_warning("Console redirection unavailable.");
   }
   return ret;
}
#endif


/**
 * Configures the logging system according to the configuration in the given
 * dictionary.
 *
 * Optionally, it's possible to reset the logging subsystem; this will shut
 * down all log handlers managed by the vmtools library before configuring
 * the log system, which means that logging will behave as if the application
 * was just started. A visible side-effect of this is that log files may be
 * rotated (if they're not configure for appending).
 *
 * @param[in] defaultDomain   Name of the default log domain.
 * @param[in] cfg             The configuration data. May be NULL.
 * @param[in] force           Whether to force logging to be enabled.
 * @param[in] reset           Whether to reset the logging subsystem first.
 */

void
VMTools_ConfigLogging(const gchar *defaultDomain,
                      GKeyFile *cfg,
                      gboolean force,
                      gboolean reset)
{
   gboolean allocDict = (cfg == NULL);
   gchar **list;
   gchar **curr;
   GPtrArray *oldDomains = NULL;
   LogHandlerData *oldDefault = NULL;

   g_return_if_fail(defaultDomain != NULL);

   if (allocDict) {
      cfg = g_key_file_new();
   }

   /*
    * If not resetting the logging system, keep the old domains around. After
    * we're done loading the new configuration, we'll go through the old domains
    * and restore any data that needs restoring, and clean up anything else.
    */
   VMToolsResetLogging(reset);
   if (!reset) {
      oldDefault = gDefaultData;
      oldDomains = gDomains;
      gDomains = NULL;
      gDefaultData = NULL;
   }

   gLogDomain = g_strdup(defaultDomain);
   gErrorData = DEFAULT_HANDLER->configfn(gLogDomain, gLogDomain, NULL, NULL);

   /*
    * Configure the default domain first. See function documentation for
    * VMToolsConfigLogDomain() for the reason.
    */
   VMToolsConfigLogDomain(gLogDomain, cfg);

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
      if (strcmp(domain, gLogDomain) == 0) {
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

   /* If needed, restore the old configuration. */
   if (!reset) {
      VMToolsRestoreLogging(oldDefault, oldDomains);
      if (oldDomains != NULL) {
         g_ptr_array_free(oldDomains, TRUE);
      }
   }

   /*
    * If core dumps are enabled (default: TRUE), then set up the exception
    * filter on Win32. On POSIX systems, try to modify the resource limit
    * to allow core dumps, but don't complain if it fails. Core dumps may
    * still fail, e.g., if the current directory is not writable by the
    * user running the process.
    *
    * On POSIX systems, if the process is itself requesting a core dump (e.g.,
    * by calling Panic() or g_error()), the core dump routine will try to find
    * a location where it can successfully create the core file. Applications
    * can try to set up core dump filters (e.g., a signal handler for SIGSEGV)
    * that can then call any of the core dumping functions handled by this
    * library.
    *
    * On POSIX systems, the maximum size of a core dump can be controlled by
    * the "maxCoreSize" config option, where "0" means "no limit". By default,
    * it's set to 5MB.
    */
   if (gEnableCoreDump) {
#if defined(_WIN32)
      CoreDump_SetUnhandledExceptionFilter();
#else
      GError *err = NULL;
      struct rlimit limit = { 0, 0 };

      getrlimit(RLIMIT_CORE, &limit);
      if (limit.rlim_max != 0) {
         limit.rlim_cur = (rlim_t) g_key_file_get_integer(cfg,
                                                          LOGGING_GROUP,
                                                          "maxCoreSize",
                                                          &err);
         if (err != NULL) {
            limit.rlim_cur = 5 * 1024 * 1024;
            g_clear_error(&err);
         } else if (limit.rlim_cur == 0) {
            limit.rlim_cur = RLIM_INFINITY;
         }

         limit.rlim_cur = MAX(limit.rlim_cur, limit.rlim_max);
         if (setrlimit(RLIMIT_CORE, &limit) == -1) {
            g_message("Failed to set core dump size limit, error %d (%s)\n",
                      errno, g_strerror(errno));
         } else {
            g_message("Core dump limit set to %d", (int) limit.rlim_cur);
         }
      }
#endif
   }

   gLogEnabled |= force;

   if (allocDict) {
      g_key_file_free(cfg);
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

   va_start(args, fmt);
   if (gPanicCount == 0) {
      g_logv(gLogDomain, G_LOG_LEVEL_ERROR, fmt, args);
      /*
       * In case an user-installed custom handler doesn't panic on error, force a
       * core dump. Also force a dump in the recursive case.
       */
      VMToolsLogPanic();
   } else if (gPanicCount == 1) {
      /*
       * Use a stack allocated string since we're in a recursive panic, so
       * probably already in a weird state.
       */
      gchar msg[1024];
      g_vsnprintf(msg, sizeof msg, fmt, args);
      fprintf(stderr, "Recursive panic: %s\n", msg);
      VMToolsLogPanic();
   } else {
      fprintf(stderr, "Recursive panic, giving up.\n");
      exit(-1);
   }
   va_end(args);
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


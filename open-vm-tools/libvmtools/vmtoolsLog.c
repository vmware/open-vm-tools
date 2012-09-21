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

#include "glibUtils.h"
#include "log.h"
#if defined(G_PLATFORM_WIN32)
#  include "coreDump.h"
#  include "w32Messages.h"
#endif
#include "str.h"
#include "system.h"

#define LOGGING_GROUP         "logging"

#define MAX_DOMAIN_LEN        64

/** The default handler to use if none is specified by the config data. */
#define DEFAULT_HANDLER "syslog"

/** The "failsafe" handler. */
#if defined(_WIN32)
#  define SAFE_HANDLER  "outputdebugstring"
#else
#  define SAFE_HANDLER  "std"
#endif

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
#define CLEAR_LOG_HANDLER(handler) do {            \
   if ((handler) != NULL) {                        \
      if (handler->logger != NULL) {               \
         handler->logger->dtor(handler->logger);   \
      }                                            \
      g_free((handler)->domain);                   \
      g_free((handler)->type);                     \
      g_free(handler);                             \
   }                                               \
} while (0)


#if defined(G_PLATFORM_WIN32)
static void
VMToolsLogOutputDebugString(const gchar *domain,
                            GLogLevelFlags level,
                            const gchar *message,
                            gpointer _data);
#endif

typedef struct LogHandler {
   GlibLogger    *logger;
   gchar         *domain;
   gchar         *type;
   guint          mask;
   guint          handlerId;
   gboolean       inherited;
} LogHandler;


static gchar *gLogDomain = NULL;
static gboolean gEnableCoreDump = TRUE;
static gboolean gLogEnabled = FALSE;
static guint gPanicCount = 0;
static LogHandler *gDefaultData;
static LogHandler *gErrorData;
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
                 LogHandler *data)
{
   char *msg = NULL;
   const char *slevel;
   size_t len = 0;
   gboolean shared = TRUE;
   gboolean addsTimestamp = TRUE;

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

   if (data->logger != NULL) {
      shared = data->logger->shared;
      addsTimestamp = data->logger->addsTimestamp;
   }

   if (!addsTimestamp) {
      char *tstamp;

      tstamp = System_GetTimeAsString();
      if (shared) {
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
      if (shared) {
         len = VMToolsAsprintf(&msg, "[%8s] [%s:%s] %s\n",
                               slevel, gLogDomain, domain, message);
      } else {
         len = VMToolsAsprintf(&msg, "[%8s] [%s] %s\n", slevel, domain, message);
      }
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
 * @param[in] _data     LogHandler pointer.
 */

static void
VMToolsLog(const gchar *domain,
           GLogLevelFlags level,
           const gchar *message,
           gpointer _data)
{
   LogHandler *data = _data;

   if (SHOULD_LOG(level, data)) {
      gchar *msg;

      data = data->inherited ? gDefaultData : data;
      msg = VMToolsLogFormat(message, domain, level, data);

      if (data->logger != NULL) {
         data->logger->logfn(domain, level, msg, data->logger);
      } else if (gErrorData->logger != NULL) {
         gErrorData->logger->logfn(domain, level, msg, gErrorData->logger);
      }
      g_free(msg);
   }
   if (IS_FATAL(level)) {
      VMToolsLogPanic();
   }
}


/*
 *******************************************************************************
 * VMToolsGetLogHandler --                                                */ /**
 *
 * @brief Instantiates the log handler for the given domain.
 *
 * @param[in] handler   Handler name.
 * @param[in] domain    Domain name.
 * @param[in] mask      The log level mask.
 * @param[in] cfg       Config dictionary.
 *
 * @return A log handler.
 *
 *******************************************************************************
 */

static LogHandler *
VMToolsGetLogHandler(const gchar *handler,
                     const gchar *domain,
                     guint mask,
                     GKeyFile *cfg)
{
   LogHandler *logger;
   GlibLogger *glogger = NULL;
   gchar key[MAX_DOMAIN_LEN + 64];

   if (strcmp(handler, "file") == 0 || strcmp(handler, "file+") == 0) {
      gboolean append = strcmp(handler, "file+") == 0;
      guint maxSize;
      guint maxFiles;
      gchar *path;
      GError *err = NULL;

      /* Use the same type name for both. */
      handler = "file";

      g_snprintf(key, sizeof key, "%s.data", domain);
      path = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      if (path != NULL) {
         g_snprintf(key, sizeof key, "%s.maxLogSize", domain);
         maxSize = (guint) g_key_file_get_integer(cfg, LOGGING_GROUP, key, &err);
         if (err != NULL) {
            g_clear_error(&err);
            maxSize = 5; /* 5 megabytes default max size. */
         }

         g_snprintf(key, sizeof key, "%s.maxOldLogFiles", domain);
         maxFiles = (guint) g_key_file_get_integer(cfg, LOGGING_GROUP, key, &err);
         if (err != NULL) {
            g_clear_error(&err);
            maxFiles = 10;
         }

         glogger = GlibUtils_CreateFileLogger(path, append, maxSize, maxFiles);
         g_free(path);
      } else {
         g_warning("Missing path for domain '%s'.", domain);
      }
   } else if (strcmp(handler, "std") == 0) {
      glogger = GlibUtils_CreateStdLogger();
   } else if (strcmp(handler, "vmx") == 0) {
      glogger = VMToolsCreateVMXLogger();
#if defined(_WIN32)
   } else if (strcmp(handler, "outputdebugstring") == 0) {
      glogger = GlibUtils_CreateDebugLogger();
   } else if (strcmp(handler, "syslog") == 0) {
      glogger = GlibUtils_CreateEventLogger(L"VMware Tools", VMTOOLS_EVENT_LOG_MESSAGE);
#else /* !_WIN32 */
   } else if (strcmp(handler, "syslog") == 0) {
      gchar *facility;

      /* Always get the facility from the default domain, since syslog is shared. */
      g_snprintf(key, sizeof key, "%s.facility", gLogDomain);
      facility = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      glogger = GlibUtils_CreateSysLogger(domain, facility);
      g_free(facility);
#endif
   } else {
      g_warning("Invalid handler for domain '%s': %s", domain, handler);
   }

   if (NULL == glogger) {
      g_warning("Failed to create a logger for handler: '%s'", handler);
   }

   logger = g_new0(LogHandler, 1);
   logger->domain = g_strdup(domain);
   logger->logger = glogger;
   logger->mask = mask;
   logger->type = strdup(handler);

   return logger;
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
 * @param[in]  oldDefault  Old default log handler.
 * @param[in]  oldDomains  List of old log domains.
 */

static void
VMToolsConfigLogDomain(const gchar *domain,
                       GKeyFile *cfg,
                       LogHandler *oldDefault,
                       GPtrArray *oldDomains)
{
   gchar *handler = NULL;
   gchar *level = NULL;
   gchar key[128];
   gboolean isDefault = strcmp(domain, gLogDomain) == 0;

   GLogLevelFlags levelsMask;
   LogHandler *data = NULL;

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

   if (handler == NULL && isDefault) {
      /*
       * If no handler defined and we're configuring the default domain,
       * then instantiate the default handler.
       */
      handler = g_strdup(DEFAULT_HANDLER);
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

   if (handler != NULL) {
      /*
       * Check whether there's an old domain with the same type configured
       * for the same domain. If there is, use it instead. Otherwise,
       * instantiate a new logger. For the check, consider both file logger
       * types as the same.
       */
      const char *oldtype = oldDefault != NULL ? oldDefault->type : NULL;

      if (oldtype != NULL && strcmp(oldtype, "file+") == 0) {
         oldtype = "file";
      }

      if (isDefault && oldtype != NULL && strcmp(oldtype, handler) == 0) {
         data = oldDefault;
      } else if (oldDomains != NULL) {
         guint i;
         for (i = 0; i < oldDomains->len; i++) {
            LogHandler *old = g_ptr_array_index(oldDomains, i);
            if (old != NULL && !old->inherited && strcmp(old->domain, domain) == 0) {
               if (strcmp(old->type, handler) == 0) {
                  data = old;
                  oldDomains->pdata[i] = NULL;
               }
               break;
            }
         }
      }

      if (data == NULL) {
         data = VMToolsGetLogHandler(handler, domain, levelsMask, cfg);
      } else {
         data->mask = levelsMask;
      }
   } else {
      /* An inherited handler. Just create a dummy instance. */
      ASSERT(gDefaultData != NULL);
      data = g_new0(LogHandler, 1);
      data->domain = g_strdup(domain);
      data->inherited = TRUE;
      data->mask = levelsMask;
   }

   if (isDefault) {
      gDefaultData = data;
      g_log_set_default_handler(VMToolsLog, gDefaultData);
   } else {
      if (gDomains == NULL) {
         gDomains = g_ptr_array_new();
      }
      g_ptr_array_add(gDomains, data);
      if (data->handlerId == 0) {
         data->handlerId = g_log_set_handler(domain,
                                             G_LOG_LEVEL_MASK |
                                             G_LOG_FLAG_FATAL |
                                             G_LOG_FLAG_RECURSION,
                                             VMToolsLog,
                                             data);
      }
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
         LogHandler *data = g_ptr_array_index(gDomains, i);
         g_log_remove_handler(data->domain, data->handlerId);
         data->handlerId = 0;
         if (hard) {
            CLEAR_LOG_HANDLER(data);
         }
      }
      if (hard) {
         g_ptr_array_free(gDomains, TRUE);
         gDomains = NULL;
      }
   }

   if (hard) {
      CLEAR_LOG_HANDLER(gDefaultData);
      gDefaultData = NULL;
   }

   if (gLogDomain != NULL) {
      g_free(gLogDomain);
      gLogDomain = NULL;
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
   return GlibUtils_AttachConsole();
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
   LogHandler *oldDefault = NULL;

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
   gErrorData = VMToolsGetLogHandler(SAFE_HANDLER,
                                     gLogDomain,
                                     G_LOG_LEVEL_MASK,
                                     cfg);

   /*
    * Configure the default domain first. See function documentation for
    * VMToolsConfigLogDomain() for the reason.
    */
   VMToolsConfigLogDomain(gLogDomain, cfg, oldDefault, oldDomains);

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

      VMToolsConfigLogDomain(domain, cfg, oldDefault, oldDomains);
   }

   g_strfreev(list);

   gLogEnabled = g_key_file_get_boolean(cfg, LOGGING_GROUP, "log", NULL);
   if (g_key_file_has_key(cfg, LOGGING_GROUP, "enableCoreDump", NULL)) {
      gEnableCoreDump = g_key_file_get_boolean(cfg, LOGGING_GROUP,
                                               "enableCoreDump", NULL);
   }

   /* If needed, restore the old configuration. */
   if (!reset) {
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
 * Generic wrapper for VMware log functions.
 *
 * CoreDump_CoreDump() may log, and glib doesn't like recursive log calls. So
 * if recursing, bypass glib and log to the default domain's log handler.
 *
 * @param[in]  level    Log level.
 * @param[in]  fmt      Message format.
 * @param[in]  args     Message arguments.
 */

static void
VMToolsLogWrapper(GLogLevelFlags level,
                  const char *fmt,
                  va_list args)
{
   if (gPanicCount == 0) {
      char *msg = Str_Vasprintf(NULL, fmt, args);
      if (msg != NULL) {
         g_log(gLogDomain, level, "%s", msg);
         free(msg);
      }
   } else {
      /* Try to avoid malloc() since we're aborting. */
      gchar msg[256];
      Str_Vsnprintf(msg, sizeof msg, fmt, args);
      VMToolsLog(gLogDomain, level, msg, gDefaultData);
   }
}


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
   VMToolsLogWrapper(G_LOG_LEVEL_DEBUG, fmt, args);
   va_end(args);
}


/**
 * Logs a message using the G_LOG_LEVEL_INFO level.
 *
 * @param[in] fmt Log message format.
 */

void
Log(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   VMToolsLogWrapper(G_LOG_LEVEL_INFO, fmt, args);
   va_end(args);
}


/**
 * Logs a message with the given log level.
 *
 * Translates lib/log levels into glib levels, and sends the message to the log
 * implementation.
 *
 * @param[in]  level    Log level.
 * @param[in]  fmt      Log message format.
 * @param[in]  args     Log message arguments.
 */

void
LogV(uint32 routing,
     const char *fmt,
     va_list args)
{
   int glevel;

   switch (routing) {
   case VMW_LOG_PANIC:
      glevel = G_LOG_LEVEL_ERROR;
      break;

   case VMW_LOG_ERROR:
      glevel = G_LOG_LEVEL_CRITICAL;
      break;

   case VMW_LOG_WARNING:
      glevel = G_LOG_LEVEL_WARNING;
      break;

   case VMW_LOG_INFO:
      glevel = G_LOG_LEVEL_MESSAGE;
      break;

   case VMW_LOG_VERBOSE:
      glevel = G_LOG_LEVEL_INFO;
      break;

   default:
      glevel = G_LOG_LEVEL_DEBUG;
   }

   VMToolsLogWrapper(glevel, fmt, args);
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
      char *msg = Str_Vasprintf(NULL, fmt, args);
      if (msg != NULL) {
         g_log(gLogDomain, G_LOG_LEVEL_ERROR, "%s", msg);
         free(msg);
      }
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
      Str_Vsnprintf(msg, sizeof msg, fmt, args);
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
   va_list args;
   va_start(args, fmt);
   VMToolsLogWrapper(G_LOG_LEVEL_WARNING, fmt, args);
   va_end(args);
}


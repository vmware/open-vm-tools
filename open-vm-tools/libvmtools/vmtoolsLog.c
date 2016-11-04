/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 *
 *    All fatal error messages will go to the 'syslog' handler no
 *    matter what handler has been configured.
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
#  include <dbghelp.h>
#  include "coreDump.h"
#  include "w32Messages.h"
#  include "win32u.h"
#endif
#include "str.h"
#include "system.h"
#include "vmware/tools/log.h"

#define LOGGING_GROUP         "logging"

#define MAX_DOMAIN_LEN                 (64)

/*
 * Default max number of log messages to be cached when log IO
 * has been frozen. In case of cache overflow, only the most
 * recent messages are preserved.
 */
#define DEFAULT_MAX_CACHE_ENTRIES      (4*1024)

/** The default handler to use if none is specified by the config data. */
#define DEFAULT_HANDLER "file+"

/** The default logfile location. */
#ifdef WIN32
// Windows log goes to %windir%\temp\vmware-<service>.log
#define DEFAULT_LOGFILE_DIR "%windir%"
#define DEFAULT_LOGFILE_NAME_PREFIX "vmware"
#else
// *ix log goes to /var/log/vmware-<service>.log
#define DEFAULT_LOGFILE_NAME_PREFIX  "/var/log/vmware"
#endif

/** The "failsafe" handler. */
#if defined(_WIN32)
#  define SAFE_HANDLER  "outputdebugstring"
#else
#  define SAFE_HANDLER  "std"
#endif

#define STD_HANDLER  "std"

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
      g_free((handler)->confData);                 \
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
   /**
    * The log handlers that write to files need special
    * treatment when guest has been quiesced.
    */
   gboolean       needsFileIO;
   gboolean       isSysLog;
   gchar         *confData;
} LogHandler;


/**
 * Structure for caching a log message
 */
typedef struct LogEntry {
   gchar           *domain;
   gchar           *msg;
   LogHandler      *handler;
   GLogLevelFlags   level;
} LogEntry;


static gchar *gLogDomain = NULL;
static GPtrArray *gCachedLogs = NULL;
static guint gDroppedLogCount = 0;
static gint gMaxCacheEntries = DEFAULT_MAX_CACHE_ENTRIES;
static gboolean gEnableCoreDump = TRUE;
static gboolean gLogEnabled = FALSE;
static gboolean gGuestSDKMode = FALSE;
static guint gPanicCount = 0;
static LogHandler *gDefaultData;
static LogHandler *gErrorData;
static LogHandler *gErrorSyslog;
static GPtrArray *gDomains = NULL;
static gboolean gLogInitialized = FALSE;
static GStaticRecMutex gLogStateMutex = G_STATIC_REC_MUTEX_INIT;
static gboolean gLoggingStopped = FALSE;
static gboolean gLogIOSuspended = FALSE;

/* Internal functions. */


/**
 * Aborts the program, optionally creating a core dump.
 */

static INLINE NORETURN void
VMToolsLogPanic(void)
{
   gPanicCount++;

   /*
    * Probably, flush the cached logs here. It is not
    * critial though because we will have the cached
    * logs in memory anyway.
    */

   if (gEnableCoreDump) {
      /*
       * TODO: Make sure to thaw the filesystem before dumping core.
       */
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
 * @param[in] cached       If the message will be cached.
 *
 * @return Formatted log message according to the log domain's config.
 *         Should be g_free()'d.
 */

static gchar *
VMToolsLogFormat(const gchar *message,
                 const gchar *domain,
                 GLogLevelFlags level,
                 LogHandler *data,
                 gboolean cached)
{
   char *msg = NULL;
   const char *slevel;
   size_t len = 0;
   gboolean shared = TRUE;
   gboolean addsTimestamp = TRUE;
   char *tstamp;

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

   tstamp = System_GetTimeAsString();

   if (!addsTimestamp) {
      if (shared) {
         len = VMToolsAsprintf(&msg, "[%s] [%8s] [%s:%s] %s\n",
                               (tstamp != NULL) ? tstamp : "no time",
                               slevel, gLogDomain, domain, message);
      } else {
         len = VMToolsAsprintf(&msg, "[%s] [%8s] [%s] %s\n",
                               (tstamp != NULL) ? tstamp : "no time",
                               slevel, domain, message);
      }
   } else {
      if (cached) {
         if (shared) {
            len = VMToolsAsprintf(&msg, "[cached at %s] [%8s] [%s:%s] %s\n",
                                  (tstamp != NULL) ? tstamp : "no time",
                                  slevel, gLogDomain, domain, message);
         } else {
            len = VMToolsAsprintf(&msg, "[cached at %s] [%8s] [%s] %s\n",
                                  (tstamp != NULL) ? tstamp : "no time",
                                  slevel, domain, message);
         }
      } else {
         if (shared) {
            len = VMToolsAsprintf(&msg, "[%8s] [%s:%s] %s\n",
                                  slevel, gLogDomain, domain, message);
         } else {
            len = VMToolsAsprintf(&msg, "[%8s] [%s] %s\n", slevel, domain, message);
         }
      }
   }

   free(tstamp);

   /*
    * The log messages from glib itself (and probably other libraries based
    * on glib) do not include a trailing new line. Most of our code does. So
    * we detect whether the original message already had a new line, and
    * remove it, to avoid having two newlines when printing our log messages.
    */
   if (msg != NULL && msg[len - 2] == '\n') {
      msg[len - 1] = '\0';
   }

   if (!msg) {
      /*
       * Memory allocation failure?
       */
      VMToolsLogPanic();
   }

   return msg;
}


/**
 * Function to free a cached LogEntry.
 *
 * @param[in] data    Log entry to be freed.
 */

static void
VMToolsFreeLogEntry(gpointer data)
{
   LogEntry *entry = data;

   g_free(entry->domain);
   g_free(entry->msg);
   g_free(entry);
}


/**
 * Function that calls the log handler.
 *
 * Also, frees the _data to avoid having separate free call.
 *
 * @param[in] _data     LogEntry pointer.
 * @param[in] userData  User data pointer.
 */

static void
VMToolsLogMsg(gpointer _data, gpointer userData)
{
   LogEntry *entry = _data;
   GlibLogger *logger = entry->handler->logger;
   gboolean usedSyslog = FALSE;

   if (logger != NULL) {
       logger->logfn(entry->domain, entry->level, entry->msg, logger);
       usedSyslog = entry->handler->isSysLog;
   } else if (gErrorData->logger != NULL) {
      gErrorData->logger->logfn(entry->domain, entry->level, entry->msg,
                                gErrorData->logger);
      usedSyslog = gErrorData->isSysLog;
   }

   /*
    * Any fatal errors need to go to syslog no matter what.
    */
   if (!usedSyslog && IS_FATAL(entry->level) && gErrorSyslog) {
      gErrorSyslog->logger->logfn(entry->domain, entry->level, entry->msg,
                                  gErrorSyslog->logger);
   }

   VMToolsFreeLogEntry(entry);
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
      LogEntry *entry;

      data = data->inherited ? gDefaultData : data;

      entry = g_malloc0(sizeof(LogEntry));
      if (entry) {
         entry->domain = domain ? g_strdup(domain) : NULL;
         if (domain && !entry->domain) {
            VMToolsLogPanic();
         }
         entry->handler = data;
         entry->level = level;
      }

      if (gLogIOSuspended && data->needsFileIO) {
         if (gMaxCacheEntries == 0) {
            /* No way to log at this point, drop it */
            VMToolsFreeLogEntry(entry);
            gDroppedLogCount++;
            goto exit;
         }

         entry->msg = VMToolsLogFormat(message, domain, level, data, TRUE);

         /*
          * Cache the log message
          */
         if (!gCachedLogs) {

            /*
             * If gMaxCacheEntries > 1K, start with 1/4th size
             * to avoid frequent allocations
             */
            gCachedLogs = g_ptr_array_sized_new(gMaxCacheEntries < 1024 ?
                                                gMaxCacheEntries :
                                                gMaxCacheEntries/4);
            if (!gCachedLogs) {
               VMToolsLogPanic();
            }

            /*
             * Some builds use glib version 2.16.4 which does not
             * support g_ptr_array_set_free_func function
             */
         }

         /*
          * We don't expect logging to be suspended for a long time,
          * so we can avoid putting a cap on cache size. However, we
          * still have a default cap of 4K messages, just to be safe.
          */
         if (gCachedLogs->len < gMaxCacheEntries) {
            g_ptr_array_add(gCachedLogs, entry);
         } else {
            /*
             * Cache is full, drop the oldest log message. This is not
             * very efficient but we don't expect this to be a common
             * case anyway.
             */
            LogEntry *oldest = g_ptr_array_remove_index(gCachedLogs, 0);
            VMToolsFreeLogEntry(oldest);
            gDroppedLogCount++;

            g_ptr_array_add(gCachedLogs, entry);
         }

      } else {
         entry->msg = VMToolsLogFormat(message, domain, level, data, FALSE);
         VMToolsLogMsg(entry, NULL);
      }
   }

exit:
   if (IS_FATAL(level)) {
      VMToolsLogPanic();
   }
}


/*
 *******************************************************************************
 * VMToolsGetLogFilePath --                                               */ /**
 *
 * @brief Fetches sanitized path for the log file.
 *
 * @param[in] key       The key for log file path.
 * @param[in] domain    Domain name.
 * @param[in] cfg       Config dictionary.
 *
 * @return Sanitized path for the log file.
 *
 *******************************************************************************
 */

static gchar *
VMToolsGetLogFilePath(const gchar *key,
                      const gchar *domain,
                      GKeyFile *cfg)
{
   gsize len = 0;
   gchar *path = NULL;
   gchar *origPath = NULL;

   path = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
   if (path == NULL) {
#ifdef WIN32
      gchar winDir[MAX_PATH];

      Win32U_ExpandEnvironmentStrings(DEFAULT_LOGFILE_DIR,
                                      (LPSTR) winDir, sizeof winDir);
      path = g_strdup_printf("%s%sTemp%s%s-%s.log",
                             winDir, DIRSEPS, DIRSEPS,
                             DEFAULT_LOGFILE_NAME_PREFIX, domain);
#else
      path = g_strdup_printf("%s-%s.log", DEFAULT_LOGFILE_NAME_PREFIX, domain);
#endif
      return path;
   }

   len = strlen(path);
   origPath = path;

   /*
    * Drop all the preceding '"' chars
    */
   while (*path == '"') {
      path++;
      len--;
   }

   /*
    * Ensure that path contains something more
    * meaningful than just '"' chars
    */
   if (len == 0) {
      g_warning("Invalid path for domain '%s'.", domain);
      g_free(origPath);
      path = NULL;
   } else {
      /* Drop the trailing '"' chars */
      gchar *sanePath = g_strdup(path);
      g_free(origPath);
      path = sanePath;

      if (path != NULL) {
         while (*(path + len - 1) == '"') {
            *(path + len - 1) = '\0';
            len--;
         }

         if (len == 0) {
            g_warning("Invalid path for domain '%s'.", domain);
            g_free(path);
            path = NULL;
         }
      }
   }

   return path;
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
   gboolean needsFileIO = FALSE;
   gchar key[MAX_DOMAIN_LEN + 64];
   gboolean isSysLog = FALSE;
   gchar *path = NULL;

   if (strcmp(handler, "file") == 0 || strcmp(handler, "file+") == 0) {
      gboolean append = strcmp(handler, "file+") == 0;
      guint maxSize;
      guint maxFiles;
      GError *err = NULL;

      /* Use the same type name for both. */
      handler = "file";

      g_snprintf(key, sizeof key, "%s.data", domain);
      path = VMToolsGetLogFilePath(key, domain, cfg);
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
         needsFileIO = TRUE;
      } else {
         g_warning("Missing path for domain '%s'.", domain);
      }
   } else if (strcmp(handler, "std") == 0) {
      glogger = GlibUtils_CreateStdLogger();
      needsFileIO = FALSE;
   } else if (strcmp(handler, "vmx") == 0) {
      glogger = VMToolsCreateVMXLogger();
      needsFileIO = FALSE;
#if defined(_WIN32)
   } else if (strcmp(handler, "outputdebugstring") == 0) {
      glogger = GlibUtils_CreateDebugLogger();
      needsFileIO = FALSE;
   } else if (strcmp(handler, "syslog") == 0) {
      glogger = GlibUtils_CreateEventLogger(L"VMware Tools", VMTOOLS_EVENT_LOG_MESSAGE);
      needsFileIO = FALSE;
      isSysLog = TRUE;
#else /* !_WIN32 */
   } else if (strcmp(handler, "syslog") == 0) {
      gchar *facility;

      /* Always get the facility from the default domain, since syslog is shared. */
      g_snprintf(key, sizeof key, "%s.facility", gLogDomain);
      facility = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      glogger = GlibUtils_CreateSysLogger(domain, facility);
      /*
       * Older versions of Linux make synchronous call to syslog.
       */
      needsFileIO = TRUE;
      g_free(facility);
      isSysLog = TRUE;
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
   logger->needsFileIO = needsFileIO;
   logger->isSysLog = isSysLog;
   logger->confData = (path != NULL ? g_strdup(path) : NULL);
   g_free(path);

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
   gchar *confData = NULL;
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
      level = g_strdup(VMTOOLS_LOGGING_LEVEL_DEFAULT);
   }

   /* Parse the handler information. */
   g_snprintf(key, sizeof key, "%s.handler", domain);
   handler = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);

   g_snprintf(key, sizeof key, "%s.data", domain);
   confData = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);

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
      const char *oldData = oldDefault != NULL ? oldDefault->confData : NULL;

      if (oldtype != NULL && strcmp(oldtype, "file+") == 0) {
         oldtype = "file";
      }

      if (isDefault && oldtype != NULL && strcmp(oldtype, handler) == 0) {
         // check for a filename change
         if (oldData && strcmp(oldData, confData) == 0) {
            data = oldDefault;
         }
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
      data->isSysLog = FALSE;
      data->confData = g_strdup(confData);
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
   g_free(confData);
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
   CLEAR_LOG_HANDLER(gErrorSyslog);
   gErrorData = NULL;
   gErrorSyslog = NULL;

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
 * Configures the logging system to log to the STDIO.
 *
 * @param[in] defaultDomain   Name of the default log domain.
 */

void
VMTools_ConfigLogToStdio(const gchar *domain)
{
   static LogHandler *gStdLogHandler = NULL;
   GKeyFile *cfg;

   g_return_if_fail(gStdLogHandler == NULL); /* Already called */

   ASSERT(domain != NULL);
   gLogDomain = g_strdup(domain);
   cfg = g_key_file_new();
   gStdLogHandler = VMToolsGetLogHandler(STD_HANDLER,
                                         gLogDomain,
                                         ~0,
                                         cfg);
   if (!gStdLogHandler) {
      fprintf(stderr, "Failed to create the STD log handler\n");
      goto exit;
   }

   g_log_set_handler(gLogDomain, ~0, VMToolsLog, gStdLogHandler);

   if (!gLogInitialized) {
      gLogInitialized = TRUE;
      g_static_rec_mutex_init(&gLogStateMutex);
   }

   gLogEnabled = TRUE;

exit:
   g_key_file_free(cfg);
}


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
   GError *err = NULL;

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
    * The syslog handler used for G_LOG_FLAG_FATAL.
    * This is only used if the default handler isn't type 'syslog'.
    */
   gErrorSyslog = VMToolsGetLogHandler("syslog",
                                       gLogDomain,
                                       G_LOG_FLAG_FATAL,
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

   /*
    * Need to set these so that the code below in this function
    * can also log messages.
    */
   gLogEnabled |= force;
   if (!gLogInitialized) {
      gLogInitialized = TRUE;
      g_static_rec_mutex_init(&gLogStateMutex);
   }

   gMaxCacheEntries = g_key_file_get_integer(cfg, LOGGING_GROUP,
                                             "maxCacheEntries", &err);
   if (err != NULL || gMaxCacheEntries < 0) {
      /*
       * Use default value in case of error.
       * A value '0' will turn off log caching.
       */
      gMaxCacheEntries = DEFAULT_MAX_CACHE_ENTRIES;
      if (err != NULL) {
         if (err->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND &&
             err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
            g_warning("Invalid value for maxCacheEntries key: Error %d.",
                      err->code);
         }
         g_clear_error(&err);
      }
   }

   if (gMaxCacheEntries > 0) {
      g_message("Log caching is enabled with maxCacheEntries=%d.",
                gMaxCacheEntries);
   } else {
      g_message("Log caching is disabled.");
   }

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
      if (g_key_file_has_key(cfg, LOGGING_GROUP, "coreDumpFlags", NULL)) {
         guint coreDumpFlags;
         coreDumpFlags = g_key_file_get_integer(cfg, LOGGING_GROUP, "coreDumpFlags", &err);
         if (err != NULL) {
            coreDumpFlags = 0;
            g_clear_error(&err);
         }
         /*
          * For flag values and information on their meanings see:
          * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680519(v=vs.85).aspx
          */
         coreDumpFlags &= MiniDumpValidTypeFlags;
         g_message("Core dump flags set to %u", coreDumpFlags);
         Panic_SetCoreDumpFlags(coreDumpFlags);
      }
      CoreDump_SetUnhandledExceptionFilter();
#else
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
   if (!gLogInitialized && !IS_FATAL(level)) {
      /*
       * Avoid logging without initialization because
       * it leads to spamming of the console output.
       * Fatal messages are exception.
       */
      return;
   }

   VMTools_AcquireLogStateLock();
   if (gLoggingStopped) {
      /* This is to avoid nested logging in vmxLogger */
      VMTools_ReleaseLogStateLock();
      return;
   }
   VMTools_ReleaseLogStateLock();

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
 * Acquire the log state lock.
 */

void
VMTools_AcquireLogStateLock(void)
{
   g_static_rec_mutex_lock(&gLogStateMutex);
}


/**
 * Release the log state lock.
 */

void
VMTools_ReleaseLogStateLock(void)
{
   g_static_rec_mutex_unlock(&gLogStateMutex);
}


/**
 * This is called to avoid nested logging in vmxLogger.
 * NOTE: This must be called after acquiring LogState lock.
 */

void
VMTools_StopLogging(void)
{
   gLoggingStopped = TRUE;
}


/**
 * This is called to reset logging in vmxLogger.
 * NOTE: This must be called after acquiring LogState lock.
 */

void
VMTools_RestartLogging(void)
{
   gLoggingStopped = FALSE;
}


/**
 * Suspend IO caused by logging activity.
 */

void
VMTools_SuspendLogIO()
{
   gLogIOSuspended = TRUE;
}


/**
 * Resume IO caused by logging activity.
 */

void
VMTools_ResumeLogIO()
{
   guint cachedEntries = 0;

   /*
    * Resume the log IO first, so that we can also log messages
    * from within this function itself!
    */
   gLogIOSuspended = FALSE;

   /*
    * Flush the cached log messages, if any
    */
   if (gCachedLogs) {
      cachedEntries = gCachedLogs->len;
      g_ptr_array_foreach(gCachedLogs, VMToolsLogMsg, NULL);
      g_ptr_array_free(gCachedLogs, TRUE);
      gCachedLogs = NULL;
   }

   g_debug("Flushed %u log messages from cache after resuming log IO.",
           cachedEntries);

   if (gDroppedLogCount > 0) {
      g_warning("Dropped %u log messages from cache.", gDroppedLogCount);
      gDroppedLogCount = 0;
   }
}


/**
 * Called if vmtools lib is used along with Guestlib SDK.
 */

void
VMTools_SetGuestSDKMode(void)
{
   gGuestSDKMode = TRUE;
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
   if (gGuestSDKMode) {
      GuestSDK_Debug(fmt, args);
   } else {
      VMToolsLogWrapper(G_LOG_LEVEL_DEBUG, fmt, args);
   }
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
   if (gGuestSDKMode) {
      GuestSDK_Log(fmt, args);
   } else {
      VMToolsLogWrapper(G_LOG_LEVEL_INFO, fmt, args);
   }
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

   if (gGuestSDKMode) {
      GuestSDK_Panic(fmt, args);
   } else {
      if (gPanicCount == 0) {
         char *msg = Str_Vasprintf(NULL, fmt, args);
         if (msg != NULL) {
            g_log(gLogDomain, G_LOG_LEVEL_ERROR, "%s", msg);
            free(msg);
         }
         /*
          * In case an user-installed custom handler doesn't panic on error,
          * force a core dump. Also force a dump in the recursive case.
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
   }
   va_end(args);
   while (1) ; // avoid compiler warning
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
   if (gGuestSDKMode) {
      GuestSDK_Warning(fmt, args);
   } else {
      VMToolsLogWrapper(G_LOG_LEVEL_WARNING, fmt, args);
   }
   va_end(args);
}


/*
 *----------------------------------------------------------------------
 *
 * VMTools_ChangeLogFilePath --
 *
 *     This function gets the log file location given in the config file
 *     and appends the string provided just before the delimiter specified.
 *     If more than one delimiter is present in the string, it appends just
 *     before the first delimiter. If the delimiter does not exist in the
 *     location, the string provided is appended at the end of the location.
 *
 *     NOTE: It is up to the caller to free the delimiter and append string.
 *
 *     Example:
 *     1) location - "C:\vmresset.log", delimiter - ".", appendString - "_4"
 *        location changed = "C:\vmresset_4.log"
 *     2) location - "C:\vmresset", delimiter - ".", appendString - "_4"
 *        location changed = "C:\vmresset_4"
 *     3) location - "C:\vmresset.log.log", delimiter - ".", appendString - "_4"
 *        location changed = "C:\vmresset_4.log.log"
 *
 * Results:
 *     TRUE if log file location is changed, FALSE otherwise.
 *
 * Side effects:
 *     Appends a string into the log file location.
 *
 *----------------------------------------------------------------------
 */

gboolean
VMTools_ChangeLogFilePath(const gchar *delimiter,     // IN
                          const gchar *appendString,  // IN
                          const gchar *domain,        // IN
                          GKeyFile *conf)             // IN, OUT
{
   gchar key[128];
   gchar *path = NULL;
   gchar *userLogTemp = NULL;
   gchar **tokens;
   gboolean retVal = FALSE;

   if (domain == NULL || conf == NULL){
      goto exit;
   }

   g_snprintf(key, sizeof key, "%s.data", domain);
   path = VMToolsGetLogFilePath(key, domain, conf);

   if (path == NULL || appendString == NULL || delimiter == NULL){
      goto exit;
   }

   tokens = g_strsplit(path, delimiter, 2);
   if (tokens != NULL && *tokens != NULL){
      userLogTemp = g_strjoin(appendString, *tokens, " ", NULL);
      userLogTemp = g_strchomp (userLogTemp);
      if (*(tokens+1) != NULL){
         gchar *userLog;
         userLog = g_strjoin(delimiter, userLogTemp, *(tokens+1), NULL);
         g_key_file_set_string(conf, LOGGING_GROUP, key, userLog);
         g_free(userLog);
      } else {
         g_key_file_set_string(conf, LOGGING_GROUP, key, userLogTemp);
      }
      retVal = TRUE;
      g_free(userLogTemp);
   }
   g_strfreev(tokens);

exit:
   if (path){
      g_free(path);
   }

   return retVal;
}

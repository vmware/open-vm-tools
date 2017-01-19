/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * @file log.c
 *
 *    Defines a logging infrastructure based on glib's logging facilities.
 *    Wrap the commonly used logging functions
 *    (Log/Warning/Debug), and provides configurability for where logs should
 *    go to.
 *
 *    To choose the logging domain for your source file, define G_LOG_DOMAIN
 *    before including glib.h.
 *
 *    Based heavily on apps/vmtoolslib/vmtoolsLog.c
 *
 *    This verson is cut down to handle just file-based logging, but I
 *    tried to leave enough structure so that it can be expanded to
 *    support syslog, the event viewer, etc if/when desired.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/resource.h>
#  include <sys/time.h>
#endif
#include "service.h"
#include "buildNumber.h"
#ifdef _WIN32
#include "winCoreDump.h"
#endif

/** Tells whether the given log level is a fatal error. */
#define IS_FATAL(level) ((level) & G_LOG_FLAG_FATAL)

#ifdef _WIN32
static gboolean haveDebugConsole = FALSE;
#endif

/*
 * The glib log levels levels we care about.  We use WARNING, MESSAGE
 * and DEBUG (which we drop by default).
 * Add in CRITICAL, INFO, ERROR in case some glib support
 * code generates them.
 */
#define DEFAULT_LOG_LEVELS (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | \
         G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_INFO)

static GLogLevelFlags logWantedLevel = DEFAULT_LOG_LEVELS;
static gboolean enableCoreDump = TRUE;

static gboolean isLogOnStdout = FALSE;


/*
 ******************************************************************************
 * Service_SetLogOnStdout --                                             */ /**
 *
 * Caller tells us whether the log should go the stdout.
 *
 * @param[in] flag     The flag of whether we should log to the stdout
 ******************************************************************************
 */

void
Service_SetLogOnStdout(gboolean flag)
{
   isLogOnStdout = flag;
   gVerboseLogging = TRUE;
}


#ifdef _WIN32
/*
 ******************************************************************************
 * GetTimeAsString --                                                    */ /**
 *
 * Returns the current time in human-readable format with millisecond
 * precision.
 *
 * @return The current time as a string.  Must be gfree'd by caller.
 ******************************************************************************
 */

gchar *
GetTimeAsString(void)
{
   /*
    * Max abbreviated month length is 80:
    * http://msdn.microsoft.com/en-us/library/dd373831%28VS.85%29.aspx
    */
   char date[128];
   char time[64];
   SYSTEMTIME localTime;
   gchar *output = NULL;

   GetLocalTime(&localTime);

   /* 1: Build the string containing the current date. */
   if (GetDateFormatA(LOCALE_USER_DEFAULT, 0, &localTime,
                      "MMM dd", date, sizeof date) == 0) {
      goto out;
   }

   /* 2: Build the string containing the current time. */
   if (GetTimeFormatA(LOCALE_USER_DEFAULT, 0, &localTime,
                      "HH':'mm':'ss" , time, sizeof time) == 0) {
      goto out;
   }

   /*
    * 3: Join the date and time strings together, and append the milliseconds
    *    field from the local time. This produces the completed timestamp.
    */
   output = g_strdup_printf("%s %s.%03d", date, time, localTime.wMilliseconds);

out:
   return output;
}

#else

/*
 ******************************************************************************
 * GetTimeAsString --                                                    */ /**
 *
 * Returns the current time in human-readable format with millisecond
 * precision.
 *
 * @return The current time as a string.  Must be gfree'd by caller.
 ******************************************************************************
 */

gchar *
GetTimeAsString(void)
{
   struct timeval tv;
   time_t sec;
   int msec;
   size_t charsWritten;
   size_t bufSize = 8; // Multiplied by 2 for the initial allocation.
   gchar *buf = NULL;
   gchar *output = NULL;

   if (gettimeofday(&tv, NULL)) {
      goto out;
   }
   sec = tv.tv_sec;
   msec = tv.tv_usec / 1000;

   /*
    * Loop repeatedly trying to format the time into a buffer, doubling the
    * buffer with each failure. This should be safe as the manpage for
    * strftime(3) seems to suggest that it only fails if the buffer isn't large
    * enough.
    *
    * The resultant string is encoded according to the current locale.
    */
   do {
      gchar *newBuf;
      bufSize *= 2;

      newBuf = g_realloc(buf, bufSize);
      if (newBuf == NULL) {
         goto out;
      }
      buf = newBuf;
      charsWritten = strftime(buf, bufSize, "%b %d %H:%M:%S", localtime(&sec));
   } while (charsWritten == 0);

   /*
    * Append the milliseconds field, but only after converting the date/time
    * string from encoding specified in the current locale to an opaque type.
    */
   output = g_strdup_printf("%s.%03d", buf, msec);

  out:
   g_free(buf);
   return output;
}

#endif // !WIN32

/*
 ******************************************************************************
 * ServiceLogFormat --                                                   */ /**
 *
 * Creates a formatted message to be logged. The format of the message will be:
 *    [timestamp] [domain] [level] Log message
 *
 * @param[in] message      User log message.
 * @param[in] domain       Log domain.
 * @param[in] level        Log level.
 *
 * @return Formatted log message.  Should be g_free()'d.
 *
 ******************************************************************************
 */

static gchar *
ServiceLogFormat(const gchar *message,
                 const gchar *domain,
                 GLogLevelFlags level)
{
   char *msg = NULL;
   const char *slevel;
   gchar *tstamp;

   if (domain == NULL) {
      domain = "VGAuthService";
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

   /*
    * XXX Use g_date_time_format when we move to glib 2.26
    */
   tstamp = GetTimeAsString();

   msg = g_strdup_printf("[%s] [%8s] [%s] %s\n",
                         (tstamp != NULL) ? tstamp : "no time",
                         slevel, domain, message);

   g_free(tstamp);
   if (NULL != msg) {
      size_t len;

      /*
       * The log messages from glib itself (and probably other libraries based
       * on glib) do not include a trailing new line. Most of our code does. So
       * we detect whether the original message already had a new line, and
       * remove it, to avoid having two newlines when printing our log messages.
       */
      len = strlen(msg);

      if (msg[len - 2] == '\n') {
         msg[len - 1] = '\0';
      }
   }

   return msg;
}


/*
 ******************************************************************************
 * ServiceLogPanic --                                                    */ /**
 *
 * Aborts the program, optionally creating a core dump.
 ******************************************************************************
 */

static void
ServiceLogPanic(void)
{
   if (enableCoreDump) {
#if defined(_WIN32)
      Win_MakeCoreDump();
#else
      abort();
#endif
   }
   /* Same behavior as Panic_Panic(). */
   exit(-1);
}


/*
 ******************************************************************************
 * Service_Log --                                                        */ /**
 *
 * Log handler function that does the common processing of log messages,
 * and delegates the actual printing of the message to the file handler.
 *
 * @param[in] domain    Log domain.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] data      User-specified data.
 ******************************************************************************
 */

static void
Service_Log(const gchar *domain,
            GLogLevelFlags level,
            const gchar *message,
            gpointer data)
{
   if (level & logWantedLevel) {
      gchar *msg = ServiceLogFormat(message, domain, level);

      ServiceFileLogger_Log(domain, level, msg, data);

#ifdef _WIN32
      /*
       * Since dup()ing stdio in ServiceInitStdioConsole() doesn't want to work,
       * let's just dump it using the Win32 APIs.
       */
      if (haveDebugConsole) {
         DWORD written;
         HANDLE h = GetStdHandle(STD_ERROR_HANDLE);

         (void) WriteFile(h, msg, (DWORD) strlen(msg), &written, NULL);
      }
#endif

      g_free(msg);
   } else {
#ifdef VMX86_DEBUG
      fprintf(stderr, "%s: not logging message: '%s'\n", __FUNCTION__, message);
#endif
   }

   if (IS_FATAL(level)) {
      ServiceLogPanic();
   }
}


/*
 ******************************************************************************
 * Service_InitLogging --                                                */ /**
 *
 * Initializes the logging system according to the configuration in the given
 * dictionary.
 *
 * @param[in] haveConsole        Should be true in Windows if we have a service
 *                               debug window.
 * @param[in] restarting         Should be set if restarting to catch a pref
 *                               change.
 ******************************************************************************
 */

void
Service_InitLogging(gboolean haveConsole,
                    gboolean restarting)
{
   gboolean logEnabled;

   enableCoreDump = Pref_GetBool(gPrefs,
                                 VGAUTH_PREF_ALLOW_CORE,
                                 VGAUTH_PREF_GROUP_NAME_SERVICE, TRUE);

#ifdef _WIN32
   haveDebugConsole = haveConsole;
#endif

   /*
    * If core dumps are enabled (default: TRUE), then set up the exception
    * filter on Win32. On POSIX systems, try to modify the resource limit
    * to allow core dumps, but don't complain if it fails. Core dumps may
    * still fail, e.g., if the current directory is not writable by the
    * user running the process.
    */
   if (!restarting && enableCoreDump) {
#if defined(_WIN32)
      // XXX: Use a config option, perhaps share the directory with the logs
      Win_EnableCoreDump("C:\\TEMP");
#else
      struct rlimit limit = { 0, 0 };

      getrlimit(RLIMIT_CORE, &limit);
      if (limit.rlim_max != 0) {
         limit.rlim_cur = RLIM_INFINITY;

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

   logEnabled = !isLogOnStdout &&
      Pref_GetBool(gPrefs,
                   VGAUTH_PREF_LOGTOFILE,
                   VGAUTH_PREF_GROUP_NAME_SERVICE, TRUE);

   if (logEnabled) {
      void *data = ServiceFileLogger_Init();
      gchar *loglevel;

      if (NULL != data) {
         (void) g_log_set_default_handler((GLogFunc) Service_Log, data);
      } else {
         fprintf(stderr, "%s: Unable to set up file logger\n", __FUNCTION__);
      }

      loglevel = Pref_GetString(gPrefs,
                                VGAUTH_PREF_NAME_LOGLEVEL,
                                VGAUTH_PREF_GROUP_NAME_SERVICE,
                                SERVICE_LOGLEVEL_NORMAL);

      if (g_ascii_strcasecmp(loglevel, SERVICE_LOGLEVEL_NORMAL) == 0) {
         logWantedLevel = DEFAULT_LOG_LEVELS;
      } else if (g_ascii_strcasecmp(loglevel, SERVICE_LOGLEVEL_VERBOSE) == 0) {
         logWantedLevel = DEFAULT_LOG_LEVELS | G_LOG_LEVEL_DEBUG;
         gVerboseLogging = TRUE;
      } else {
         logWantedLevel = DEFAULT_LOG_LEVELS;
#ifdef VMX86_DEBUG
         // add DEBUG for obj builds
         logWantedLevel |= G_LOG_LEVEL_DEBUG;
         gVerboseLogging = TRUE;
#endif
         Warning("%s: Unrecognized loglevel '%s'\n", __FUNCTION__, loglevel);
      }
      Log("VGAuthService%s '%s' logging at level '%s'\n",
          restarting ? " resetting" : "",
          BUILD_NUMBER,
          loglevel ? loglevel : "<default>");
      g_free(loglevel);

      /*
       * Once logging is set, dump all prefs so we know all the settings.
       * (This also works around the chicken-and-egg issue where any
       * noise from Pref_Init() is lost).
       */
      Pref_LogAllEntries(gPrefs);

   }
}

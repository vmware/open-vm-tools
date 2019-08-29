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
 * @file sysLogger.c
 *
 * Logger that writes to syslog(3). Since there's only one "syslog connection"
 * for the whole application, this code does reference counting to allow
 * different domains to be configured with a "syslog" handler, and still be
 * able to call closelog(3) when appropriate.
 */

#include "glibUtils.h"
#include <stdio.h>
#include <string.h>
#include <syslog.h>

typedef struct SysLogger {
   GlibLogger  handler;
   gchar      *domain;
   gint        refcount;
} SysLogger;


static SysLogger *gSysLogger;
static GMutex gSysLoggerLock;


/*
 *******************************************************************************
 * SysLoggerLog --                                                        */ /**
 *
 * @brief Sends the given log message to syslog.
 *
 * @param[in] domain     Unused.
 * @param[in] level      Log level.
 * @param[in] message    Message to log.
 * @param[in] data       Unused.
 *
 ******************************************************************************
 */

static void
SysLoggerLog(const gchar *domain,
             GLogLevelFlags level,
             const gchar *message,
             gpointer data)
{
   gchar *local;
   int priority;

   /* glib and syslog disagree about critical / error. */
   if (level & G_LOG_LEVEL_ERROR) {
      priority = LOG_CRIT;
   } else if (level & G_LOG_LEVEL_CRITICAL) {
      priority = LOG_ERR;
   } else if (level & G_LOG_LEVEL_WARNING) {
      priority = LOG_WARNING;
   } else if (level & G_LOG_LEVEL_MESSAGE) {
      priority = LOG_NOTICE;
   } else if (level & G_LOG_LEVEL_INFO) {
      priority = LOG_INFO;
   } else {
      priority = LOG_DEBUG;
   }

   local = g_locale_from_utf8(message, -1, NULL, NULL, NULL);
   if (local != NULL) {
      syslog(priority, "%s", local);
      g_free(local);
   } else {
      syslog(priority, "%s", message);
   }
}


/*
 *******************************************************************************
 * SysLoggerUnref --                                                      */ /**
 *
 * @brief Decreases the ref count and closes syslog if it reaches 0.
 *
 * @param[in] data   Unused.
 *
 *******************************************************************************
 */

static void
SysLoggerUnref(gpointer data)
{
   g_return_if_fail(data == gSysLogger);
   g_return_if_fail(gSysLogger->refcount > 0);
   g_mutex_lock(&gSysLoggerLock);
   gSysLogger->refcount -= 1;
   if (gSysLogger->refcount == 0) {
      closelog();
      g_free(gSysLogger->domain);
      g_free(gSysLogger);
      gSysLogger = NULL;
   }
   g_mutex_unlock(&gSysLoggerLock);
}


/*
 *******************************************************************************
 * GlibUtils_CreateSysLogger --                                           */ /**
 *
 * @brief Initializes syslog if it hasn't been done yet.
 *
 * Since syslog is shared, it's not recommended to change the default domain
 * during the lifetime of the application, since that may not reflect on the
 * syslogs (and, when it does, it might be confusing).
 *
 * @param[in] domain    Application name, used as the syslog identity.
 * @param[in] facility  Facility to use. One of: "daemon", "local[0-7]",
                        "user" (default).
 *
 * @return Syslog logger data.
 *
 *******************************************************************************
 */

GlibLogger *
GlibUtils_CreateSysLogger(const char *domain,
                          const char *facility)
{
   g_mutex_lock(&gSysLoggerLock);
   if (gSysLogger == NULL) {
      int facid = LOG_USER;

      if (facility != NULL) {
         int idx;
         if (strcmp(facility, "daemon") == 0) {
            facid = LOG_DAEMON;
         } else if (sscanf(facility, "local%d", &idx) == 1) {
            switch (idx) {
            case 0:
               facid = LOG_LOCAL0;
               break;

            case 1:
               facid = LOG_LOCAL1;
               break;

            case 2:
               facid = LOG_LOCAL2;
               break;

            case 3:
               facid = LOG_LOCAL3;
               break;

            case 4:
               facid = LOG_LOCAL4;
               break;

            case 5:
               facid = LOG_LOCAL5;
               break;

            case 6:
               facid = LOG_LOCAL6;
               break;

            case 7:
               facid = LOG_LOCAL7;
               break;

            default:
               g_message("Invalid local facility for %s: %s\n", domain, facility);
               break;
            }
         } else if (strcmp(facility, "user") != 0) {
            g_message("Invalid syslog facility for %s: %s\n", domain, facility);
         }
      }

      gSysLogger = g_new0(SysLogger, 1);
      gSysLogger->handler.addsTimestamp = TRUE;
      gSysLogger->handler.shared = FALSE;
      gSysLogger->handler.logfn = SysLoggerLog;
      gSysLogger->handler.dtor = SysLoggerUnref;

      gSysLogger->domain = g_strdup(domain);
      gSysLogger->refcount = 1;
      openlog(gSysLogger->domain, LOG_CONS | LOG_PID, facid);
   } else {
      gSysLogger->refcount += 1;
   }
   g_mutex_unlock(&gSysLoggerLock);
   return &gSysLogger->handler;
}


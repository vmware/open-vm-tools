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
 * @file sysLogger.c
 *
 * Logger that writes to syslog(3). Since there's only one "syslog connection"
 * for the whole application, this code does reference counting to allow
 * different domains to be configured with a "syslog" handler, and still be
 * able to call closelog(3) when appropriate.
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "vmware.h"
#include "vmtoolsInt.h"

typedef struct SysLogData {
   LogHandlerData    handler;
   gint              refcount;
} SysLogData;


static SysLogData *gSysLogger;
static GStaticMutex gSysLoggerLock = G_STATIC_MUTEX_INIT;


/*
 ******************************************************************************
 * VMSysLoggerLog --                                                    */ /**
 *
 * Sends the given log message to syslog.
 *
 * @param[in] domain     Unused.
 * @param[in] level      Log level.
 * @param[in] message    Message to log.
 * @param[in] _data      Unused.
 * @param[in] errfn      Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static gboolean
VMSysLoggerLog(const gchar *domain,
               GLogLevelFlags level,
               const gchar *message,
               LogHandlerData *_data,
               LogErrorFn errfn)
{
   int priority;

   /* glib and syslog disagree about critical / error. */
   if (level | G_LOG_LEVEL_ERROR) {
      priority = LOG_CRIT;
   } else if (level | G_LOG_LEVEL_CRITICAL) {
      priority = LOG_ERR;
   } else if (level | G_LOG_LEVEL_WARNING) {
      priority = LOG_WARNING;
   } else if (level | G_LOG_LEVEL_MESSAGE) {
      priority = LOG_NOTICE;
   } else if (level | G_LOG_LEVEL_INFO) {
      priority = LOG_INFO;
   } else {
      priority = LOG_DEBUG;
   }

   syslog(priority, "%s", message);
   return TRUE;
}


/*
 ******************************************************************************
 * VMSysLoggerUnref --                                                  */ /**
 *
 * Decreases the ref count and closes syslog if it reaches 0.
 *
 * @param[in] _data     Unused.
 *
 ******************************************************************************
 */

static void
VMSysLoggerUnref(LogHandlerData *_data)
{
   ASSERT(_data == (LogHandlerData *) gSysLogger);
   g_static_mutex_lock(&gSysLoggerLock);
   ASSERT(gSysLogger->refcount > 0);
   gSysLogger->refcount -= 1;
   if (gSysLogger->refcount == 0) {
      closelog();
      g_free(gSysLogger);
      gSysLogger = NULL;
   }
   g_static_mutex_unlock(&gSysLoggerLock);
}


/*
 ******************************************************************************
 * VMSysLoggerConfig --                                                 */ /**
 *
 * Initializes syslog if it hasn't been done yet.
 *
 * Since syslog is shared, it's not recommended to change the default domain
 * during the lifetime of the application, since that may not reflect on the
 * syslogs (and, when it does, it might be confusing).
 *
 * @param[in] defaultDomain    Application name, used as the syslog identity.
 * @param[in] domain           Name of log domain.
 * @param[in] name             Name of log handler.
 * @param[in] cfg              Configuration data.
 *
 * @return Syslog logger data.
 *
 ******************************************************************************
 */

LogHandlerData *
VMSysLoggerConfig(const gchar *defaultDomain,
                  const gchar *domain,
                  const gchar *name,
                  GKeyFile *cfg)
{
   g_static_mutex_lock(&gSysLoggerLock);
   if (gSysLogger == NULL) {
      int facility = LOG_USER;
      gchar *facstr;
      gchar key[128];

      g_snprintf(key, sizeof key, "%s.facility", defaultDomain);
      facstr = g_key_file_get_string(cfg, LOGGING_GROUP, key, NULL);
      if (facstr != NULL) {
         int idx;
         /*
          * We only allow switching to LOG_DAEMON and LOG_LOCAL*. Since the
          * facility is tied to the log domain configuration, we force using
          * the default domain to retrieve this option.
          */
         if (strcmp(facstr, "daemon") == 0) {
            facility = LOG_DAEMON;
         } else if (sscanf(facstr, "local%d", &idx) == 1) {
            switch (idx) {
            case 0:
               facility = LOG_LOCAL0;
               break;

            case 1:
               facility = LOG_LOCAL1;
               break;

            case 2:
               facility = LOG_LOCAL2;
               break;

            case 3:
               facility = LOG_LOCAL3;
               break;

            case 4:
               facility = LOG_LOCAL4;
               break;

            case 5:
               facility = LOG_LOCAL5;
               break;

            case 6:
               facility = LOG_LOCAL6;
               break;

            case 7:
               facility = LOG_LOCAL7;
               break;

            default:
               g_message("Invalid local facility for %s: %s\n", defaultDomain, facstr);
               break;
            }
         } else if (strcmp(facstr, "user") != 0) {
            g_message("Invalid syslog facility for %s: %s\n", defaultDomain, facstr);
         }
         g_free(facstr);
      }

      gSysLogger = g_new0(SysLogData, 1);
      gSysLogger->handler.logfn = VMSysLoggerLog;
      gSysLogger->handler.convertToLocal = TRUE;
      gSysLogger->handler.timestamp = FALSE;
      gSysLogger->handler.shared = FALSE;
      gSysLogger->handler.copyfn = NULL;
      gSysLogger->handler.dtor = VMSysLoggerUnref;

      gSysLogger->refcount = 1;
      openlog(defaultDomain, LOG_CONS | LOG_PID, facility);
   } else {
      gSysLogger->refcount += 1;
   }
   g_static_mutex_unlock(&gSysLoggerLock);
   return &gSysLogger->handler;
}


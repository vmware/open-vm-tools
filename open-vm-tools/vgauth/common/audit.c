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

/*
 * @file audit.h
 *
 * Auditing support.
 */

#include "audit.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include "auditMessages.h"
#include <windows.h>
#else
#include <syslog.h>
#endif
#include "VGAuthLog.h"

/*
 * Since the syslog() APIs are global, the audit system must be as well.
 */

static gboolean doLogSuccess = TRUE;
static gboolean auditInited = FALSE;
#ifdef _WIN32
static HANDLE hAuditSource = INVALID_HANDLE_VALUE;
#endif

/*
 ******************************************************************************
 * Audit_Init --                                                         */ /**
 *
 * Initializes the audit library.
 *
 * @param[in] appName     The application name, which is included as part of the
 *                        audit message.
 * @param[in] logSuccess  If set, audit success messages are logged.
 *
 ******************************************************************************
 */

void
Audit_Init(const char *appName,
           gboolean logSuccess)
{
#ifdef _WIN32
   wchar_t *appName16 =
      (wchar_t *) g_utf8_to_utf16(appName, -1, NULL, NULL, NULL);
   hAuditSource = RegisterEventSourceW(NULL, appName16);
   if (NULL == hAuditSource) {
      VGAUTH_LOG_ERR_WIN("RegisterEventSourceW() failed");
   }
   g_free(appName16);

#else // !_WIN32

   int facility;

   /*
    * LOG_AUTHPRIV is the new name on Linux.
    * Solaris (and older Linux?) want LOG_AUTH.
    */
#ifdef LOG_AUTHPRIV
   facility = LOG_AUTHPRIV;
#else
   facility = LOG_AUTH;
#endif

   openlog(appName, LOG_PID, facility);
#endif

   auditInited = TRUE;
   doLogSuccess = logSuccess;
}


/*
 ******************************************************************************
 * Audit_Shutdown --                                                     */ /**
 *
 * Shuts down the audit library.
 *
 ******************************************************************************
 */

void
Audit_Shutdown(void)
{
#ifdef _WIN32
   DeregisterEventSource(hAuditSource);
   hAuditSource = INVALID_HANDLE_VALUE;
#else
   closelog();
#endif
   auditInited = FALSE;
}


/*
 ******************************************************************************
 * Audit_Event --                                                         */ /**
 *
 * Logs an auditing event.
 * Note that a final '.' in the @a fmt is added by the underlying system
 * (Windows events add them.)
 *
 * @param[in] isSuccess   If true, the message is a successful event.
 * @param[in] fmt         The format message for the event.
 *
 ******************************************************************************
 */

void
Audit_Event(gboolean isSuccess,
            const char *fmt,
            ...)
{
   va_list args;

   va_start(args, fmt);
   Audit_EventV(isSuccess, fmt, args);
   va_end(args);
}


/*
 ******************************************************************************
 * Audit_EventV --                                                       */ /**
 *
 * Logs an auditing event using va_list arguments.
 * Note that a final '.' in the @a fmt is added by the underlying system
 * (Windows events add them.)
 *
 * @param[in] isSuccess   If true, the message is a successful event.
 * @param[in] fmt         The format message for the event.
 * @param[in] args        The arguments for @a fmt.
 *
 ******************************************************************************
 */

void
Audit_EventV(gboolean isSuccess,
             const char *fmt,
             va_list args)
{
   gchar *buf;

   if (isSuccess && !doLogSuccess) {
      return;
   }
   buf = g_strdup_vprintf(fmt, args);

#ifdef VMX86_DEBUG
   if (!auditInited) {
      fprintf(stderr, "Audit Event being dropped!: %s\n", buf);
      goto done;
   }
#endif

#ifdef _WIN32
   {
      wchar_t *buf16 = (wchar_t *) g_utf8_to_utf16(buf, -1, NULL, NULL, NULL);

      ReportEventW(hAuditSource,
                   isSuccess ? EVENTLOG_AUDIT_SUCCESS : EVENTLOG_AUDIT_FAILURE,
                   0,                        // category
                   VGAUTH_AUDIT_MESSAGE,     // Event ID
                   NULL,                     // user
                   1,                        // numStrings
                   0,                        // data size
                   &buf16,                   // string array
                   NULL);                    // any binary data

      g_free(buf16);
   }
#else
   /*
    * XXX
    *
    * This may need tuning.  Other apps (sshd) seems to use LOG_INFO
    * for both success and failure events, but that feels wrong.
    */
   /*
    * The message gets a final '.' in the event viewer.  Add one here for
    * syslog()
    */
   syslog(isSuccess ? LOG_INFO : LOG_WARNING, "%s.", buf);
#endif

#ifdef VMX86_DEBUG
done:
#endif
   g_free(buf);
}


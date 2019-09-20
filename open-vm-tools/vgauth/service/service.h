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
 * @file service.h --
 *
 *    Stand-alone service functions and data types.
 */

#ifndef _SERVICE_H_
#define _SERVICE_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "VGAuthError.h"
#include "VGAuthLog.h"
#include "serviceInt.h"
#include "audit.h"
#include "vmxlog.h"


#define VGAUTH_SERVICE_NAME "VGAuthService"

VGAuthError ServiceIOStartListen(ServiceConnection *conn);

VGAuthError ServiceIOPrepareMainLoop(void);

VGAuthError ServiceIOMainLoop(void);

VGAuthError ServiceStopIO(ServiceConnection *conn);

#ifdef _WIN32
VGAuthError ServiceIORegisterQuitEvent(HANDLE hQuitEvent);

gboolean ServiceRegisterService(gboolean bRegister,
                                const gchar *name,
                                const gchar *displayName,
                                const gchar *description,
                                const gchar *binaryPath,
                                gchar **errString);

gboolean ServiceInitStdioConsole(void);
#endif

#ifndef _WIN32
typedef enum ServiceDaemonizeFlags {
   SERVICE_DAEMONIZE_DEFAULT = 0,
   SERVICE_DAEMONIZE_LOCKPID = (1 << 0),
} ServiceDaemonizeFlags;

gboolean ServiceDaemonize(const char *path,
                          char * const *args,
                          ServiceDaemonizeFlags flags,
                          const char *pidPath);

GSource *ServiceNewSignalSource(int signum);

void ServiceSetSignalHandlers(void);

gboolean ServiceSuicide(const char *pidPath);
#endif

#ifdef _WIN32
#define LOGFILENAME_DEFAULT "vgauthsvclog.txt"
#define LOGFILENAME_PATH_DEFAULT "c:\\temp\\" LOGFILENAME_DEFAULT
#else
#define LOGFILENAME_PATH_DEFAULT "/var/log/vmware-vgauthsvc.log"
#endif

void Service_SetLogOnStdout(gboolean flag);
void Service_InitLogging(gboolean haveDebugConsole, gboolean restart);
void *ServiceFileLogger_Init(void);
gboolean ServiceFileLogger_Log(const gchar *domain,
                               GLogLevelFlags level,
                               const gchar *message,
                               gpointer data);

#endif   // _SERVICE_H_

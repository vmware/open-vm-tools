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

#ifndef _VMTOOLSINT_H_
#define _VMTOOLSINT_H_

/**
 * @file vmtoolsInt.h
 *
 * Internal definitions used by the vmtools library.
 */

#include "vmware.h"
#include "vmware/tools/utils.h"

/* ************************************************************************** *
 * Internationalization.                                                      *
 * ************************************************************************** */

void
VMToolsMsgCleanup(void);

/* ************************************************************************** *
 * Logging.                                                                   *
 * ************************************************************************** */

#define LOGGING_GROUP         "logging"

struct LogHandlerData;

typedef void (*LogErrorFn)(const gchar *domain,
                           GLogLevelFlags level,
                           const gchar *fmt,
                           ...);
typedef gboolean (*VMToolsLogFn)(const gchar *domain,
                                 GLogLevelFlags level,
                                 const gchar *message,
                                 struct LogHandlerData *data,
                                 LogErrorFn errfn);
typedef void (*LogHandlerDestroyFn)(struct LogHandlerData *data);
typedef void (*LogHandlerCopyFn)(struct LogHandlerData *current,
                                 struct LogHandlerData *old);

typedef struct LogHandlerData {
   VMToolsLogFn         logfn;            ///< Function that does the logging.
                                          ///  Same as GLogFunc but returns
                                          ///  whether the message was
                                          ///  successfully logged.
   gboolean             convertToLocal;   ///< Whether to config the message to the
                                          ///  local encoding before printing.
   gboolean             timestamp;        ///< Whether to include timestamps in
                                          ///  the log message.
   gboolean             shared;           ///< Whether the log output is shared
                                          ///  among various processes.
   LogHandlerCopyFn     copyfn;           ///< Copy function (optional). This is
                                          ///  used when replacing an existing
                                          ///  config with a new one for the
                                          ///  same handler.
   LogHandlerDestroyFn  dtor;             ///< Destructor for the handler data.
   /* Fields below managed by the common code. */
   guint                type;
   gchar               *domain;
   GLogLevelFlags       mask;
   guint                handlerId;
   gboolean             inherited;
} LogHandlerData;


LogHandlerData *
VMFileLoggerConfig(const gchar *defaultDomain,
                   const gchar *domain,
                   const gchar *name,
                   GKeyFile *cfg);

LogHandlerData *
VMStdLoggerConfig(const gchar *defaultDomain,
                  const gchar *domain,
                  const gchar *name,
                  GKeyFile *cfg);

#if defined(_WIN32)
LogHandlerData *
VMDebugOutputConfig(const gchar *defaultDomain,
                    const gchar *domain,
                    const gchar *name,
                    GKeyFile *cfg);
#else
LogHandlerData *
VMSysLoggerConfig(const gchar *defaultDomain,
                  const gchar *domain,
                  const gchar *name,
                  GKeyFile *cfg);
#endif

LogHandlerData *
VMXLoggerConfig(const gchar *defaultDomain,
                const gchar *domain,
                const gchar *name,
                GKeyFile *cfg);

/* ************************************************************************** *
 * Miscelaneous.                                                              *
 * ************************************************************************** */

gint
VMToolsAsprintf(gchar **string,
                gchar const *format,
                ...)  PRINTF_DECL(2, 3);

#endif /* _VMTOOLSINT_H_ */


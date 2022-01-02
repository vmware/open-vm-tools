/*********************************************************
 * Copyright (C) 2011-2017,2021 VMware, Inc. All rights reserved.
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

#ifndef _GLIBUTILS_H_
#define _GLIBUTILS_H_

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @file glibUtils.h
 *
 * A collection of utility functions that depend only on glib.
 *
 * These functions are guaranteed to have no dependencies on bora/lib libraries
 * or headers.
 */

#include <glib.h>
#if defined(_WIN32)
#  include <windows.h>
#endif

/**
 * @brief Description for a logger.
 *
 * Contains information about a logger. The properties here are aimed at
 * helping the logging code using this library to construct and appropriate
 * log message depending on the output being used.
 *
 * For example, some sinks (like syslog) already add a timestamp to every log
 * message. So if the @a addsTimestamp field is TRUE, the logging code can
 * choose to rely on that and not add a redundant timestamp field to the log
 * message.
 */
typedef struct GlibLogger {
   gboolean          shared;        /**< Output is shared with other processes. */
   gboolean          addsTimestamp; /**< Output adds timestamp automatically. */
   GLogFunc          logfn;         /**< The function that writes to the output. */
   GDestroyNotify    dtor;          /**< Destructor. */
   gboolean          logHeader;     /**< Header needs to be logged. */
} GlibLogger;


GlibLogger *
GlibUtils_CreateFileLogger(const char *path,
                           gboolean append,
                           guint maxSize,
                           guint maxFiles);

GlibLogger *
GlibUtils_CreateStdLogger(void);

#if defined(_WIN32)

gboolean
GlibUtils_AttachConsole(void);

GlibLogger *
GlibUtils_CreateDebugLogger(void);

GlibLogger *
GlibUtils_CreateEventLogger(const wchar_t *source,
                            DWORD eventId);

#else

GlibLogger *
GlibUtils_CreateSysLogger(const char *domain,
                          const char *facility);

#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _GLIBUTILS_H_ */

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

#ifndef _VMTOOLS_H_
#define _VMTOOLS_H_

/**
 * @file vmtools.h
 *
 *    Public functions from the VMTools shared library.
 *
 * @addtogroup vmtools_utils
 * @{
 */

#if !defined(G_LOG_DOMAIN)
#  define G_LOG_DOMAIN VMTools_GetDefaultLogDomain()
#endif

#define  VMTOOLS_GUEST_SERVICE   "vmsvc"
#define  VMTOOLS_USER_SERVICE    "vmusr"

/* Needs to come before glib.h. */
const char *
VMTools_GetDefaultLogDomain(void);

#include <glib.h>
#if defined(G_PLATFORM_WIN32)
#  include <windows.h>
#else
#  include <signal.h>
#  include <sys/time.h>
#endif

/**
 * Converts an UTF-8 path to the local (i.e., glib) file name encoding.
 * This is a no-op on Windows, since the local encoding is always UTF-8
 * in glib. The returned value should not be freed directly; instead,
 * use VMTOOLS_FREE_FILENAME.
 *
 * @param[in]  path  Path in UTF-8 (should not be NULL).
 * @param[out] err   Where to store errors (type: GError **; may be NULL).
 *
 * @return The path in glib's filename encoding, or NULL on error.
 */

#if defined(G_PLATFORM_WIN32)
#  define VMTOOLS_GET_FILENAME_LOCAL(path, err) (gchar *) (path)
#else
#  define VMTOOLS_GET_FILENAME_LOCAL(path, err) g_filename_from_utf8((path),  \
                                                                     -1,      \
                                                                     NULL,    \
                                                                     NULL,    \
                                                                     (err))
#endif

/**
 * Frees a path allocated with VMTOOLS_GET_FILENAME_LOCAL. No-op on Windows.
 *
 * @param[in]  path  Path in UTF-8.
 */

#if defined(G_PLATFORM_WIN32)
#  define VMTOOLS_RELEASE_FILENAME_LOCAL(path)   (void) (path)
#else
#  define VMTOOLS_RELEASE_FILENAME_LOCAL(path)   g_free(path)
#endif


void
vm_free(void *ptr);

void
VMTools_SetDefaultLogDomain(const gchar *domain);

void
VMTools_ConfigLogging(GKeyFile *cfg);

void
VMTools_EnableLogging(gboolean enable);

gchar *
VMTools_GetToolsConfFile(void);

GKeyFile *
VMTools_LoadConfig(const gchar *path,
                   GKeyFileFlags flags,
                   gboolean autoUpgrade);


gboolean
VMTools_ReloadConfig(const gchar *path,
                     GKeyFileFlags flags,
                     GKeyFile **config,
                     time_t *mtime);

gboolean
VMTools_WriteConfig(const gchar *path,
                    GKeyFile *config,
                    GError **err);

#if defined(G_PLATFORM_WIN32)

GSource *
VMTools_NewHandleSource(HANDLE h);

#else

/** Type of callback used by the signal event source. */
typedef gboolean (*SignalSourceCb)(const siginfo_t *, gpointer);

GSource *
VMTools_NewSignalSource(int signum);

#endif

GArray *
VMTools_WrapArray(gconstpointer data,
                  guint elemSize,
                  guint count);

/** @} */

#endif /* _VMTOOLS_H_ */


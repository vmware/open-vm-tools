/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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

#ifndef _VMWARE_TOOLS_UTILS_H_
#define _VMWARE_TOOLS_UTILS_H_

/**
 * @file utils.h
 *
 *    Public functions from the VMTools shared library, and other definitions.
 *
 * @defgroup vmtools_utils Utility Functions
 * @{
 *
 * @brief A collection of useful functions.
 *
 * This module contains functions for loading configuration data and extensions
 * to the glib API that are useful when writing applications.
 *
 */

#define  VMTOOLS_GUEST_SERVICE   "vmsvc"
#define  VMTOOLS_USER_SERVICE    "vmusr"

#if defined(__cplusplus)
#  define VMTOOLS_EXTERN_C extern "C"
#else
#  define VMTOOLS_EXTERN_C
#endif

#include <glib.h>
#if defined(G_PLATFORM_WIN32)
#  include <windows.h>
#else
#  include <signal.h>
#  include <sys/time.h>
#endif


/* Work around a glib limitation: it doesn't set G_INLINE_FUNC on Win32. */
#if defined(G_PLATFORM_WIN32)
#  if defined(G_INLINE_FUNC)
#     undef G_INLINE_FUNC
#  endif
#  define G_INLINE_FUNC static __inline
#endif

#ifndef ABS
#  define ABS(x) (((x) >= 0) ? (x) : -(x))
#endif


/**
 * Converts an UTF-8 path to the local (i.e., glib) file name encoding.
 * This is a no-op on Windows, since the local encoding is always UTF-8
 * in glib. The returned value should not be freed directly; instead,
 * use VMTOOLS_RELEASE_FILENAME_LOCAL.
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

/** Convenience macro around VMTools_WrapArray. */
#define VMTOOLS_WRAP_ARRAY(a) VMTools_WrapArray((a), sizeof *(a), G_N_ELEMENTS(a))


G_BEGIN_DECLS

void
vm_free(void *ptr);

gboolean
VMTools_LoadConfig(const gchar *path,
                   GKeyFileFlags flags,
                   GKeyFile **config,
                   time_t *mtime);

gboolean
VMTools_WriteConfig(const gchar *path,
                    GKeyFile *config,
                    GError **err);

gboolean
VMTools_ChangeLogFilePath(const gchar *delimiter,
                          const gchar *appendString,
                          const gchar *domain,
                          GKeyFile *conf);

gboolean
VMTools_ConfigGetBoolean(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         const gboolean defValue);

gint
VMTools_ConfigGetInteger(GKeyFile *config,
                         const gchar *section,
                         const gchar *key,
                         const gint defValue);

gchar *
VMTools_ConfigGetString(GKeyFile *config,
                        const gchar *section,
                        const gchar *key,
                        const gchar *defValue);

#if defined(G_PLATFORM_WIN32)

gboolean
VMTools_AttachConsole(void);

GSource *
VMTools_NewHandleSource(HANDLE h);

#else

/** Type of callback used by the signal event source. */
typedef gboolean (*SignalSourceCb)(const siginfo_t *, gpointer);

GSource *
VMTools_NewSignalSource(int signum);

gchar *
VMTools_GetLibdir(void);

#endif

GSource *
VMTools_CreateTimer(gint timeout);

void
VMTools_SetGuestSDKMode(void);

void
VMTools_AcquireLogStateLock(void);

void
VMTools_ReleaseLogStateLock(void);

gchar *
VMTools_GetTimeAsString(void);

void
VMTools_SuspendLogIO(void);

void
VMTools_ResumeLogIO(void);

GArray *
VMTools_WrapArray(gconstpointer data,
                  guint elemSize,
                  guint count);

G_END_DECLS

/** @} */

#endif /* _VMWARE_TOOLS_UTILS_H_ */


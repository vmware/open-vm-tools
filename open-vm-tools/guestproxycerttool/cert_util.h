/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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

#ifndef _CERT_UTIL_H_
#define _CERT_UTIL_H_

#include <glib.h>
#include <glib/gstdio.h>

/*
 *----------------------------------------------------------------------
 *
 * Error --
 *
 *    Prefix the error message with the program name and output the
 *    message to the standard error.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

#ifdef _WIN32
#define Error(fmt, ...)                                         \
   fprintf(stderr, "%s: " fmt, g_get_prgname(), __VA_ARGS__);
#else
#define Error(fmt, args...)                                     \
   fprintf(stderr, "%s: " fmt, g_get_prgname(), ##args);
#endif

gchar *
CertUtil_CreateCertFileName(const gchar *certDir, // IN
                            const gchar *hash,    // IN
                            int version);         // IN

gboolean
CertUtil_FindCert(const gchar *certFile,          // IN
                  const gchar *certDir,           // IN
                  const gchar *hash,              // IN
                  int *num,                       // OUT
                  int *last);                     // OUT

gboolean
CertUtil_CopyFile(const gchar *src,               // IN
                  const gchar *dst);              // IN

gboolean
CertUtil_RemoveDir(const gchar *dirToRemove);     // IN

const gchar *CertUtil_GetToolDir(void);

gboolean CheckRootPriv(void);

#endif // #ifndef _CERT_UTIL_H_

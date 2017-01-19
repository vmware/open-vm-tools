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

#ifndef _USERCHECK_H_
#define _USERCHECK_H_

/*
 * @file usercheck.h
 *
 * User existance support.
 */

#include <glib.h>
#include "VGAuthError.h"
#ifdef _WIN32
#include "winUtil.h"
#else
#include <sys/types.h>
#include <pwd.h>
#endif

gboolean UsercheckUserExists(const gchar *userName);
#ifndef _WIN32
VGAuthError UsercheckLookupUser(const gchar *userName,
                                uid_t *uid,                       // OUT
                                gid_t *gid);                      // OUT

VGAuthError UsercheckLookupUid(uid_t uid,
                               gchar **userName);

#endif

#ifdef _WIN32
gboolean Usercheck_IsAdminMember(const gchar *userName);
#endif

gboolean Usercheck_CompareByName(const char *u1, const char *u2);

gboolean Usercheck_UsernameIsLegal(const char *userName);

#endif // _USERCHECK_H_

/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * hgfsUri.h
 *
 *    Provides a library for guest applications to convert local pathames to
 *    x-vmware-share:// style URIs
 */

#ifndef _HGFS_URI_H_
#define _HGFS_URI_H_

#include "vm_basic_types.h"
#include "unicode.h"

#if defined(_WIN32)
char *HgfsUri_ConvertFromUtf16ToHgfsUri(wchar_t *pathNameUtf16, Bool hgfsOnly);
#endif // _WIN32
#if defined __linux__ || defined __APPLE__
char *HgfsUri_ConvertFromPathToHgfsUri(const char *pathName, Bool hgfsOnly);
#endif // POSIX

#endif

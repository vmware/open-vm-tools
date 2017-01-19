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

/**
 * @file winToken.h --
 *
 * NtCReateToken() support.
 *
 */


#ifndef _VGAUTH_CREATE_TOKEN_H_
#define _VGAUTH_CREATE_TOKEN_H_

#include <Windows.h>

// obsolete NtCreateToken() wrappers
HANDLE WinToken_Create(const gchar *user);
HANDLE WinTokenCreateImpl(const gchar *user);

#endif // _VGAUTH_CREATE_TOKEN_H_

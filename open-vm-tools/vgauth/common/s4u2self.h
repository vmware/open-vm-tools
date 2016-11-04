/*********************************************************
 * Copyright (C) 2012-2016 VMware, Inc. All rights reserved.
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
 * @file s4u2self.h --
 *
 *    Code to use the Windows Service-for-User-to-Self extension.
 */


#ifndef _VGAUTH_S4U2SELF_H_
#define _VGAUTH_S4U2SELF_H_

#include <windows.h>

DWORD Win_CreateS4UTokenForUser(const char *userName, HANDLE *userTokenRet);

#endif   // _VGAUTH_S4U2SELF_H_

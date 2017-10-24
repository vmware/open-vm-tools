/*********************************************************
 * Copyright (C) 2003-2017 VMware, Inc. All rights reserved.
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
 * impersonate.h --
 *
 *	Provides functions to assist in impersonating and unimpersonating
 *	as a given user.
 */

#ifndef _IMPERSONATE_H_
#define _IMPERSONATE_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "auth.h"

#if defined(__cplusplus)
extern "C" {
#endif

void Impersonate_Init(void);

Bool Impersonate_Owner(const char *file);
Bool Impersonate_Do(const char *user, AuthToken token);
Bool Impersonate_Undo(void);
char *Impersonate_Who(void);

Bool Impersonate_ForceRoot(void);
Bool Impersonate_UnforceRoot(void);

Bool Impersonate_Runas(const char *cfg, const char *caller, 
                       AuthToken callerToken);

#ifdef _WIN32
Bool Impersonate_CfgRunasOnly(const char *cfg);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _IMPERSONATE_H_

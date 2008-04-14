/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

extern void Impersonate_Init(void);

extern Bool Impersonate_Owner(const char *file);
extern Bool Impersonate_Do(const char *user, AuthToken token);
extern Bool Impersonate_Undo(void);
extern char *Impersonate_Who(void);

extern Bool Impersonate_ForceRoot(void);
extern Bool Impersonate_UnforceRoot(void);

extern Bool Impersonate_Runas(const char *cfg, const char *caller, 
                              AuthToken callerToken);

#ifdef _WIN32
extern Bool Impersonate_CfgRunasOnly(const char *cfg);
#endif

#endif // ifndef _IMPERSONATE_H_

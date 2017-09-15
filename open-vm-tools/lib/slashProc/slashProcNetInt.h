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

/**
 * @file netInt.h
 *
 *	SlashProcNet internal bits.
 */

#ifndef _SLASHPROCNETINT_H_
#define _SLASHPROCNETINT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#ifdef VMX86_DEVEL
EXTERN void SlashProcNetSetPathSnmp(const char *newPathToNetSnmp);
EXTERN void SlashProcNetSetPathSnmp6(const char *newPathToNetSnmp6);
EXTERN void SlashProcNetSetPathRoute(const char *newPathToNetRoute);
EXTERN void SlashProcNetSetPathRoute6(const char *newPathToNetRoute6);
#endif // ifdef VMX86_DEVEL

#endif // ifndef _SLASHPROCNETINT_H_


/*********************************************************
 * Copyright (C) 2003-2016 VMware, Inc. All rights reserved.
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
 * impersonateInt.h --
 *
 *	Header file shared by impersonate code
 */

#ifndef _IMPERSONATE_INT_H_
#define _IMPERSONATE_INT_H_

#include "vmware.h"
#include "msg.h"
#include "impersonate.h"
#include "auth.h"

//#define IMP_VERBOSE 1
#define INVALID_PTHREAD_KEY_VALUE (-1)
#ifdef IMP_VERBOSE
#define IMPWARN(x) Warning x
#else
#define IMPWARN(x)
#endif

typedef struct ImpersonationState {
   const char *impersonatedUser;       // the user we are currently impersonating
   int refCount;                       // # of times we are impersonating as same user

#ifdef _WIN32
   HANDLE impersonatedToken;           // the access token currently impersonated with
   Bool forceRoot;                     // are we temporarily switching back to root?
#endif
} ImpersonationState;

void ImpersonateInit(void);
ImpersonationState *ImpersonateGetTLS(void);
Bool ImpersonateRunas(const char *cfg, const char *caller, AuthToken callerToken);
Bool ImpersonateOwner(const char *file);
Bool ImpersonateDo(const char *user, AuthToken token);
Bool ImpersonateUndo(void);
Bool ImpersonateForceRoot(void);
Bool ImpersonateUnforceRoot(void);

#endif // ImpersonateInt.h
